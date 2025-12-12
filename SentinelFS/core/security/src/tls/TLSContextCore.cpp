/**
 * @file TLSContextCore.cpp
 * @brief TLSContext core functionality - initialization, certificates, settings
 */

#include "TLSContext.h"
#include "Logger.h"
#include "Crypto.h"
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <sys/stat.h>

namespace SentinelFS {

// Thread-local storage definitions
thread_local TLSContext* TLSContext::currentContext_ = nullptr;
thread_local std::string TLSContext::currentHostname_;

namespace {
    // Secure cipher suites (TLS 1.3 + strong TLS 1.2 ciphers)
    const char* DEFAULT_CIPHERS = 
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256:"
        "TLS_AES_128_GCM_SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256";
    
    std::string getOpenSSLError() {
        unsigned long err = ERR_get_error();
        if (err == 0) return "Unknown error";
        
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        return std::string(buf);
    }
    
    // Base64 encode for SPKI hash
    std::string base64Encode(const std::vector<uint8_t>& data) {
        BIO* bio = BIO_new(BIO_s_mem());
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_push(b64, bio);
        
        BIO_write(bio, data.data(), static_cast<int>(data.size()));
        BIO_flush(bio);
        
        BUF_MEM* bufPtr;
        BIO_get_mem_ptr(bio, &bufPtr);
        
        std::string result(bufPtr->data, bufPtr->length);
        BIO_free_all(bio);
        
        return result;
    }
}

TLSContext::TLSContext(Mode mode) : mode_(mode) {}

TLSContext::~TLSContext() {
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

TLSContext::TLSContext(TLSContext&& other) noexcept
    : mode_(other.mode_)
    , ctx_(other.ctx_)
    , pinningPolicy_(other.pinningPolicy_)
    , pins_(std::move(other.pins_))
    , verifyCallback_(std::move(other.verifyCallback_))
    , hostnameVerification_(other.hostnameVerification_)
    , lastError_(std::move(other.lastError_))
{
    other.ctx_ = nullptr;
}

TLSContext& TLSContext::operator=(TLSContext&& other) noexcept {
    if (this != &other) {
        if (ctx_) SSL_CTX_free(ctx_);
        
        mode_ = other.mode_;
        ctx_ = other.ctx_;
        pinningPolicy_ = other.pinningPolicy_;
        pins_ = std::move(other.pins_);
        verifyCallback_ = std::move(other.verifyCallback_);
        hostnameVerification_ = other.hostnameVerification_;
        lastError_ = std::move(other.lastError_);
        
        other.ctx_ = nullptr;
    }
    return *this;
}

bool TLSContext::initialize() {
    auto& logger = Logger::instance();
    
    // Create appropriate context based on mode
    const SSL_METHOD* method = (mode_ == Mode::SERVER) 
        ? TLS_server_method() 
        : TLS_client_method();
    
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) {
        lastError_ = "Failed to create SSL context: " + getOpenSSLError();
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    // Set minimum TLS version to 1.2 (no SSLv3, TLS 1.0, TLS 1.1)
    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
    
    // Prefer TLS 1.3
    SSL_CTX_set_max_proto_version(ctx_, TLS1_3_VERSION);
    
    // Set secure cipher suites
    if (SSL_CTX_set_cipher_list(ctx_, DEFAULT_CIPHERS) != 1) {
        logger.log(LogLevel::WARN, "Failed to set cipher list, using defaults", "TLSContext");
    }
    
    // Set options for security
    SSL_CTX_set_options(ctx_, 
        SSL_OP_NO_SSLv2 |           // Disable SSLv2
        SSL_OP_NO_SSLv3 |           // Disable SSLv3
        SSL_OP_NO_TLSv1 |           // Disable TLS 1.0
        SSL_OP_NO_TLSv1_1 |         // Disable TLS 1.1
        SSL_OP_NO_COMPRESSION |     // Disable compression (CRIME)
        SSL_OP_SINGLE_DH_USE |      // Generate new DH key for each handshake
        SSL_OP_SINGLE_ECDH_USE |    // Generate new ECDH key for each handshake
        SSL_OP_CIPHER_SERVER_PREFERENCE  // Server chooses cipher
    );
    
    // Enable session tickets (for performance)
    SSL_CTX_set_options(ctx_, SSL_OP_NO_TICKET);  // Disable for better forward secrecy
    
    // Set verification mode
    if (mode_ == Mode::CLIENT) {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, verifyCallbackWrapper);
    } else {
        // Server mode - optional client certificate verification
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
    }
    
    logger.log(LogLevel::DEBUG, "TLS context initialized successfully", "TLSContext");
    return true;
}

bool TLSContext::loadCertificate(const std::string& certPath,
                                  const std::string& keyPath,
                                  const std::string& keyPassword) {
    auto& logger = Logger::instance();
    
    if (!ctx_) {
        lastError_ = "TLS context not initialized";
        return false;
    }
    
    // Set password callback if provided
    if (!keyPassword.empty()) {
        SSL_CTX_set_default_passwd_cb_userdata(ctx_, 
            const_cast<char*>(keyPassword.c_str()));
    }
    
    // Load certificate
    if (SSL_CTX_use_certificate_file(ctx_, certPath.c_str(), SSL_FILETYPE_PEM) != 1) {
        lastError_ = "Failed to load certificate: " + getOpenSSLError();
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx_, keyPath.c_str(), SSL_FILETYPE_PEM) != 1) {
        lastError_ = "Failed to load private key: " + getOpenSSLError();
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    // Verify key matches certificate
    if (SSL_CTX_check_private_key(ctx_) != 1) {
        lastError_ = "Private key does not match certificate";
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    logger.log(LogLevel::INFO, "Loaded TLS certificate: " + certPath, "TLSContext");
    return true;
}

bool TLSContext::loadCACertificates(const std::string& caPath) {
    auto& logger = Logger::instance();
    
    if (!ctx_) {
        lastError_ = "TLS context not initialized";
        return false;
    }
    
    struct stat st;
    if (stat(caPath.c_str(), &st) != 0) {
        lastError_ = "CA path does not exist: " + caPath;
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    int result;
    if (S_ISDIR(st.st_mode)) {
        result = SSL_CTX_load_verify_locations(ctx_, nullptr, caPath.c_str());
    } else {
        result = SSL_CTX_load_verify_locations(ctx_, caPath.c_str(), nullptr);
    }
    
    if (result != 1) {
        lastError_ = "Failed to load CA certificates: " + getOpenSSLError();
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    logger.log(LogLevel::INFO, "Loaded CA certificates from: " + caPath, "TLSContext");
    return true;
}

bool TLSContext::useSystemCertificates() {
    auto& logger = Logger::instance();
    
    if (!ctx_) {
        lastError_ = "TLS context not initialized";
        return false;
    }
    
    if (SSL_CTX_set_default_verify_paths(ctx_) != 1) {
        lastError_ = "Failed to load system certificates: " + getOpenSSLError();
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    logger.log(LogLevel::DEBUG, "Using system certificate store", "TLSContext");
    return true;
}

void TLSContext::setVerifyCallback(VerifyCallback callback) {
    verifyCallback_ = std::move(callback);
}

void TLSContext::setMinTLSVersion(int version) {
    if (ctx_) {
        SSL_CTX_set_min_proto_version(ctx_, version);
    }
}

void TLSContext::setCipherSuites(const std::string& ciphers) {
    if (ctx_) {
        SSL_CTX_set_cipher_list(ctx_, ciphers.c_str());
    }
}

void TLSContext::setHostnameVerification(bool enable) {
    hostnameVerification_ = enable;
}

SSL* TLSContext::wrapSocket(int socket, const std::string& hostname) {
    auto& logger = Logger::instance();
    
    if (!ctx_) {
        lastError_ = "TLS context not initialized";
        return nullptr;
    }
    
    SSL* ssl = SSL_new(ctx_);
    if (!ssl) {
        lastError_ = "Failed to create SSL object: " + getOpenSSLError();
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return nullptr;
    }
    
    SSL_set_fd(ssl, socket);
    
    // Set SNI hostname for client mode
    if (mode_ == Mode::CLIENT && !hostname.empty()) {
        SSL_set_tlsext_host_name(ssl, hostname.c_str());
        
        // Enable hostname verification (OpenSSL 1.1.0+)
        SSL_set1_host(ssl, hostname.c_str());
    }
    
    return ssl;
}

std::string TLSContext::computeSPKIHash(X509* cert) {
    if (!cert) return "";
    
    // Extract SubjectPublicKeyInfo
    int len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), nullptr);
    if (len <= 0) return "";
    
    std::vector<uint8_t> spki(len);
    uint8_t* ptr = spki.data();
    i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), &ptr);
    
    // Compute SHA256 hash
    std::vector<uint8_t> hash(EVP_MD_size(EVP_sha256()));
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, spki.data(), spki.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    unsigned int hashLen;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hashLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    EVP_MD_CTX_free(ctx);
    
    return base64Encode(hash);
}

std::string TLSContext::computeFingerprint(X509* cert) {
    if (!cert) return "";
    
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len;
    
    if (!X509_digest(cert, EVP_sha256(), md, &len)) {
        return "";
    }
    
    return Crypto::toHex(std::vector<uint8_t>(md, md + len));
}

} // namespace SentinelFS
