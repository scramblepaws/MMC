#include <QImage>

#include "ui/Icons.h"
#include "test_harness.h"

using namespace mmc;

void test_icons(TestRunner& runner)
{
    // Every icon has to draw something that stays inside its square. An icon
    // that is drawn wrong, for example when a path is read badly, shows up
    // here as a shape that is too big or that is not there at all.
    for (const QString& name : Icons::names()) {
        const QImage picture = Icons::image(name, QColor(0xF2, 0xF3, 0xF7), 64);
        CHECK(!picture.isNull());
        if (picture.isNull())
            continue;

        int inked = 0;
        int left = 64, top = 64, right = 0, bottom = 0;
        for (int y = 0; y < picture.height(); ++y) {
            for (int x = 0; x < picture.width(); ++x) {
                if (qAlpha(picture.pixel(x, y)) > 20) {
                    ++inked;
                    left = qMin(left, x);
                    top = qMin(top, y);
                    right = qMax(right, x);
                    bottom = qMax(bottom, y);
                }
            }
        }
        if (inked == 0) {
            runner.check(false, "the icon is empty", __FILE__, __LINE__);
            continue;
        }
        // The drawing of an icon stays in its square.
        if (left < 0 || top < 0 || right >= picture.width() || bottom >= picture.height())
            runner.check(false, "the icon is drawn outside of its square", __FILE__, __LINE__);
        // And it is not a dot and not a blob.
        if (inked > picture.width() * picture.height() / 2)
            runner.check(false, "the icon is a blob", __FILE__, __LINE__);
    }

    // A provider without an icon gives nothing, not a broken picture.
    CHECK(Icons::image(QStringLiteral("nobody"), QColor(0xFF, 0xFF, 0xFF), 18).isNull());
}
