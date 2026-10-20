# Setting up the providers

MMC signs in the way an application does, not the way a person does. Every
provider therefore needs an application registration with a client id and a
client secret, and the user has to allow the app once per mailbox.

The redirect address MMC listens on is:

```
http://127.0.0.1:56789/oauth/callback
```

Fill the client id and the client secret into **Settings** in MMC. The
values live in the encrypted vault, they are not in the repository.

## Google (Gmail)

1. Open the [Google Cloud console](https://console.cloud.google.com/), make a
   project.
2. Enable the **Gmail API** for that project.
3. Under *APIs and services → OAuth consent screen*, set the app to
   *External* and add your own mail addresses as test users.
4. Under *Credentials*, make an **OAuth client ID** of type *Desktop app*.
5. Add the redirect URI `http://127.0.0.1:56789/oauth/callback`.
6. Copy the client id and the client secret into MMC.

The scopes MMC asks for are `https://mail.google.com/`, `openid` and
`email`. Because a test user consent screen only allows 100 test users, add
every address you plan to use, or publish the app if you have a Workspace
domain.

## Microsoft (Outlook)

1. Open the [Microsoft Entra admin center](https://entra.microsoft.com/),
   register an application.
2. Under *Authentication*, add a **Mobile and desktop application** platform
   with the redirect URI `http://127.0.0.1:56789/oauth/callback`.
3. Under *API permissions*, add the delegated permissions
   `IMAP.AccessAsUser.All` and `SMTP.Send` of the Office 365 Exchange Online
   API.
4. Copy the application (client) id and the client secret into MMC.

A secret is not needed for a public client, leave the secret empty if you
made one without a secret.

## Yandex Mail

1. Open the [Yandex OAuth console](https://oauth.yandex.ru/client/new) and
   create an application.
2. The redirect URI is `http://127.0.0.1:56789/oauth/callback`.
3. Under *Rights*, add access to mail and to the mail address.
4. Copy the id and the secret into MMC.

MMC asks for `https://oauth.yandex.ru/mail.imap` and `login:email`. Yandex
has changed its scope names before, so check the rights list in the console
against what the app asks for if a sign in fails with a rights error.

## App passwords instead

Every provider also accepts an app password, and MMC can use one when the
account has no token. Generate the app password in the provider settings and
paste it into the account dialog. Google is retiring app passwords for some
accounts, so the browser sign in is the better path.
