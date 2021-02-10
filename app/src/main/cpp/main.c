#include "/Users/erik/src/tigr/tigr.h"
#include <stdio.h>

/**
 * TiGr entry point.
 */
void tigrMain() {
    Tigr* screen = tigrWindow(1, 1, "Hello", TIGR_AUTO | TIGR_4X);
    int x;
    int y;
    int buttons;

    const char* message = "Hello, world.";
    int textHeight = tigrTextHeight(tfont, message);
    int textWidth = tigrTextWidth(tfont, message);

    float liveTime = 0;

    while (!tigrClosed(screen)) {
        liveTime += tigrTime();

        tigrMouse(screen, &x, &y, &buttons);
        tigrClear(screen, tigrRGB(0x80, 0x90, 0xa0));

        int textX = (screen->w - textWidth) / 2;
        int textY = (screen->h - textHeight) / 2;

        tigrPrint(screen, tfont, textX, textY, tigrRGB(0xff, 0xff, 0xff), message);

        char buf[32];
        sprintf(buf, "%.2f", liveTime);
        tigrPrint(screen, tfont, textX, textY + 20, tigrRGB(0xff, 0xff, 0xff), buf);
        
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
