#include "TlsContext.h"
#include "Logger.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

bool TlsContext::init(const std::string& certPath, const std::string& keyPath,
                      Config::TlsVersion minVersion, std::string& errorMsg) {
    if (certPath.empty() || keyPath.empty()) {
        errorMsg = "Certificate or key path is empty";
        return false;
    }

    try {
        auto ctx = std::make_shared<asio::ssl::context>(asio::ssl::context::tls);

        asio::ssl::context::options opts = asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3;
        if (minVersion >= Config::TlsVersion::TLS_1_0) opts |= asio::ssl::context::no_tlsv1;
        if (minVersion >= Config::TlsVersion::TLS_1_1) opts |= asio::ssl::context::no_tlsv1_1;
        if (minVersion >= Config::TlsVersion::TLS_1_2) opts |= asio::ssl::context::no_tlsv1_2;
        ctx->set_options(opts);

        ctx->use_certificate_chain_file(certPath);
        ctx->use_private_key_file(keyPath, asio::ssl::context::pem);

        SSL_CTX* nativeCtx = ctx->native_handle();
        if (!SSL_CTX_check_private_key(nativeCtx)) {
            errorMsg = "Private key does not match certificate";
            return false;
        }

        X509* cert = SSL_CTX_get0_certificate(nativeCtx);
        if (cert) {
            ASN1_TIME* notAfter = X509_get_notAfter(cert);
            if (notAfter) {
                int dayDiff = 0, secDiff = 0;
                ASN1_TIME_diff(&dayDiff, &secDiff, nullptr, notAfter);
                if (dayDiff < 0) {
                    errorMsg = "Certificate has expired";
                    return false;
                }
            }
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_context = ctx;
        m_certPath = certPath;
        m_keyPath = keyPath;
        m_minVersion = minVersion;
        m_ready = true;
        Logger::getInstance().info("[TLS] Context initialized: cert=" + certPath + " minVersion=" +
            Config::tlsVersionToString(minVersion));
        return true;
    } catch (const std::exception& e) {
        errorMsg = std::string("TLS init failed: ") + e.what();
        Logger::getInstance().error("[TLS] " + errorMsg);
        return false;
    }
}

bool TlsContext::reload(std::string& errorMsg) {
    std::string certPath, keyPath;
    Config::TlsVersion minVersion;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        certPath = m_certPath;
        keyPath = m_keyPath;
        minVersion = m_minVersion;
    }
    return init(certPath, keyPath, minVersion, errorMsg);
}

std::shared_ptr<asio::ssl::context> TlsContext::get() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_context;
}

bool TlsContext::isReady() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ready;
}