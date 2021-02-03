#include "/Users/erik/src/tigr/tigr.h"

extern "C" void tigrMain() {
    Tigr* screen = tigrWindow(800, 600, "Hello", TIGR_AUTO | TIGR_4X);
    int x;
    int y;
    int buttons;
    while (!tigrClosed(screen)) {
        tigrMouse(screen, &x, &y, &buttons);
        tigrClear(screen, tigrRGB(0x80, 0x90, 0xa0));
        tigrPrint(screen, tfont, 120, 110, tigrRGB(0xff, 0xff, 0xff), "Hello, world.");
        if (buttons != 0) {
            TPixel lineColor = tigrRGB(0, 0, 0);
            tigrLine(screen, 0, 0, x, y, lineColor);
            tigrLine(screen, 0, screen->h - 1, x, y, lineColor);
            tigrLine(screen, screen->w - 1, 0, x, y, lineColor);
            tigrLine(screen, screen->w - 1, screen->h - 1, x, y, lineColor);
        }
        tigrUpdate(screen);
    }
    tigrFree(screen);
}
