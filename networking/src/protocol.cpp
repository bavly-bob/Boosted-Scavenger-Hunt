// protocol implementation - message encoding/decoding and type conversion

#include "protocol.hpp"
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace netwatch::protocol {

std::string encode(const std::string& json)
{
    uint32_t len_net = htonl(static_cast<uint32_t>(json.size()));
    std::string frame(4 + json.size(), '\0');
    std::memcpy(&frame[0], &len_net, 4);
    std::memcpy(&frame[4], json.data(), json.size());
    return frame;
}

uint32_t decodeHeader(const char* data)
{
    uint32_t len_net = 0;
    std::memcpy(&len_net, data, 4);
    return ntohl(len_net);
}

std::string toString(MessageType type)
{
    switch (type) {
        case MessageType::Heartbeat:    return "Heartbeat";
        case MessageType::SystemStats:  return "SystemStats";
        case MessageType::ProcessList:  return "ProcessList";
        default:                        return "Unknown";
    }
}

MessageType fromString(const std::string& typeStr)
{
    if (typeStr == "Heartbeat")    return MessageType::Heartbeat;
    if (typeStr == "SystemStats")  return MessageType::SystemStats;
    if (typeStr == "ProcessList")  return MessageType::ProcessList;
    return MessageType::Unknown;
}

} // namespace netwatch::protocol
