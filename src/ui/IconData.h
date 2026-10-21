#pragma once

namespace mmc {

// One shape of an icon. The data is the geometry of the shape, the same
// numbers the svg file of the icon holds.
enum class IconShape {
    Path,
    Polyline,
    Polygon,
    Line,
    Circle,
    Rect,
};

struct IconShapeData {
    IconShape kind;
    const char* data;
};

struct IconData {
    const char* name;
    const IconShapeData* shapes;
    int shapeCount;
};

// The icons MMC has, and how many of them there are.
extern const IconData kIcons[];
extern const int kIconCount;

} // namespace mmc
