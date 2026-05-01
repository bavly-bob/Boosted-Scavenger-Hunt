// connection implementation - handles reading and writing messages to a client
// All async operations are dispatched through net_strand to ensure that
// net_write_queue and net_disconnect_handler are never accessed concurrently
// from multiple I/O threads (fixes data-race).

#include "connection.hpp"
#include <iostream>
#include <deque>
#include <cstring>

namespace netwatch::networking 
{
namespace // anonymous namespace for internal constants without polluting the global namespace
{
    constexpr std::size_t HEADER_SIZE = 4;
    constexpr std::size_t MAX_MESSAGE_SIZE = 10 * 1024 * 1024; // 10 MB
}

// Constructor — initialise strand from the socket's executor so all async
// operations on this connection share a single logical thread.
Connection::Connection(tcp::socket socket)
    : net_strand(socket.get_executor()),
      net_socket(std::move(socket)),
      net_writing(false)
{}

// Start reading loop
void Connection::start() 
{
    readHeader(); // start reading by the header to make sure we don't read more than we need to 
}

// ===================== SEND (WITH QUEUE) =====================

void Connection::send(const std::string& message) 
{
    if (!isOpen()) return;

    uint32_t len_net = htonl(static_cast<uint32_t>(message.size()));

    auto buffer = std::make_shared<std::vector<char>>(HEADER_SIZE + message.size());

    /*
    std::memcpy is used deliberately here for low-level binary framing.
    The buffer is a [4-byte-length][body] frame; std::memcpy is the most
    direct, safe, and predictable way to write fixed-size binary headers.
    The buffer is owned by a shared_ptr so it survives the async_write.
    */
    std::memcpy(buffer->data(), &len_net, HEADER_SIZE);
    std::memcpy(buffer->data() + HEADER_SIZE, message.data(), message.size());

    // Post the enqueue+write through the strand — guarantees the queue is
    // never touched from two threads at once.
    boost::asio::post(net_strand, [self = shared_from_this(), buffer]() {
        self->net_write_queue.push_back(buffer);
        if (!self->net_writing)
            self->doWrite();
    });
}

void Connection::doWrite() 
{
    // Called only from within the strand — no mutex needed.
    if (net_write_queue.empty()) 
    {
        net_writing = false;
        return;
    }

    net_writing = true;
    auto buffer = net_write_queue.front();

    boost::asio::async_write(net_socket, boost::asio::buffer(*buffer),
        boost::asio::bind_executor(net_strand,
            [self = shared_from_this(), buffer]
            (const boost::system::error_code& ec, std::size_t)
            {
                if (!ec) 
                {
                    self->net_write_queue.pop_front();
                    self->doWrite();
                } 
                else
                {
                    self->handleError(ec);
                }
            }));
}

// ===================== READ =====================

void Connection::readHeader()
{
    boost::asio::async_read(net_socket,
        boost::asio::buffer(net_header_buffer),
        boost::asio::bind_executor(net_strand,
            [self = shared_from_this()]
            (const boost::system::error_code& ec, std::size_t)
            {
                if (ec) {
                    self->handleError(ec);
                    return;
                }

                uint32_t bodyLength = 0;
                std::memcpy(&bodyLength, self->net_header_buffer.data(), HEADER_SIZE);
                bodyLength = ntohl(bodyLength);

                if (bodyLength == 0) {
                    // Zero-length frame: heartbeat / ignore
                    self->readHeader();
                    return;
                }
                if (bodyLength > MAX_MESSAGE_SIZE) {
                    self->handleError(boost::asio::error::message_size);
                    return;
                }

                self->readBody(bodyLength);
            }));
}

void Connection::readBody(std::size_t length)
{
    net_body_buffer.resize(length);

    boost::asio::async_read(net_socket,
        boost::asio::buffer(net_body_buffer),
        boost::asio::bind_executor(net_strand,
            [self = shared_from_this()]
            (const boost::system::error_code& ec, std::size_t)
            {
                if (ec) {
                    self->handleError(ec);
                    return;
                }

                if (self->net_message_handler) {
                    std::string msg(self->net_body_buffer.data(),
                                    self->net_body_buffer.size());
                    self->net_message_handler(msg);
                }

                self->readHeader(); // continue loop
            }));
}

// ===================== HANDLERS =====================

void Connection::setMessageHandler(MessageHandler handler) 
{
    net_message_handler = std::move(handler);
}

void Connection::setDisconnectHandler(DisconnectHandler handler) 
{
    net_disconnect_handler = std::move(handler);
}

// ===================== STATE =====================

bool Connection::isOpen() const 
{
    return net_socket.is_open();
}

// ===================== ERROR / CLOSE =====================

void Connection::handleError(const boost::system::error_code& ec) 
{
    // Called from within the strand — safe to access all members.
    if (net_socket.is_open()) 
    {
        boost::system::error_code ignored;
        net_socket.shutdown(tcp::socket::shutdown_both, ignored);
        net_socket.close(ignored);
    }

    net_write_queue.clear();
    net_writing = false;

    if (net_disconnect_handler) 
    {
        auto handler = std::move(net_disconnect_handler);
        net_disconnect_handler = nullptr;
        handler(); // fire once, then null
    }
}

} // namespace netwatch::networking