/**
 * @file Verification.cpp
 * @brief TLSContext certificate verification, handshake, and TLSConnection implementation
 */

#include "TLSContext.h"
#include "Logger.h"
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <cstring>

namespace SentinelFS {

namespace {
    std::string getOpenSSLError() {
        unsigned long err = ERR_get_error();
        if (err == 0) return "Unknown error";
        
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        return std::string(buf);
    }
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
