#include "AIHelper.h"

#include <boost/asio.hpp>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {
using boost::asio::ip::tcp;

void setEnv(const char* key, const char* value)
{
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    boost::asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
    const unsigned short port = acceptor.local_endpoint().port();

    std::atomic<bool> serverReady{false};
    std::thread serverThread([&]() {
        serverReady = true;
        boost::system::error_code ec;
        for (int i = 0; i < 2; ++i) {
            tcp::socket socket(io);
            acceptor.accept(socket, ec);
            if (ec) {
                return;
            }

            std::array<char, 8192> requestBuf{};
            socket.read_some(boost::asio::buffer(requestBuf), ec);
            if (ec && ec != boost::asio::error::eof) {
                return;
            }

            const std::string body = R"({"choices":[{"message":{"content":"AI Hint: mocked reply"}}]})";
            const std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            boost::asio::write(socket, boost::asio::buffer(response), ec);
            if (ec) {
                return;
            }
        }
    });

    while (!serverReady.load()) {
    }

    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/v1/chat/completions";
    setEnv("AI_API_URL", url.c_str());
    setEnv("AI_API_KEY", "test-key");
    setEnv("AI_MODEL", "mock-model");

    AIHelper helper;
    if (!helper.isEnabled()) {
        std::cerr << "AI helper should be enabled for HTTP endpoint\n";
        serverThread.join();
        return 1;
    }
    if (!helper.isConnected()) {
        std::cerr << "AI helper failed connectivity probe\n";
        serverThread.join();
        return 1;
    }

    QString result;
    QEventLoop loop;
    helper.rephrase("original clue", [&](QString out) {
        result = std::move(out);
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();

    serverThread.join();

    if (result.trimmed() != QStringLiteral("AI Hint: mocked reply")) {
        std::cerr << "Unexpected AI rephrase output\n";
        return 1;
    }

    return 0;
}
