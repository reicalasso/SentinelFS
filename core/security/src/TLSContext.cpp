#include "TLSContext.h"
#include "Logger.h"
#include "Crypto.h"
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
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

void TLSContext::setPinningPolicy(PinningPolicy policy) {
    pinningPolicy_ = policy;
}

void TLSContext::addPin(const CertificatePin& pin) {
    // Remove existing pin for same hostname
    removePin(pin.hostname);
    pins_.push_back(pin);
}

bool TLSContext::removePin(const std::string& hostname) {
    auto it = std::find_if(pins_.begin(), pins_.end(),
        [&hostname](const CertificatePin& p) {
            return p.hostname == hostname;
        });
    
    if (it != pins_.end()) {
        pins_.erase(it);
        return true;
    }
    return false;
}

bool TLSContext::loadPins(const std::string& path) {
    auto& logger = Logger::instance();
    
    std::ifstream file(path);
    if (!file.is_open()) {
        lastError_ = "Failed to open pins file: " + path;
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    // Simple JSON-like parsing (production should use a proper JSON library)
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // Parse entries in format: hostname:spkiHash:fingerprint:expires
    std::istringstream iss(content);
    std::string line;
    
    pins_.clear();
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream lineStream(line);
        CertificatePin pin;
        
        std::getline(lineStream, pin.hostname, ':');
        std::getline(lineStream, pin.spkiHash, ':');
        std::getline(lineStream, pin.fingerprint, ':');
        
        std::string expires;
        std::getline(lineStream, expires, ':');
        if (!expires.empty()) {
            pin.expiresAt = std::stol(expires);
        }
        
        std::getline(lineStream, pin.comment);
        
        if (!pin.hostname.empty() && (!pin.spkiHash.empty() || !pin.fingerprint.empty())) {
            pins_.push_back(pin);
        }
    }
    
    logger.log(LogLevel::INFO, "Loaded " + std::to_string(pins_.size()) + " certificate pins", "TLSContext");
    return true;
}

bool TLSContext::savePins(const std::string& path) {
    auto& logger = Logger::instance();
    
    std::ofstream file(path);
    if (!file.is_open()) {
        lastError_ = "Failed to create pins file: " + path;
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    file << "# SentinelFS Certificate Pins\n";
    file << "# Format: hostname:spkiHash:fingerprint:expires:comment\n\n";
    
    for (const auto& pin : pins_) {
        file << pin.hostname << ":"
             << pin.spkiHash << ":"
             << pin.fingerprint << ":"
             << pin.expiresAt << ":"
             << pin.comment << "\n";
    }
    
    logger.log(LogLevel::DEBUG, "Saved " + std::to_string(pins_.size()) + " certificate pins", "TLSContext");
    return true;
}

std::vector<TLSContext::CertificatePin> TLSContext::getPins() const {
    return pins_;
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
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, spki.data(), spki.size());
    unsigned int hashLen;
    EVP_DigestFinal_ex(ctx, hash.data(), &hashLen);
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

TLSContext::VerificationResult TLSContext::verifyCertificate(X509* cert, 
                                                              const std::string& hostname) {
    auto& logger = Logger::instance();
    VerificationResult result;
    
    if (!cert) {
        result.errorMessage = "No certificate provided";
        return result;
    }
    
    // Extract certificate information
    result.fingerprint = computeFingerprint(cert);
    result.spkiHash = computeSPKIHash(cert);
    
    // Get subject and issuer
    char* subjectStr = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
    char* issuerStr = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);
    result.subjectName = subjectStr ? subjectStr : "";
    result.issuerName = issuerStr ? issuerStr : "";
    OPENSSL_free(subjectStr);
    OPENSSL_free(issuerStr);
    
    // Verify hostname if enabled
    if (hostnameVerification_ && !hostname.empty()) {
        if (!verifyHostname(cert, hostname)) {
            result.errorMessage = "Hostname verification failed";
            result.errorCode = X509_V_ERR_HOSTNAME_MISMATCH;
            logger.log(LogLevel::WARN, result.errorMessage + " for " + hostname, "TLSContext");
            return result;
        }
    }
    
    // Check certificate pinning
    if (pinningPolicy_ != PinningPolicy::NONE) {
        if (!checkPin(hostname, result.spkiHash, result.fingerprint)) {
            result.errorMessage = "Certificate pin verification failed";
            result.errorCode = -1;
            logger.log(LogLevel::WARN, result.errorMessage + " for " + hostname, "TLSContext");
            return result;
        }
    }
    
    result.valid = true;
    return result;
}

bool TLSContext::verifyHostname(X509* cert, const std::string& hostname) {
    // Check Subject Alternative Names first
    GENERAL_NAMES* names = static_cast<GENERAL_NAMES*>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
    
    if (names) {
        for (int i = 0; i < sk_GENERAL_NAME_num(names); ++i) {
            GENERAL_NAME* name = sk_GENERAL_NAME_value(names, i);
            
            if (name->type == GEN_DNS) {
                const char* dns = reinterpret_cast<const char*>(
                    ASN1_STRING_get0_data(name->d.dNSName));
                int len = ASN1_STRING_length(name->d.dNSName);
                
                std::string dnsName(dns, len);
                if (matchPattern(dnsName, hostname)) {
                    GENERAL_NAMES_free(names);
                    return true;
                }
            }
        }
        GENERAL_NAMES_free(names);
    }
    
    // Fall back to Common Name
    X509_NAME* subject = X509_get_subject_name(cert);
    int idx = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
    
    if (idx >= 0) {
        X509_NAME_ENTRY* entry = X509_NAME_get_entry(subject, idx);
        ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
        
        const char* cn = reinterpret_cast<const char*>(ASN1_STRING_get0_data(data));
        int len = ASN1_STRING_length(data);
        
        std::string commonName(cn, len);
        if (matchPattern(commonName, hostname)) {
            return true;
        }
    }
    
    return false;
}

bool TLSContext::matchPattern(const std::string& pattern, const std::string& hostname) {
    // Exact match
    if (pattern == hostname) return true;
    
    // Wildcard match (only leftmost label)
    if (pattern.length() > 2 && pattern[0] == '*' && pattern[1] == '.') {
        std::string suffix = pattern.substr(1);  // .example.com
        
        size_t dotPos = hostname.find('.');
        if (dotPos != std::string::npos) {
            std::string hostSuffix = hostname.substr(dotPos);
            return hostSuffix == suffix;
        }
    }
    
    return false;
}

bool TLSContext::checkPin(const std::string& hostname, 
                          const std::string& spkiHash,
                          const std::string& fingerprint) {
    auto& logger = Logger::instance();
    time_t now = time(nullptr);
    
    for (const auto& pin : pins_) {
        // Skip expired pins
        if (pin.expiresAt > 0 && pin.expiresAt < now) {
            continue;
        }
        
        // Check hostname match
        if (!matchPattern(pin.hostname, hostname) && pin.hostname != "*") {
            continue;
        }
        
        // Check SPKI hash or fingerprint
        if (!pin.spkiHash.empty() && pin.spkiHash == spkiHash) {
            logger.log(LogLevel::DEBUG, "SPKI pin matched for " + hostname, "TLSContext");
            return true;
        }
        
        if (!pin.fingerprint.empty() && pin.fingerprint == fingerprint) {
            logger.log(LogLevel::DEBUG, "Fingerprint pin matched for " + hostname, "TLSContext");
            return true;
        }
    }
    
    // Handle TOFU policy
    if (pinningPolicy_ == PinningPolicy::TRUST_ON_FIRST_USE) {
        handleTOFU(hostname, spkiHash);
        return true;  // Trust on first use
    }
    
    return (pinningPolicy_ == PinningPolicy::NONE);
}

void TLSContext::handleTOFU(const std::string& hostname, const std::string& spkiHash) {
    auto& logger = Logger::instance();
    
    // Add new pin for first-seen certificate
    CertificatePin pin;
    pin.hostname = hostname;
    pin.spkiHash = spkiHash;
    pin.comment = "Auto-pinned via TOFU";
    
    addPin(pin);
    
    logger.log(LogLevel::INFO, "TOFU: Pinned certificate for " + hostname, "TLSContext");
    
    // Save pins if path is configured
    if (!tofuStorePath_.empty()) {
        savePins(tofuStorePath_);
    }
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

TLSContext::VerificationResult TLSContext::performHandshake(SSL* ssl) {
    auto& logger = Logger::instance();
    VerificationResult result;
    
    // Store context for callback
    currentContext_ = this;
    
    int ret;
    if (mode_ == Mode::CLIENT) {
        ret = SSL_connect(ssl);
    } else {
        ret = SSL_accept(ssl);
    }
    
    currentContext_ = nullptr;
    
    if (ret != 1) {
        int err = SSL_get_error(ssl, ret);
        result.errorCode = err;
        
        switch (err) {
            case SSL_ERROR_ZERO_RETURN:
                result.errorMessage = "TLS connection closed";
                break;
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                result.errorMessage = "TLS handshake incomplete (non-blocking)";
                break;
            case SSL_ERROR_SYSCALL:
                result.errorMessage = "TLS syscall error: " + std::string(strerror(errno));
                break;
            case SSL_ERROR_SSL:
                result.errorMessage = "TLS error: " + getOpenSSLError();
                break;
            default:
                result.errorMessage = "Unknown TLS error: " + std::to_string(err);
        }
        
        logger.log(LogLevel::ERROR, result.errorMessage, "TLSContext");
        return result;
    }
    
    // Get peer certificate
    X509* peerCert = SSL_get_peer_certificate(ssl);
    if (!peerCert && mode_ == Mode::CLIENT) {
        result.errorMessage = "No peer certificate received";
        logger.log(LogLevel::ERROR, result.errorMessage, "TLSContext");
        return result;
    }
    
    if (peerCert) {
        result = verifyCertificate(peerCert, currentHostname_);
        X509_free(peerCert);
    }
    
    // Check verification result
    long verifyResult = SSL_get_verify_result(ssl);
    if (verifyResult != X509_V_OK) {
        result.errorMessage = X509_verify_cert_error_string(verifyResult);
        result.errorCode = static_cast<int>(verifyResult);
        logger.log(LogLevel::WARN, "Certificate verification: " + result.errorMessage, "TLSContext");
        // Note: We may still proceed if pinning succeeded
    }
    
    result.valid = true;
    logger.log(LogLevel::INFO, "TLS handshake successful", "TLSContext");
    
    return result;
}

int TLSContext::verifyCallbackWrapper(int preverify_ok, X509_STORE_CTX* ctx) {
    // Get current TLSContext
    if (!currentContext_) {
        return preverify_ok;
    }
    
    // Get the certificate being verified
    X509* cert = X509_STORE_CTX_get_current_cert(ctx);
    int depth = X509_STORE_CTX_get_error_depth(ctx);
    int err = X509_STORE_CTX_get_error(ctx);
    
    auto& logger = Logger::instance();
    
    // Only verify end-entity certificate (depth 0) for pinning
    if (depth == 0 && currentContext_->pinningPolicy_ != PinningPolicy::NONE) {
        auto result = currentContext_->verifyCertificate(cert, currentHostname_);
        
        if (currentContext_->verifyCallback_) {
            if (!currentContext_->verifyCallback_(result)) {
                X509_STORE_CTX_set_error(ctx, X509_V_ERR_APPLICATION_VERIFICATION);
                return 0;
            }
        }
        
        if (!result.valid && currentContext_->pinningPolicy_ == PinningPolicy::STRICT_PIN) {
            logger.log(LogLevel::ERROR, "Strict pin verification failed", "TLSContext");
            X509_STORE_CTX_set_error(ctx, X509_V_ERR_APPLICATION_VERIFICATION);
            return 0;
        }
    }
    
    if (!preverify_ok) {
        logger.log(LogLevel::DEBUG, 
            "Certificate verify failed at depth " + std::to_string(depth) + 
            ": " + X509_verify_cert_error_string(err), "TLSContext");
    }
    
    return preverify_ok;
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

// TLSConnection implementation
TLSConnection::TLSConnection(SSL* ssl, int socket)
    : ssl_(ssl), socket_(socket) {}

TLSConnection::~TLSConnection() {
    if (ssl_) {
        shutdown();
        SSL_free(ssl_);
    }
}

TLSConnection::TLSConnection(TLSConnection&& other) noexcept
    : ssl_(other.ssl_), socket_(other.socket_) {
    other.ssl_ = nullptr;
    other.socket_ = -1;
}

TLSConnection& TLSConnection::operator=(TLSConnection&& other) noexcept {
    if (this != &other) {
        if (ssl_) {
            shutdown();
            SSL_free(ssl_);
        }
        ssl_ = other.ssl_;
        socket_ = other.socket_;
        other.ssl_ = nullptr;
        other.socket_ = -1;
    }
    return *this;
}

ssize_t TLSConnection::read(void* buffer, size_t maxSize) {
    if (!ssl_) return -1;
    
    int ret = SSL_read(ssl_, buffer, static_cast<int>(maxSize));
    if (ret <= 0) {
        int err = SSL_get_error(ssl_, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0;  // Would block
        }
        return -1;  // Error
    }
    return ret;
}

ssize_t TLSConnection::write(const void* data, size_t size) {
    if (!ssl_) return -1;
    
    int ret = SSL_write(ssl_, data, static_cast<int>(size));
    if (ret <= 0) {
        int err = SSL_get_error(ssl_, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return 0;  // Would block
        }
        return -1;  // Error
    }
    return ret;
}

void TLSConnection::shutdown() {
    if (ssl_) {
        SSL_shutdown(ssl_);
    }
}

X509* TLSConnection::getPeerCertificate() const {
    if (!ssl_) return nullptr;
    return SSL_get_peer_certificate(ssl_);
}

std::string TLSConnection::getProtocolVersion() const {
    if (!ssl_) return "";
    return SSL_get_version(ssl_);
}

std::string TLSConnection::getCipherSuite() const {
    if (!ssl_) return "";
    const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_);
    return cipher ? SSL_CIPHER_get_name(cipher) : "";
}

} // namespace SentinelFS
