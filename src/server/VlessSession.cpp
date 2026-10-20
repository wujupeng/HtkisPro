#include "VlessSession.h"
#include "VlessHeaderParser.h"
#include "Logger.h"
#include "Authenticator.h"
#include <cstring>

using asio::ip::tcp;

VlessSession::VlessSession(tcp::socket socket, const std::vector<std::string>& uuids)
    : CryptoSessionBase(std::move(socket)),
      m_uuids(uuids) {
    m_protocol_name = "VLESS";
    m_protocol = ProtocolType::VLESS;
}

void VlessSession::onHandshake() {
    doReadHeader();
}

void VlessSession::doReadHeader() {
    auto self = std::static_pointer_cast<VlessSession>(shared_from_this());

    m_recv_buf.resize(256);
    m_client_socket.async_read_some(asio::buffer(m_recv_buf),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                auto header = VlessHeaderParser::parse(m_recv_buf.data(), length);
                if (!header) {
                    Logger::getInstance().warn("[VLESS] Header parse failed from " + m_client_addr);
                    closeSession(SessionEndReason::Error);
                    return;
                }

                std::string uuidStr(header->uuid.begin(), header->uuid.end());
                bool uuidValid = false;
                for (const auto& allowedUuid : m_uuids) {
                    if (allowedUuid == uuidStr) {
                        uuidValid = true;
                        break;
                    }
                }
                if (!uuidValid) {
                    Logger::getInstance().warn("[VLESS] UUID not authorized from " + m_client_addr);
                    closeSession(SessionEndReason::AuthFailed);
                    return;
                }

                if (header->command != 0x01) {
                    Logger::getInstance().warn("[VLESS] Unsupported command " + std::to_string(header->command) + " from " + m_client_addr);
                    closeSession(SessionEndReason::Error);
                    return;
                }

                m_target_host = header->host;
                m_target_port = header->port;

                Logger::getInstance().info("[VLESS] CONNECT " + m_target_host + ":" + std::to_string(m_target_port) + " from=" + m_client_addr);
                doResolve(m_target_host, m_target_port);
            } else {
                closeSession(SessionEndReason::ClientDisconnect);
            }
        });
}