#pragma once

#include <boost/asio.hpp>

#include <functional>
#include <memory>
#include <string>

namespace netwatch::networking {

using io_context = boost::asio::io_context;
using tcp = boost::asio::ip::tcp;

template <typename T>
using Ptr = std::shared_ptr<T>;

using MessageHandler = std::function<void(const std::string&)>;
using DisconnectHandler = std::function<void()>;

} // namespace netwatch::networking
