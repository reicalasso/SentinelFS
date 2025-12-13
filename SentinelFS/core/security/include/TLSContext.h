#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <openssl/ssl.h>
#include <openssl/x509.h>

namespace SentinelFS {

/**
 * @brief TLS configuration and certificate management
 * 
 * Provides:
 * - TLS context creation for client/server connections
 * - Certificate validation with optional pinning
 * - Certificate chain verification
 * - Custom certificate store management
 */
class TLSContext {
public:
    /**
     * @brief Certificate pinning policy
     */
    enum class PinningPolicy {
        NONE,               // No pinning - use system CA store
        TRUST_ON_FIRST_USE, // Pin certificate on first connection (TOFU)
        STRICT_PIN,         // Require pre-configured pin
        SPKI_PIN            // Pin Subject Public Key Info (recommended)
    };
    
    /**
     * @brief Verification result
     */
    struct VerificationResult {
        bool valid{false};
        std::string errorMessage;
        std::string subjectName;
        std::string issuerName;
        std::string fingerprint;       // SHA256 fingerprint of certificate
        std::string spkiHash;          // SHA256 hash of SubjectPublicKeyInfo
        int errorCode{0};
    };
    
    /**
     * @brief Certificate pin entry
     */
    struct CertificatePin {
        std::string hostname;          // Hostname or wildcard pattern
        std::string spkiHash;          // Base64 SHA256 of SPKI
        std::string fingerprint;       // SHA256 fingerprint (hex)
        std::string comment;           // Optional description
        time_t expiresAt{0};           // Pin expiration (0 = never)
    };
    
    /**
     * @brief TLS connection mode
     */
    enum class Mode {
        CLIENT,
        SERVER
    };
    
    /**
     * @brief Constructor
     * @param mode Client or server mode
     */
    explicit TLSContext(Mode mode);
    
    ~TLSContext();
    
    // Disable copy
    TLSContext(const TLSContext&) = delete;
    TLSContext& operator=(const TLSContext&) = delete;
    
    // Enable move
    TLSContext(TLSContext&&) noexcept;
    TLSContext& operator=(TLSContext&&) noexcept;
    
    /**
     * @brief Initialize TLS context
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Load certificate and private key for server mode
     * @param certPath Path to PEM certificate file
     * @param keyPath Path to PEM private key file
     * @param keyPassword Optional password for encrypted key
     * @return true on success
     */
    bool loadCertificate(const std::string& certPath,
                         const std::string& keyPath,
                         const std::string& keyPassword = "");
    
    /**
     * @brief Load certificate chain (CA certificates)
     * @param caPath Path to CA certificate file or directory
     * @return true on success
     */
    bool loadCACertificates(const std::string& caPath);
    
    /**
     * @brief Use system default CA store
     * @return true on success
     */
    bool useSystemCertificates();
    
    /**
     * @brief Set certificate pinning policy
     * @param policy Pinning policy to use
     */
    void setPinningPolicy(PinningPolicy policy);
    
    /**
     * @brief Add a certificate pin
     * @param pin Pin entry to add
     */
    void addPin(const CertificatePin& pin);
    
    /**
     * @brief Remove a certificate pin
     * @param hostname Hostname to remove pin for
     * @return true if pin was removed
     */
    bool removePin(const std::string& hostname);
    
    /**
     * @brief Load pins from file
     * @param path Path to pins file (JSON format)
     * @return true on success
     */
    bool loadPins(const std::string& path);
    
    /**
     * @brief Save pins to file
     * @param path Path to save pins file
     * @return true on success
     */
    bool savePins(const std::string& path);
    
    /**
     * @brief Get all configured pins
     * @return Vector of certificate pins
     */
    std::vector<CertificatePin> getPins() const;
    
    /**
     * @brief Verify a certificate against current policy
     * @param cert X509 certificate to verify
     * @param hostname Expected hostname (for CN/SAN validation)
     * @return Verification result
     */
    VerificationResult verifyCertificate(X509* cert, const std::string& hostname);
    
    /**
     * @brief Compute SPKI hash from certificate
     * @param cert X509 certificate
     * @return Base64 encoded SHA256 hash of SPKI
     */
    static std::string computeSPKIHash(X509* cert);
    
    /**
     * @brief Compute certificate fingerprint
     * @param cert X509 certificate
     * @return Hex encoded SHA256 fingerprint
     */
    static std::string computeFingerprint(X509* cert);
    
    /**
     * @brief Wrap a socket with TLS
     * @param socket Raw socket file descriptor
     * @param hostname Hostname for SNI (client mode)
     * @return SSL pointer on success, nullptr on failure
     */
    SSL* wrapSocket(int socket, const std::string& hostname = "");
    
    /**
     * @brief Perform TLS handshake
     * @param ssl SSL connection
     * @return Verification result after handshake
     */
    VerificationResult performHandshake(SSL* ssl);
    
    /**
     * @brief Set custom verification callback
     * @param callback Called during certificate verification
     */
    using VerifyCallback = std::function<bool(const VerificationResult&)>;
    void setVerifyCallback(VerifyCallback callback);
    
    /**
     * @brief Set minimum TLS version
     * @param version Minimum version (TLS1_2_VERSION, TLS1_3_VERSION, etc.)
     */
    void setMinTLSVersion(int version);
    
    /**
     * @brief Set cipher suites
     * @param ciphers Colon-separated cipher list
     */
    void setCipherSuites(const std::string& ciphers);
    
    /**
     * @brief Enable/disable hostname verification
     * @param enable true to enable
     */
    void setHostnameVerification(bool enable);
    
    /**
     * @brief Get the underlying SSL_CTX
     * @return SSL_CTX pointer (for advanced use)
     */
    SSL_CTX* getContext() const { return ctx_; }
    
    /**
     * @brief Get last error message
     */
    std::string getLastError() const { return lastError_; }
    
    /**
     * @brief Rotate certificate pin with backup
     * @param hostname Target hostname
     * @param oldSpkiHash Current pin hash
     * @param newSpkiHash New pin hash
     * @param validityDays Backup validity period
     * @return true on success
     */
    bool rotatePin(const std::string& hostname, const std::string& oldSpkiHash,
                   const std::string& newSpkiHash, int validityDays = 30);
    
    /**
     * @brief Verify certificate with backup pins
     * @param hostname Target hostname
     * @param spkiHash Certificate SPKI hash
     * @param fingerprint Certificate fingerprint
     * @return true if any pin matches
     */
    bool verifyWithBackup(const std::string& hostname, const std::string& spkiHash,
                         const std::string& fingerprint);
    
    /**
     * @brief Get pins expiring soon
     * @param daysThreshold Days until expiration
     * @return List of expiring pins
     */
    std::vector<CertificatePin> getExpiringPins(int daysThreshold = 7);
    
    /**
     * @brief Remove expired pins
     * @return true if any pins were removed
     */
    bool cleanupExpiredPins();

private:
    bool verifyHostname(X509* cert, const std::string& hostname);
    bool matchPattern(const std::string& pattern, const std::string& hostname);
    bool checkPin(const std::string& hostname, const std::string& spkiHash, 
                  const std::string& fingerprint);
    void handleTOFU(const std::string& hostname, const std::string& spkiHash);
    
    static int verifyCallbackWrapper(int preverify_ok, X509_STORE_CTX* ctx);
    
    Mode mode_;
    SSL_CTX* ctx_{nullptr};
    PinningPolicy pinningPolicy_{PinningPolicy::NONE};
    std::vector<CertificatePin> pins_;
    VerifyCallback verifyCallback_;
    bool hostnameVerification_{true};
    std::string lastError_;
    std::string tofuStorePath_;
    
    // Thread-local storage for verification callback context
    static thread_local TLSContext* currentContext_;
    static thread_local std::string currentHostname_;
};

/**
 * @brief RAII wrapper for SSL connection
 */
class TLSConnection {
public:
    TLSConnection(SSL* ssl, int socket);
    ~TLSConnection();
    
    // Disable copy
    TLSConnection(const TLSConnection&) = delete;
    TLSConnection& operator=(const TLSConnection&) = delete;
    
    // Enable move
    TLSConnection(TLSConnection&&) noexcept;
    TLSConnection& operator=(TLSConnection&&) noexcept;
    
    /**
     * @brief Read data from TLS connection
     * @param buffer Output buffer
     * @param maxSize Maximum bytes to read
     * @return Bytes read, or -1 on error
     */
    ssize_t read(void* buffer, size_t maxSize);
    
    /**
     * @brief Write data to TLS connection
     * @param data Data to write
     * @param size Number of bytes
     * @return Bytes written, or -1 on error
     */
    ssize_t write(const void* data, size_t size);
    
    /**
     * @brief Graceful TLS shutdown
     */
    void shutdown();
    
    /**
     * @brief Check if connection is valid
     */
    bool isValid() const { return ssl_ != nullptr; }
    
    /**
     * @brief Get peer certificate
     */
    X509* getPeerCertificate() const;
    
    /**
     * @brief Get negotiated protocol version
     */
    std::string getProtocolVersion() const;
    
    /**
     * @brief Get negotiated cipher suite
     */
    std::string getCipherSuite() const;

private:
    SSL* ssl_{nullptr};
    int socket_{-1};
};

} // namespace SentinelFS
