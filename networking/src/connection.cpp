// connection implementation - handles reading and writing messages to a client

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

// Constructor
Connection::Connection(tcp::socket socket)
    : net_socket(std::move(socket)), net_writing(false) {} // sockets can't be copied safely so we move them 

// Start reading loop
void Connection::start() 
{
    readHeader(); // start reading by the header to make sure we don't read more than we need to 
}

// ===================== SEND (WITH QUEUE) =====================

void Connection::send(const std::string& message) 
{
    if (!isOpen()) return;

    uint32_t len_net = htonl(static_cast<uint32_t>(message.size())); // convert to network byte order to ensure work cross-platform

    Ptr<std::vector<char>> buffer = std::make_shared<std::vector<char>>(HEADER_SIZE + message.size());


    /* 
    We use std::memcpy here intentionally for low-level binary operations.
    Reasoning:
        - This is a network protocol buffer ([header][body]), so we are working with raw bytes.
        - std::memcpy is the most direct and reliable way to copy fixed-size binary data (like the 4-byte length header).
        - It avoids unnecessary abstractions when dealing with byte-level memory.
        - Using std::span/std::ranges would still require reinterpret_cast for the header,
          so it would not actually improve safety, only add complexity.
        - In async networking, keeping this simple and predictable is more important than stylistic consistency.
    
    Note:
        - The buffer is owned by a shared_ptr to ensure it stays alive during async_write.
        - This is a controlled and safe use of "C-style" memory operations in a performance-critical path.
    */
    std::memcpy(buffer->data(), &len_net, HEADER_SIZE); // copy 4-byte length header
    std::memcpy(buffer->data() + HEADER_SIZE, message.data(), message.size()); // copy message body

    // Queue the message
    net_write_queue.push_back(buffer);

    // If already writing, do nothing
    if (net_writing) return;

    doWrite();
}

void Connection::doWrite() 
{
    if (net_write_queue.empty()) 
    {
        net_writing = false;
        return;
    }

    net_writing = true;

    auto buffer = net_write_queue.front();

    boost::asio::async_write(net_socket, boost::asio::buffer(*buffer),
        [self = shared_from_this(), buffer](const boost::system::error_code& ec, std::size_t) {
            if (!ec) 
            {
                self->net_write_queue.pop_front(); // remove the front element from the queue
                self->doWrite(); // continue next write
            } 
            
            else self->handleError(ec); // handle error
        });
}

// ===================== READ =====================

void Connection::readHeader() {
    boost::asio::async_read(net_socket, boost::asio::buffer(net_header_buffer),
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                self->handleError(ec);
                return;
            }

            uint32_t bodyLength = 0;
            std::memcpy(&bodyLength, self->net_header_buffer.data(), HEADER_SIZE);
            bodyLength = ntohl(bodyLength); // FIXED endianness

            // Validate size
            if (bodyLength == 0) {
                // Decide protocol behavior: ignore or treat as heartbeat
                self->readHeader();
                return;
            }

            if (bodyLength > MAX_MESSAGE_SIZE) {
                self->handleError(boost::asio::error::message_size);
                return;
            }

            self->readBody(bodyLength);
        });
}

void Connection::readBody(std::size_t length) {
    net_body_buffer.resize(length);

    boost::asio::async_read(net_socket, boost::asio::buffer(net_body_buffer),
        [self = shared_from_this()](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                self->handleError(ec);
                return;
            }

            if (self->net_message_handler) {
                std::string msg(self->net_body_buffer.data(), self->net_body_buffer.size());
                self->net_message_handler(msg);
            }

            self->readHeader(); // continue loop
        });
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
    // Graceful shutdown
    if (net_socket.is_open()) 
    {
        boost::system::error_code ignored;
        net_socket.shutdown(tcp::socket::shutdown_both, ignored);
        net_socket.close(ignored);
    }

    // Clear write queue
    net_write_queue.clear();
    net_writing = false;

    // Call disconnect handler safely
    if (net_disconnect_handler) 
    {
        auto handler = std::move(net_disconnect_handler);
        net_disconnect_handler = nullptr;
        handler();
    }
}

} // namespace netwatch::networking