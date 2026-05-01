#pragma once

#include <QObject>
#include <QString>
#include <functional>

class AIHelper : public QObject {
    QString m_apiKey;
    QString m_apiUrl;
    QString m_model;

    QString requestRephrase(const QString& text) const;
    bool canUseHttpEndpoint() const;

public:
    explicit AIHelper(QObject* parent = nullptr);

    bool isEnabled() const;
    bool isConnected(int timeoutMs = 1500) const;

    void rephrase(const QString& text, std::function<void(QString)> callback);
};
