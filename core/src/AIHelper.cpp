#include "AIHelper.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QByteArray>
#include <QUrl>
#include <QCoreApplication>

AIHelper::AIHelper(QObject* parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this))
{
    const QByteArray key = qgetenv("AI_API_KEY");
    if (!key.isEmpty()) {
        m_apiKey = QString::fromUtf8(key);
    }
    const QByteArray url = qgetenv("AI_API_URL");
    if (!url.isEmpty()) {
        m_apiUrl = QString::fromUtf8(url);
    } else {
        if (m_apiKey.startsWith("sk-or-")) {
            m_apiUrl = QStringLiteral("https://openrouter.ai/v1/chat/completions");
        } else {
            m_apiUrl = QStringLiteral("https://api.openai.com/v1/chat/completions");
        }
    }
    const QByteArray modelEnv = qgetenv("AI_MODEL");
    if (!modelEnv.isEmpty()) {
        m_model = QString::fromUtf8(modelEnv);
    } else {
        if (m_apiKey.startsWith("sk-or-")) {
            m_model = QStringLiteral("elephant-alpha");
        } else {
            m_model = QStringLiteral("gpt-3.5-turbo");
        }
    }
}

bool AIHelper::isEnabled() const
{
    return !m_apiKey.isEmpty();
}

void AIHelper::rephrase(const QString& text, std::function<void(QString)> callback)
{
    if (!isEnabled()) {
        QTimer::singleShot(0, [text, callback]() { callback(text); });
        return;
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
    body["max_tokens"] = 999999;

    QNetworkRequest req{QUrl(m_apiUrl)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QStringLiteral("Bearer ").append(m_apiKey).toUtf8());

    const QJsonDocument doc(body);
    QNetworkReply* reply = m_net->post(req, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [reply, text, callback]() {
        QString out = text;
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray resp = reply->readAll();
            QJsonParseError err;
            const QJsonDocument respDoc = QJsonDocument::fromJson(resp, &err);
            if (err.error == QJsonParseError::NoError && respDoc.isObject()) {
                QJsonObject obj = respDoc.object();
                QJsonArray choices = obj.value("choices").toArray();
                if (!choices.isEmpty()) {
                    QJsonObject first = choices.at(0).toObject();
                    QJsonObject message = first.value("message").toObject();
                    QString content = message.value("content").toString();
                    if (content.isEmpty()) {
                        content = first.value("text").toString();
                    }
                    if (!content.isEmpty()) {
                        out = content.trimmed();
                    }
                }
            }
        }
        reply->deleteLater();
        callback(out);
    });
}
