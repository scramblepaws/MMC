# The look of MMC: tokens, Lucide icons and Inter

The interface is built from one set of values, so every part of the program
looks the same: the colours, the spacing, the corners and the type live in
`src/ui/Theme.cpp` and nowhere else. The dark surfaces with the violet accent
come from a small table of colours that the stylesheet is written from.

Two things were decided about the icons and the font:

**The icons are drawn by MMC itself.** The outline icons of Lucide are
translated into a table of paths (`tools/gen_icons.py` makes it) and drawn
with QPainter. That way they can be tinted, they stay sharp on every screen
and the program does not need a library to draw a picture.

**The font is Inter, and it is shipped with the program.** Inter is a
variable font, so the weight and the size are set on its own axes and every
weight looks right. It is loaded from the resources, so MMC looks the same on
every computer, also the ones that do not have Inter installed.

Consequence: the program is a little bigger (the font is 850 kilobytes) and
the icons are the ones of Lucide, which are the outline icons most programs
use, so MMC does not look like the only program with that style.
