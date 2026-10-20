#include "net/OAuth2.h"

#include "core/Crypto.h"

#include <QDesktopServices>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>

namespace mmc {
namespace {

QString base64Url(const QByteArray& data)
{
    QByteArray out = data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QString::fromLatin1(out);
}

QString randomHex(int bytes)
{
    return QString::fromLatin1(randomBytes(bytes).toHex());
}

} // namespace

QString OAuth2::makeCodeVerifier()
{
    return base64Url(randomBytes(48));
}

QString OAuth2::codeChallengeFor(const QString& verifier)
{
    return base64Url(QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256));
}

QString OAuth2::redirectUriForPort(int port)
{
    return QStringLiteral("http://127.0.0.1:%1/oauth/callback").arg(port);
}

QString OAuth2::buildAuthUrl(const Provider& provider, const QString& clientId,
                             const QString& redirectUri, const QString& state,
                             const QString& codeChallenge, bool usePkce)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), provider.scopes.join(QChar(' ')));
    query.addQueryItem(QStringLiteral("state"), state);
    if (usePkce && !codeChallenge.isEmpty())
        query.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    if (provider.id == QLatin1String("microsoft")) {
        query.addQueryItem(QStringLiteral("response_mode"), QStringLiteral("query"));
        query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    }
    if (provider.id == QLatin1String("yandex"))
        query.addQueryItem(QStringLiteral("force_confirm"), QStringLiteral("true"));

    QUrl url(provider.authUrl);
    url.setQuery(query);
    return url.toString();
}

namespace {

bool postForm(const QUrl& url,
              const QMap<QString, QString>& fields,
              QByteArray* responseBody,
              QString* error)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery query;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
        query.addQueryItem(it.key(), it.value());
    const QByteArray body = query.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkReply* reply = manager.post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (error) *error = QObject::tr("The server did not answer in time.");
        return false;
    }
    const int status = reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute).toInt();
    *responseBody = reply->readAll();
    reply->deleteLater();

    if (status >= 400) {
        if (error) {
            const QJsonDocument doc = QJsonDocument::fromJson(*responseBody);
            const QString description = doc.object().value(QStringLiteral("error_description")).toString();
            *error = description.isEmpty()
                         ? QObject::tr("The server refused the request (HTTP %1).").arg(status)
                         : description;
        }
        return false;
    }
    return true;
}

bool parseTokenResponse(const QByteArray& body, OAuth2Tokens* tokens, QString* error)
{
    const QJsonObject obj = QJsonDocument::fromJson(body).object();
    if (obj.isEmpty()) {
        if (error) *error = QObject::tr("The server sent an unexpected answer.");
        return false;
    }
    if (obj.contains(QStringLiteral("error"))) {
        if (error) {
            *error = obj.value(QStringLiteral("error_description")).toString(
                obj.value(QStringLiteral("error")).toString());
        }
        return false;
    }
    tokens->accessToken = obj.value(QStringLiteral("access_token")).toString();
    tokens->refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
    tokens->scope = obj.value(QStringLiteral("scope")).toString();
    const int expiresIn = obj.value(QStringLiteral("expires_in")).toInt();
    if (expiresIn > 0)
        tokens->expiry = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
    if (tokens->accessToken.isEmpty()) {
        if (error) *error = QObject::tr("The server did not return an access token.");
        return false;
    }
    return true;
}

} // namespace

bool OAuth2::authorize(const Provider& provider,
                       const QString& clientId,
                       const QString& clientSecret,
                       const QString& redirectUri,
                       OAuth2Tokens* tokens,
                       QString* error)
{
    if (clientId.isEmpty()) {
        if (error)
            *error = QObject::tr("No client id for %1. Register an application and fill in the "
                                 "client id in Settings.").arg(provider.name);
        return false;
    }

    const int port = QUrl(redirectUri).port(56789);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, static_cast<quint16>(port))) {
        if (error)
            *error = QObject::tr("Port %1 is in use, MMC cannot receive the sign in answer. "
                                 "Close the program using it and try again.").arg(port);
        return false;
    }

    const QString state = randomHex(16);
    const QString verifier = makeCodeVerifier();
    const QString challenge = codeChallengeFor(verifier);

    QString code;
    QString returnedState;
    QString oauthError;

    {
        const QString url = buildAuthUrl(provider, clientId, redirectUri, state, challenge, true);
        QDesktopServices::openUrl(QUrl(url));

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
            QTcpSocket* socket = server.nextPendingConnection();
            if (!socket)
                return;
            QByteArray request;
            while (socket->waitForReadyRead(2000))
                request += socket->readAll();
            const QByteArray requestLine = request.left(request.indexOf("\r\n"));
            const QList<QByteArray> parts = requestLine.split(' ');
            if (parts.size() >= 2) {
                const QUrlQuery query(QString::fromLatin1(parts.at(1)));
                code = query.queryItemValue(QStringLiteral("code"));
                returnedState = query.queryItemValue(QStringLiteral("state"));
                oauthError = query.queryItemValue(QStringLiteral("error"));
            }
            const QByteArray reply =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                "Content-Length: 62\r\nConnection: close\r\n\r\n"
                "<html><body><h3>MMC</h3><p>You can close this tab now.</p></body></html>";
            socket->write(reply);
            socket->waitForBytesWritten(2000);
            socket->disconnectFromHost();
            loop.quit();
        });
        timeout.start(5 * 60 * 1000);
        loop.exec();
    }
    server.close();

    if (!oauthError.isEmpty()) {
        if (error) *error = QObject::tr("The provider refused the sign in: %1").arg(oauthError);
        return false;
    }
    if (code.isEmpty()) {
        if (error) *error = QObject::tr("No sign in answer arrived.");
        return false;
    }
    if (returnedState != state) {
        if (error) *error = QObject::tr("The sign in answer did not match the request.");
        return false;
    }

    const auto makeFields = [&](bool withVerifier) {
        QMap<QString, QString> f;
        f.insert(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
        f.insert(QStringLiteral("code"), code);
        f.insert(QStringLiteral("redirect_uri"), redirectUri);
        f.insert(QStringLiteral("client_id"), clientId);
        if (!clientSecret.isEmpty())
            f.insert(QStringLiteral("client_secret"), clientSecret);
        if (withVerifier)
            f.insert(QStringLiteral("code_verifier"), verifier);
        return f;
    };

    QByteArray body;
    QString exchangeError;
    if (!postForm(QUrl(provider.tokenUrl), makeFields(true), &body, &exchangeError)) {
        // Some providers do not implement PKCE. Try again without it before
        // giving up.
        if (!postForm(QUrl(provider.tokenUrl), makeFields(false), &body, &exchangeError)) {
            if (error) *error = exchangeError;
            return false;
        }
    }
    if (!parseTokenResponse(body, tokens, error))
        return false;
    return true;
}

bool OAuth2::refresh(const Provider& provider,
                     const QString& clientId,
                     const QString& clientSecret,
                     const QString& refreshToken,
                     OAuth2Tokens* tokens,
                     QString* error)
{
    QMap<QString, QString> fields;
    fields.insert(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    fields.insert(QStringLiteral("refresh_token"), refreshToken);
    fields.insert(QStringLiteral("client_id"), clientId);
    if (!clientSecret.isEmpty())
        fields.insert(QStringLiteral("client_secret"), clientSecret);
    fields.insert(QStringLiteral("scope"), provider.scopes.join(QChar(' ')));

    QByteArray body;
    if (!postForm(QUrl(provider.tokenUrl), fields, &body, error))
        return false;
    if (!parseTokenResponse(body, tokens, error))
        return false;
    if (tokens->refreshToken.isEmpty())
        tokens->refreshToken = refreshToken;
    return true;
}

} // namespace mmc
