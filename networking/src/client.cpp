// client impementation - connects to server and sends messages

#include "client.hpp"
#include "connection.hpp"
#include <iostream>

namespace netwatch::networking {

Client::Client(io_context& io) : net_io_context(io), net_resolver(io) {}


// Resolve + start async connect
void Client::connect(const std::string& host, uint16_t port) 
{
    tcp::resolver::results_type endpoints = net_resolver.resolve(host, std::to_string(port));
    doConnect(endpoints);
}


// Actually perform async connection
void Client::doConnect(const tcp::resolver::results_type& endpoints) 
{
    // Create a temporary socket for connection attempt
    Ptr<tcp::socket> socket = std::make_shared<tcp::socket>(net_io_context);

    boost::asio::async_connect(*socket, endpoints, 
                                [self = shared_from_this(), socket]
                                (const boost::system::error_code& ec, const tcp::endpoint&) 
        {
            if (!ec) 
            {
                std::cout << "[Client] Connected successfully\n";

                // Wrap socket into Connection object
                self->net_connection = std::make_shared<Connection>(std::move(*socket));

                self->net_connection->setDisconnectHandler([self]() {
                    std::cout << "[Client] Disconnected from server\n";
                    self->net_connection.reset();
                });

                self->net_connection->start(); // start reading
            } 
            else
                std::cerr << "[Client] Connection failed: " << ec.message() << "\n";
        }
    );
}


// Send message through active connection
void Client::send(const std::string& message) 
{
    if (net_connection && net_connection->isOpen()) 
        net_connection->send(message);
    else 
        std::cerr << "[Client] Cannot send, no active connection\n";
}


// Disconnect safely
void Client::disconnect() 
{
    if (net_connection) 
    {
        std::cout << "[Client] Disconnecting...\n";
        net_connection->send(""); // optional: send shutdown signal
        net_connection.reset();   // release connection
    }
}


// Access current connection
Ptr<Connection> Client::getConnection() const 
{
    return net_connection;
}

} // namespace netwatch::networking