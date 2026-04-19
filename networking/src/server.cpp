// Server implementation - listens for incoming connections, manages multiple clients

#include "server.hpp"
#include <iostream>
#include <algorithm>

namespace netwatch::networking {

Server::Server(io_context& io, uint16_t port)
    : server_io(io),
      server_acceptor(io, tcp::endpoint(tcp::v4(), port))
{
    // Allow address reuse so we can restart quickly after a crash
    server_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
}

void Server::start()
{
    std::cout << "[Server] Listening on port " 
              << server_acceptor.local_endpoint().port() << "\n";
    doAccept();
}

void Server::doAccept()
{
    server_acceptor.async_accept(
        [self = shared_from_this()](const boost::system::error_code& ec, tcp::socket socket)
        {
            if (ec)
            {
                // If the acceptor was closed (e.g. during shutdown), stop silently
                if (ec == boost::asio::error::operation_aborted)
                    return;

                std::cerr << "[Server] Accept error: " << ec.message() << "\n";
            }
            else
            {
                std::cout << "[Server] New connection from " 
                          << socket.remote_endpoint() << "\n";

                auto conn = std::make_shared<Connection>(std::move(socket));

                // Set a disconnect handler that removes the connection from our list
                conn->setDisconnectHandler([self, weak = std::weak_ptr<Connection>(conn)]() {
                    auto ptr = weak.lock();
                    if (!ptr) return;

                    self->server_connections.erase(
                        std::remove(self->server_connections.begin(), self->server_connections.end(), ptr),
                        self->server_connections.end()
                    );
                    std::cout << "[Server] Client disconnected. Active connections: " 
                              << self->server_connections.size() << "\n";
                });

                self->server_connections.push_back(conn);
                conn->start();
            }

            // Continue accepting regardless of success/failure
            self->doAccept();
        });
}

std::vector<Ptr<Connection>> Server::getConnections() const
{
    return server_connections;
}

} // namespace netwatch::networking
