#include "AIHelper.h"

#include <boost/asio.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>
#include <QUrl>
#include <QByteArray>
#include <Qt>

#include <array>
#include <thread>

namespace {
struct ParsedHttpUrl {
    bool valid = false;
    QString host;
    QString service;
    QString path;
};

ParsedHttpUrl parseHttpUrl(const QString& raw)
{
    ParsedHttpUrl parsed;
    const QUrl url(raw);
    if (!url.isValid() || url.scheme().toLower() != QStringLiteral("http")) {
        return parsed;
    }
    if (url.host().isEmpty()) {
        return parsed;
    }

    parsed.host = url.host();
    parsed.service = QString::number(url.port(80));
    parsed.path = url.path();
    if (parsed.path.isEmpty()) {
        parsed.path = QStringLiteral("/");
    }
    if (!url.query().isEmpty()) {
        parsed.path += QStringLiteral("?") + url.query();
    }

    parsed.valid = true;
    return parsed;
}

QString extractTextFromResponse(const QJsonObject& root)
{
    const QJsonArray choices = root.value("choices").toArray();
    if (!choices.isEmpty()) {
        const QJsonObject first = choices.at(0).toObject();
        const QJsonObject message = first.value("message").toObject();
        const QString messageContent = message.value("content").toString().trimmed();
        if (!messageContent.isEmpty()) {
            return messageContent;
        }
        const QString text = first.value("text").toString().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
    }

    const QString response = root.value("response").toString().trimmed();
    if (!response.isEmpty()) {
        return response;
    }

    const QString text = root.value("text").toString().trimmed();
    if (!text.isEmpty()) {
        return text;
    }

    return QString();
}
}

AIHelper::AIHelper(QObject* parent)
    : QObject(parent)
{
    const QByteArray key = qgetenv("AI_API_KEY");
    if (!key.isEmpty()) {
        m_apiKey = QString::fromUtf8(key);
    }

    const QByteArray url = qgetenv("AI_API_URL");
    if (!url.isEmpty()) {
        m_apiUrl = QString::fromUtf8(url);
    } else {
        m_apiUrl = QStringLiteral("http://127.0.0.1:11434/v1/chat/completions");
    }

    const QByteArray model = qgetenv("AI_MODEL");
    if (!model.isEmpty()) {
        m_model = QString::fromUtf8(model);
    } else {
        m_model = QStringLiteral("gpt-3.5-turbo");
    }
}

bool AIHelper::isEnabled() const
{
    return canUseHttpEndpoint();
}

bool AIHelper::canUseHttpEndpoint() const
{
    return parseHttpUrl(m_apiUrl).valid;
}

bool AIHelper::isConnected(int timeoutMs) const
{
    Q_UNUSED(timeoutMs);
    const ParsedHttpUrl endpoint = parseHttpUrl(m_apiUrl);
    if (!endpoint.valid) {
        return false;
    }

    using boost::asio::ip::tcp;
    boost::asio::io_context io;
    boost::system::error_code ec;

    tcp::resolver resolver(io);
    const auto endpoints = resolver.resolve(endpoint.host.toStdString(), endpoint.service.toStdString(), ec);
    if (ec) {
        return false;
    }

    tcp::socket socket(io);
    boost::asio::connect(socket, endpoints, ec);
    return !ec;
}

QString AIHelper::requestRephrase(const QString& text) const
{
    const ParsedHttpUrl endpoint = parseHttpUrl(m_apiUrl);
    if (!endpoint.valid) {
        return text;
    }

    QJsonObject body;
    body["model"] = m_model;

    QJsonArray messages;
    QJsonObject sys;
    sys["role"] = QStringLiteral("system");
    sys["content"] = QStringLiteral("You are a concise in-game assistant. Rephrase the user's clue in a short, friendly, and game-appropriate way without changing the meaning.");
    messages.append(sys);

    QJsonObject user;
    user["role"] = QStringLiteral("user");
    user["content"] = text;
    messages.append(user);
    body["messages"] = messages;

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QString request;
    request += QStringLiteral("POST %1 HTTP/1.1\r\n").arg(endpoint.path);
    request += QStringLiteral("Host: %1\r\n").arg(endpoint.host);
    request += QStringLiteral("Content-Type: application/json\r\n");
    request += QStringLiteral("Content-Length: %1\r\n").arg(payload.size());
    if (!m_apiKey.isEmpty()) {
        request += QStringLiteral("Authorization: Bearer %1\r\n").arg(m_apiKey);
    }
    request += QStringLiteral("Connection: close\r\n\r\n");

    using boost::asio::ip::tcp;
    boost::asio::io_context io;
    boost::system::error_code ec;

    tcp::resolver resolver(io);
    const auto endpoints = resolver.resolve(endpoint.host.toStdString(), endpoint.service.toStdString(), ec);
    if (ec) {
        return text;
    }

    tcp::socket socket(io);
    boost::asio::connect(socket, endpoints, ec);
    if (ec) {
        return text;
    }

    const QByteArray http = request.toUtf8() + payload;
    boost::asio::write(socket, boost::asio::buffer(http.constData(), static_cast<std::size_t>(http.size())), ec);
    if (ec) {
        return text;
    }

    std::string response;
    std::array<char, 4096> buf{};
    while (true) {
        const std::size_t n = socket.read_some(boost::asio::buffer(buf), ec);
        if (n > 0) {
            response.append(buf.data(), n);
        }
        if (ec == boost::asio::error::eof) {
            break;
        }
        if (ec) {
            return text;
        }
    }

    const std::size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return text;
    }
    const std::string statusLine = response.substr(0, response.find("\r\n"));
    if (statusLine.find(" 2") == std::string::npos) {
        return text;
    }

    const QByteArray bodyBytes(response.data() + static_cast<std::ptrdiff_t>(headerEnd + 4),
                               static_cast<int>(response.size() - (headerEnd + 4)));
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(bodyBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return text;
    }

    const QString extracted = extractTextFromResponse(doc.object());
    return extracted.isEmpty() ? text : extracted;
}

void AIHelper::rephrase(const QString& text, std::function<void(QString)> callback)
{
    if (!isEnabled()) {
        QTimer::singleShot(0, [text, callback]() {
            callback(text.trimmed());
        });
        return;
    }

    std::thread([this, text, callback]() {
        const QString out = requestRephrase(text).trimmed();
        QMetaObject::invokeMethod(this, [callback, out]() {
            callback(out);
        }, Qt::QueuedConnection);
    }).detach();
}
