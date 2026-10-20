#include "net/Providers.h"

#include <QHash>

namespace mmc {
namespace {

QHash<QString, Provider> makeProviders()
{
    QHash<QString, Provider> map;

    Provider google;
    google.id = QStringLiteral("google");
    google.name = QStringLiteral("Google (Gmail)");
    google.authUrl = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
    google.tokenUrl = QStringLiteral("https://oauth2.googleapis.com/token");
    google.imapHost = QStringLiteral("imap.gmail.com");
    google.imapPort = 993;
    google.smtpHost = QStringLiteral("smtp.gmail.com");
    google.smtpPort = 465;
    google.scopes = {QStringLiteral("https://mail.google.com/"),
                     QStringLiteral("openid"),
                     QStringLiteral("email")};
    map.insert(google.id, google);

    Provider microsoft;
    microsoft.id = QStringLiteral("microsoft");
    microsoft.name = QStringLiteral("Microsoft (Outlook)");
    microsoft.authUrl = QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/authorize");
    microsoft.tokenUrl = QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/token");
    microsoft.imapHost = QStringLiteral("outlook.office365.com");
    microsoft.imapPort = 993;
    microsoft.smtpHost = QStringLiteral("smtp.office365.com");
    microsoft.smtpPort = 587;
    microsoft.smtpImplicitTls = false;   // STARTTLS
    microsoft.scopes = {QStringLiteral("offline_access"),
                        QStringLiteral("https://outlook.office.com/IMAP.AccessAsUser.All"),
                        QStringLiteral("https://outlook.office.com/SMTP.Send")};
    map.insert(microsoft.id, microsoft);

    Provider yandex;
    yandex.id = QStringLiteral("yandex");
    yandex.name = QStringLiteral("Yandex Mail");
    yandex.authUrl = QStringLiteral("https://oauth.yandex.ru/client/authorization");
    yandex.tokenUrl = QStringLiteral("https://oauth.yandex.ru/token");
    yandex.imapHost = QStringLiteral("imap.yandex.ru");
    yandex.imapPort = 993;
    yandex.smtpHost = QStringLiteral("smtp.yandex.ru");
    yandex.smtpPort = 465;
    // Verify the scope name in the Yandex OAuth console, it changes between
    // the old "mail:imap_full" style and the new URL style.
    yandex.scopes = {QStringLiteral("https://oauth.yandex.ru/mail.imap"),
                      QStringLiteral("login:email")};
    map.insert(yandex.id, yandex);

    return map;
}

const QHash<QString, Provider>& providers()
{
    static const QHash<QString, Provider> map = makeProviders();
    return map;
}

} // namespace

const Provider* providerById(const QString& id)
{
    const auto it = providers().constFind(id);
    return it == providers().constEnd() ? nullptr : &it.value();
}

QStringList providerIds()
{
    return {QStringLiteral("google"), QStringLiteral("microsoft"), QStringLiteral("yandex")};
}

QString providerIdForAddress(const QString& address)
{
    const QString domain = address.section('@', 1).toLower();
    if (domain.isEmpty())
        return {};

    if (domain == QStringLiteral("gmail.com") || domain == QStringLiteral("googlemail.com"))
        return QStringLiteral("google");
    if (domain == QStringLiteral("outlook.com") || domain == QStringLiteral("hotmail.com")
        || domain == QStringLiteral("live.com") || domain == QStringLiteral("msn.com")
        || domain.endsWith(QStringLiteral(".outlook.com")))
        return QStringLiteral("microsoft");
    if (domain.startsWith(QStringLiteral("yandex.")) || domain == QStringLiteral("ya.ru")
        || domain == QStringLiteral("yandex.com") || domain.endsWith(QStringLiteral(".yandex.ru")))
        return QStringLiteral("yandex");
    return {};
}

QString providerClientIdKey(const QString& providerId)
{
    return QStringLiteral("provider.%1.clientId").arg(providerId);
}

QString providerClientSecretKey(const QString& providerId)
{
    return QStringLiteral("provider.%1.clientSecret").arg(providerId);
}

} // namespace mmc
