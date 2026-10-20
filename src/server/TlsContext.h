#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>
#include <mutex>
#include <string>
#include "Config.h"

class TlsContext {
public:
    static TlsContext& getInstance() {
        static TlsContext instance;
        return instance;
    }

    bool init(const std::string& certPath, const std::string& keyPath,
              Config::TlsVersion minVersion, std::string& errorMsg);
    bool reload(std::string& errorMsg);
    std::shared_ptr<asio::ssl::context> get() const;
    bool isReady() const;

private:
    TlsContext() = default;

    mutable std::mutex m_mutex;
    std::shared_ptr<asio::ssl::context> m_context;
    std::string m_certPath;
    std::string m_keyPath;
    Config::TlsVersion m_minVersion{Config::TlsVersion::TLS_1_2};
    bool m_ready{false};
};