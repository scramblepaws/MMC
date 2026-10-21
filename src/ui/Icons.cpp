#include "ui/Icons.h"

#include "ui/IconData.h"

#include <QHash>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QtMath>

namespace mmc {
namespace {

constexpr double kGrid = 24.0;   // the icons are drawn in a 24 by 24 grid
constexpr double kStroke = 2.0;   // and stroked with this width

// Turns the path data of an svg file into a path to draw.
class PathBuilder
{
public:
    const QPainterPath& path() const { return m_path; }

    void moveTo(const QPointF& point)
    {
        m_path.moveTo(point);
        m_current = point;
        m_subControl = point;
    }

    void lineTo(const QPointF& point)
    {
        m_path.lineTo(point);
        m_current = point;
        m_subControl = point;
    }

    void cubicTo(const QPointF& control1, const QPointF& control2, const QPointF& point)
    {
        m_path.cubicTo(control1, control2, point);
        m_current = point;
        m_subControl = control2;
    }

    void quadTo(const QPointF& control, const QPointF& point)
    {
        m_path.quadTo(control, point);
        m_current = point;
        m_subControl = control;
    }

    void close()
    {
        m_path.closeSubpath();
        m_current = m_start;
        m_subControl = m_current;
    }

    void setStart(const QPointF& point) { m_start = point; }

    // The control point of the last curve, for the S and T commands.
    const QPointF& subControl() const { return m_subControl; }
    void setSubControl(const QPointF& point) { m_subControl = point; }
    const QPointF& current() const { return m_current; }

    // An elliptical arc, the same as the A command of svg.
    void arcTo(const QPointF& end, double rx, double ry, double rotationDegrees,
               bool largeArc, bool sweep)
    {
        if (rx == 0 || ry == 0) {
            lineTo(end);
            return;
        }
        rx = std::abs(rx);
        ry = std::abs(ry);

        const double phi = qDegreesToRadians(rotationDegrees);
        const double cosPhi = std::cos(phi);
        const double sinPhi = std::sin(phi);

        const double dx2 = (m_current.x() - end.x()) / 2.0;
        const double dy2 = (m_current.y() - end.y()) / 2.0;
        const double x1 = cosPhi * dx2 + sinPhi * dy2;
        const double y1 = -sinPhi * dx2 + cosPhi * dy2;

        double lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
        if (lambda > 1) {
            const double scale = std::sqrt(lambda);
            rx *= scale;
            ry *= scale;
        }

        double coefficient = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
        if (coefficient < 0)
            coefficient = 0;
        coefficient /= rx * rx * y1 * y1 + ry * ry * x1 * x1;
        coefficient = std::sqrt(coefficient);
        if (largeArc == sweep)
            coefficient = -coefficient;

        const double cx1 = coefficient * rx * y1 / ry;
        const double cy1 = -coefficient * ry * x1 / rx;

        const double cx = cosPhi * cx1 - sinPhi * cy1 + (m_current.x() + end.x()) / 2.0;
        const double cy = sinPhi * cx1 + cosPhi * cy1 + (m_current.y() + end.y()) / 2.0;

        auto angle = [](double ux, double uy, double vx, double vy) {
            const double dot = ux * vx + uy * vy;
            const double length = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
            double value = std::acos(qBound(-1.0, std::min(1.0, dot / length), 1.0));
            if (ux * vy - uy * vx < 0)
                value = -value;
            return value;
        };

        const double theta1 = angle(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
        double deltaTheta = angle((x1 - cx1) / rx, (y1 - cy1) / ry,
                                  (-x1 - cx1) / rx, (-y1 - cy1) / ry);
        if (!sweep && deltaTheta > 0)
            deltaTheta -= 2 * M_PI;
        else if (sweep && deltaTheta < 0)
            deltaTheta += 2 * M_PI;

        // The arc is drawn as a few curves.
        const int steps = qMax(1, static_cast<int>(std::ceil(std::abs(deltaTheta) / (M_PI / 2))));
        const double delta = deltaTheta / steps;
        double angleNow = theta1;
        double t = 4.0 / 3.0 * std::tan(delta / 4.0);

        for (int i = 0; i < steps; ++i) {
            const double angleNext = angleNow + delta;
            const double cosNow = std::cos(angleNow);
            const double sinNow = std::sin(angleNow);
            const double cosNext = std::cos(angleNext);
            const double sinNext = std::sin(angleNext);

            auto point = [&](double cosine, double sine) {
                return QPointF(cx + rx * cosine * cosPhi - ry * sine * sinPhi,
                               cy + rx * cosine * sinPhi + ry * sine * cosPhi);
            };
            auto derivative = [&](double cosine, double sine) {
                return QPointF(-rx * sine * cosPhi - ry * cosine * sinPhi,
                               -rx * sine * sinPhi + ry * cosine * cosPhi);
            };

            const QPointF p1 = point(cosNow, sinNow);
            const QPointF p2 = point(cosNext, sinNext);
            const QPointF d1 = derivative(cosNow, sinNow);
            const QPointF d2 = derivative(cosNext, sinNext);

            m_path.cubicTo(p1 + d1 * t, p2 - d2 * t, p2);
            m_current = p2;
            angleNow = angleNext;
        }
        m_subControl = m_current;
    }

private:
    QPainterPath m_path;
    QPointF m_current;
    QPointF m_start;
    QPointF m_subControl;
};

class Scanner
{
public:
    explicit Scanner(const QByteArray& data) : m_data(data) {}

    bool atEnd() const { return m_position >= m_data.size(); }

    int position() const { return m_position; }

    void skipSeparators()
    {
        while (m_position < m_data.size()
               && (m_data.at(m_position) == ' ' || m_data.at(m_position) == ','
                   || m_data.at(m_position) == '\n' || m_data.at(m_position) == '\r'
                   || m_data.at(m_position) == '\t')) {
            ++m_position;
        }
    }

    bool hasNumber()
    {
        skipSeparators();
        if (m_position >= m_data.size())
            return false;
        const char ch = m_data.at(m_position);
        return (ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.';
    }

    // True when the next thing in the data is a number and not a command.
    bool nextIsNumber()
    {
        skipSeparators();
        if (m_position >= m_data.size())
            return false;
        const char ch = m_data.at(m_position);
        return (ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.';
    }

    double number()
    {
        skipSeparators();
        const char* start = m_data.constData() + m_position;
        char* end = nullptr;
        const double value = std::strtod(start, &end);
        m_position += static_cast<int>(end - start);
        return value;
    }

    bool hasFlag()
    {
        skipSeparators();
        if (m_position >= m_data.size())
            return false;
        const char ch = m_data.at(m_position);
        return ch == '0' || ch == '1';
    }

    bool flag()
    {
        skipSeparators();
        const bool value = m_data.at(m_position) == '1';
        ++m_position;
        return value;
    }

    char command()
    {
        skipSeparators();
        if (m_position >= m_data.size())
            return 0;
        return m_data.at(m_position++);
    }

    bool atNumber() const
    {
        if (m_position >= m_data.size())
            return false;
        const char ch = m_data.at(m_position);
        return (ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.';
    }

private:
    QByteArray m_data;
    int m_position = 0;
};

QPainterPath pathFromData(const QByteArray& data)
{
    PathBuilder builder;
    Scanner scanner(data);
    QPointF current;
    QPointF start;
    char previous = 0;   // the command that was used, with its case

    while (!scanner.atEnd()) {
        const int before = scanner.position();
        // A number where a command should be means the last command is
        // repeated: "M 1 2 3 4" is a move and then a line.
        char command = 0;
        if (scanner.nextIsNumber() && previous != 0) {
            command = previous;
            if (command == 'M')
                command = 'L';
            else if (command == 'm')
                command = 'l';
        } else {
            command = scanner.command();
        }
        if (command == 0)
            break;
        const bool relative = (command >= 'a' && command <= 'z');
        const char upper = relative ? static_cast<char>(command - 'a' + 'A') : command;
        const char previousUpper = (previous >= 'a' && previous <= 'z')
                                       ? static_cast<char>(previous - 'a' + 'A')
                                       : previous;

        auto point = [&](double x, double y) {
            return relative ? QPointF(current.x() + x, current.y() + y) : QPointF(x, y);
        };

        switch (upper) {
        case 'M': {
            const double x = scanner.number();
            const double y = scanner.number();
            current = point(x, y);
            start = current;
            builder.setStart(current);
            builder.moveTo(current);
            break;
        }
        case 'L': {
            const double x = scanner.number();
            const double y = scanner.number();
            current = point(x, y);
            builder.lineTo(current);
            break;
        }
        case 'H': {
            const double x = scanner.number();
            current = relative ? QPointF(current.x() + x, current.y()) : QPointF(x, current.y());
            builder.lineTo(current);
            break;
        }
        case 'V': {
            const double y = scanner.number();
            current = relative ? QPointF(current.x(), current.y() + y) : QPointF(current.x(), y);
            builder.lineTo(current);
            break;
        }
        case 'C': {
            const double x1 = scanner.number(), y1 = scanner.number();
            const double x2 = scanner.number(), y2 = scanner.number();
            const double x = scanner.number(), y = scanner.number();
            const QPointF control1 = point(x1, y1);
            const QPointF control2 = point(x2, y2);
            const QPointF end = point(x, y);
            builder.cubicTo(control1, control2, end);
            current = end;
            break;
        }
        case 'S': {
            const double x2 = scanner.number(), y2 = scanner.number();
            const double x = scanner.number(), y = scanner.number();
            QPointF control1 = builder.subControl();
            if (previousUpper != 'C' && previousUpper != 'S')
                control1 = current;
            else
                control1 = QPointF(2 * current.x() - control1.x(), 2 * current.y() - control1.y());
            const QPointF control2 = point(x2, y2);
            const QPointF end = point(x, y);
            builder.cubicTo(control1, control2, end);
            current = end;
            break;
        }
        case 'Q': {
            const double x1 = scanner.number(), y1 = scanner.number();
            const double x = scanner.number(), y = scanner.number();
            const QPointF control = point(x1, y1);
            const QPointF end = point(x, y);
            builder.quadTo(control, end);
            current = end;
            break;
        }
        case 'T': {
            const double x = scanner.number(), y = scanner.number();
            QPointF control = builder.subControl();
            if (previousUpper != 'Q' && previousUpper != 'T')
                control = current;
            else
                control = QPointF(2 * current.x() - control.x(), 2 * current.y() - control.y());
            const QPointF end = point(x, y);
            builder.quadTo(control, end);
            current = end;
            break;
        }
        case 'A': {
            const double rx = scanner.number();
            const double ry = scanner.number();
            const double rotation = scanner.number();
            const bool largeArc = scanner.flag();
            const bool sweep = scanner.flag();
            const double x = scanner.number();
            const double y = scanner.number();
            const QPointF end = point(x, y);
            builder.arcTo(end, rx, ry, rotation, largeArc, sweep);
            current = end;
            break;
        }
        case 'Z': {
            builder.close();
            current = start;
            break;
        }
        default:
            break;
        }
        previous = command;
        if (scanner.position() == before) {
            // Nothing was read, stop instead of going round for ever.
            break;
        }
    }
    return builder.path();
}

QPainterPath shapeToPath(IconShape kind, const QByteArray& data)
{
    Scanner scanner(data);
    switch (kind) {
    case IconShape::Path:
        return pathFromData(data);
    case IconShape::Polyline:
    case IconShape::Polygon: {
        QPainterPath path;
        bool first = true;
        while (scanner.hasNumber()) {
            const double x = scanner.number();
            const double y = scanner.number();
            if (first) {
                path.moveTo(x, y);
                first = false;
            } else {
                path.lineTo(x, y);
            }
        }
        if (kind == IconShape::Polygon)
            path.closeSubpath();
        return path;
    }
    case IconShape::Line: {
        const double x1 = scanner.number(), y1 = scanner.number();
        const double x2 = scanner.number(), y2 = scanner.number();
        QPainterPath path;
        path.moveTo(x1, y1);
        path.lineTo(x2, y2);
        return path;
    }
    case IconShape::Circle: {
        const double cx = scanner.number(), cy = scanner.number(), r = scanner.number();
        QPainterPath path;
        path.addEllipse(QPointF(cx, cy), r, r);
        return path;
    }
    case IconShape::Rect: {
        const double x = scanner.number(), y = scanner.number();
        const double w = scanner.number(), h = scanner.number();
        QPainterPath path;
        path.addRect(QRectF(x, y, w, h));
        return path;
    }
    }
    return {};
}

const IconData* findIcon(const QString& name)
{
    for (int i = 0; i < kIconCount; ++i) {
        if (name == QLatin1String(kIcons[i].name))
            return &kIcons[i];
    }
    return nullptr;
}

} // namespace

bool Icons::has(const QString& name)
{
    return findIcon(name) != nullptr;
}

QStringList Icons::names()
{
    QStringList out;
    for (int i = 0; i < kIconCount; ++i)
        out << QLatin1String(kIcons[i].name);
    return out;
}

QPixmap Icons::pixmap(const QString& name, const QColor& colour, int size)
{
    const QImage picture = image(name, colour, size);
    if (picture.isNull())
        return {};
    return QPixmap::fromImage(picture);
}

QImage Icons::image(const QString& name, const QColor& colour, int size)
{
    const IconData* icon = findIcon(name);
    if (!icon || size <= 0)
        return {};

    // The drawing is done on a bigger canvas and scaled down, so the lines
    // stay smooth.
    const int factor = 2;
    QImage canvas(size * factor, size * factor, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(size * factor / kGrid, size * factor / kGrid);

    QPen pen(colour.isValid() ? colour : QColor(0xA0, 0xA6, 0xB7));
    pen.setWidthF(kStroke);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    for (int i = 0; i < icon->shapeCount; ++i) {
        const IconShapeData& shape = icon->shapes[i];
        const QPainterPath path = shapeToPath(shape.kind, QByteArray(shape.data));
        painter.drawPath(path);
    }
    painter.end();

    return canvas.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QIcon Icons::icon(const QString& name, const QColor& colour, int size)
{
    const QPixmap picture = pixmap(name, colour, size);
    if (picture.isNull())
        return {};
    return QIcon(picture);
}

} // namespace mmc
