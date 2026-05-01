#include "protocol.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

int main()
{
    const std::string payload = R"({"type":"join","version":1})";
    const std::string frame = netwatch::protocol::encode(payload);
    if (frame.size() != payload.size() + 4) {
        std::cerr << "Encoded frame size mismatch\n";
        return 1;
    }

    const std::uint32_t decodedSize = netwatch::protocol::decodeHeader(frame.data());
    if (decodedSize != payload.size()) {
        std::cerr << "Decoded header length mismatch\n";
        return 1;
    }

    const std::string body = frame.substr(4);
    if (body != payload) {
        std::cerr << "Frame payload mismatch\n";
        return 1;
    }

    if (netwatch::protocol::toString(netwatch::protocol::MessageType::Heartbeat) != "Heartbeat") {
        std::cerr << "Message type toString mismatch\n";
        return 1;
    }
    if (netwatch::protocol::fromString("SystemStats") != netwatch::protocol::MessageType::SystemStats) {
        std::cerr << "Message type fromString mismatch\n";
        return 1;
    }

    return 0;
}
