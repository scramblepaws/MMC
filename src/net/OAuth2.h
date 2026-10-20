#pragma once

#include "net/Providers.h"

#include <QDateTime>
#include <QString>

namespace mmc {

struct OAuth2Tokens {
    QString accessToken;
    QString refreshToken;
    QDateTime expiry;      // invalid when the provider does not tell us
    QString scope;
    bool isValid() const { return !accessToken.isEmpty(); }
};

// The desktop OAuth2 flow: opens the browser, listens on a loopback port for
// the redirect, exchanges the code for tokens and can refresh them later.
class OAuth2
{
public:
    static constexpr int kDefaultLoopbackPort = 56789;

    // Runs the interactive flow. Returns false and fills in error on failure.
    // The wait happens in a nested event loop, so the window stays alive.
    static bool authorize(const Provider& provider,
                          const QString& clientId,
                          const QString& clientSecret,
                          const QString& redirectUri,
                          OAuth2Tokens* tokens,
                          QString* error);

    static bool refresh(const Provider& provider,
                        const QString& clientId,
                        const QString& clientSecret,
                        const QString& refreshToken,
                        OAuth2Tokens* tokens,
                        QString* error);

    static QString buildAuthUrl(const Provider& provider,
                                const QString& clientId,
                                const QString& redirectUri,
                                const QString& state,
                                const QString& codeChallenge,
                                bool usePkce);

    static QString makeCodeVerifier();
    static QString codeChallengeFor(const QString& verifier);
    static QString redirectUriForPort(int port);
};

} // namespace mmc
