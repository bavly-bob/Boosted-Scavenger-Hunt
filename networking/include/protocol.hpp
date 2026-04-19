// defines message structure and encoding/decoding utilities

#pragma once
#include <string>
#include <cstdint>

namespace netwatch::protocol {

// Message types
enum class MessageType : uint8_t {
    Heartbeat = 0,
    SystemStats = 1,
    ProcessList = 2,
    Unknown = 255
};

// Wrap raw JSON string into a framed message
// Format: [uint32_t length][payload]
std::string encode(const std::string& json);

// Extract message length from header
uint32_t decodeHeader(const char* data);

std::string toString(MessageType type);
MessageType fromString(const std::string& typeStr);

} // namespace netwatch::protocol