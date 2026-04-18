#pragma once

#include <QObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;

// Simple AI helper that can rephrase a clue using an external API.
// Reads API key from environment variable `AI_API_KEY` and optional `AI_API_URL`.
class AIHelper : public QObject {
    Q_OBJECT

    QNetworkAccessManager* m_net = nullptr;
    QString m_apiKey;
    QString m_apiUrl;
    QString m_model;

public:
    explicit AIHelper(QObject* parent = nullptr);

    bool isEnabled() const;

    // Rephrase `text` asynchronously; callback will be called with the result.
    void rephrase(const QString& text, std::function<void(QString)> callback);
};
