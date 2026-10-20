#include <QFile>
#include <QImage>
#include <QImageReader>

#include "core/ProviderIcons.h"
#include "test_harness.h"

using namespace mmc;

void test_icons(TestRunner& runner)
{
    // The logos of the providers are in the resources, they have to be there
    // and they have to be readable. QImage is used instead of QPixmap because
    // the test program has no window system.
    for (const QString& provider : {QStringLiteral("google"), QStringLiteral("microsoft"),
                                    QStringLiteral("yandex")}) {
        const QString name = (provider == QLatin1String("google"))   ? QStringLiteral("gmail")
                             : (provider == QLatin1String("microsoft")) ? QStringLiteral("outlook")
                                                                       : provider;
        const QString path = QStringLiteral(":/icons/%1.png").arg(name);
        CHECK(QFile::exists(path));

        QImageReader reader(path);
        CHECK(reader.canRead());
        const QImage image = reader.read();
        CHECK(!image.isNull());
        CHECK_EQ(image.width(), 64);
        CHECK_EQ(image.height(), 64);
        // The logo has to have something in it, not only transparent pixels.
        CHECK(image.hasAlphaChannel());
    }

    // A provider without a logo gives nothing, not a broken picture.
    CHECK(providerPixmap(QStringLiteral("nobody")).isNull());
}
