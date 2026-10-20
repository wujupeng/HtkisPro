#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <asio/ssl.hpp>

using TlsStream = asio::ssl::stream<asio::ip::tcp::socket>;