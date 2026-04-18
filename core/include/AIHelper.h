#pragma once

#include <QObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;

class AIHelper : public QObject {
    Q_OBJECT

    QNetworkAccessManager* m_net = nullptr;
    QString m_apiKey;
    QString m_apiUrl;
    QString m_model;

public:
    explicit AIHelper(QObject* parent = nullptr);

    bool isEnabled() const;

    void rephrase(const QString& text, std::function<void(QString)> callback);
};
