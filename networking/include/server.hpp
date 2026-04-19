// listens for incoming connections, manages multiple clients, and broadcasts messages

#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include "connection.hpp"
#include "types.hpp"

namespace netwatch::networking {

class Server : public std::enable_shared_from_this<Server> {
public:
    Server(io_context& io, uint16_t port);
    void start();
    std::vector<Ptr<Connection>> getConnections() const;

private: // internal async handlers
    void doAccept();

private: // member variables
    io_context& server_io;
    tcp::acceptor server_acceptor;
    std::vector<Ptr<Connection>> server_connections;
};

} // namespace netwatch::networking