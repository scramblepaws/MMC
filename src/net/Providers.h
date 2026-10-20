#pragma once

#include <QString>
#include <QStringList>

namespace mmc {

// One supported mail provider: where to log in, and where its mail lives.
struct Provider {
    QString id;
    QString name;
    QString authUrl;
    QString tokenUrl;
    QString imapHost;
    int imapPort = 993;
    QString smtpHost;
    int smtpPort = 465;
    QStringList scopes;
    bool smtpImplicitTls = true;   // 465 = TLS from the start, 587 = STARTTLS
    bool imapImplicitTls = true;
    QString userAgent = QStringLiteral("MMC/0.1");
};

const Provider* providerById(const QString& id);
QStringList providerIds();

// Guesses the provider from an email address. Empty when it cannot be told.
QString providerIdForAddress(const QString& address);

// Settings keys used to store the client id / secret of a provider.
QString providerClientIdKey(const QString& providerId);
QString providerClientSecretKey(const QString& providerId);

} // namespace mmc
