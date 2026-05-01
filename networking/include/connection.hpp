// Handles one TCP connection - receives messages of format: <4-byte length><message>
// All callbacks are serialised through a strand so the class is safe to use
// from multiple Boost.Asio threads (fixes data-race on server_connections).

#pragma once
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include "types.hpp"
#include <deque>

namespace netwatch::networking {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    explicit Connection(tcp::socket socket);
    void start();  // begin async read loop
    void send(const std::string& message);
    void setMessageHandler(MessageHandler handler); 
    void setDisconnectHandler(DisconnectHandler handler);
    bool isOpen() const; // check if connection is still open

private: // internal async handlers
    void doWrite();
    void readHeader();
    void readBody(std::size_t length);
    void handleError(const boost::system::error_code& ec);

private: // member variables
    // Strand serialises all async callbacks — no mutex needed for the queue.
    // Uses any_io_executor to match tcp::socket's default executor in Boost 1.89+
    boost::asio::strand<boost::asio::any_io_executor> net_strand;
    tcp::socket net_socket;
    std::array<char, 4> net_header_buffer;
    std::vector<char> net_body_buffer;

    std::deque<std::shared_ptr<std::vector<char>>> net_write_queue;
    bool net_writing;

    // callbacks
    MessageHandler net_message_handler;
    DisconnectHandler net_disconnect_handler;
};

} // namespace netwatch::networking