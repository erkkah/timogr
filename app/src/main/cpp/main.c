#include "tigr.h"
#include <stdio.h>
#include <errno.h>

/**
 * TIGR entry point.
 */
void tigrMain() {
    Tigr* screen = tigrWindow(1, 1, "Hello", TIGR_AUTO | TIGR_2X);

    int x;
    int y;
    int buttons;
    TigrTouchPoint touchPoints[3];

    const char* message = "Hello, world.";
    int textHeight = tigrTextHeight(tfont, message);
    int textWidth = tigrTextWidth(tfont, message);

    float liveTime = 0;

    Tigr* logo = tigrLoadImage("timogr.png");
    if (!logo) {
        tigrError(0, "Failed to load image: %d", errno);
    }

    while (!tigrClosed(screen)) {
        liveTime += tigrTime();

        tigrMouse(screen, &x, &y, &buttons);
        int numTouches = tigrTouch(screen, touchPoints, 3);

        if (numTouches && y > screen->h - screen->h / 10) {
            tigrShowKeyboard(1);
        }

        if (tigrKeyDown(screen, TK_RETURN)) {
            tigrShowKeyboard(0);
        }

        if (tigrKeyDown(screen, TK_ESCAPE)) {
            break;
        }

        tigrClear(screen, tigrRGB(0x80, 0x90, 0xa0));

        int logoX = (screen->w - logo->w) / 2;
        tigrBlitAlpha(screen, logo, logoX, 10, 0, 0, logo->w, logo->h, numTouches > 0 ? 0.5 : 1);

        TPixel mouseLineColor = tigrRGB(0, 0, 0);
        if (buttons != 0) {
            tigrLine(screen, 0, 0, x, y, mouseLineColor);
            tigrLine(screen, 0, screen->h - 1, x, y, mouseLineColor);
            tigrLine(screen, screen->w - 1, 0, x, y, mouseLineColor);
            tigrLine(screen, screen->w - 1, screen->h - 1, x, y, mouseLineColor);
        }

        TPixel touchLineColor = tigrRGB(0, 0x80, 0x80);
        for (int i = 0; i < numTouches; i++) {
            TigrTouchPoint* point = touchPoints + i;
            tigrLine(screen, 0, point->y, screen->w - 1, point->y, touchLineColor);
            tigrLine(screen, point->x, 0, point->x, screen->h - 1, touchLineColor);
        }

        int textX = (screen->w - textWidth) / 2;
        int textY = (screen->h - textHeight) / 2;

        tigrPrint(screen, tfont, textX, textY, tigrRGB(0xff, 0xff, 0xff), message);

        char buf[32];
        sprintf(buf, "%.2f", liveTime);
        tigrPrint(screen, tfont, textX, textY + 20, tigrRGB(0xff, 0xff, 0xff), buf);
        
        tigrUpdate(screen);
    }
    tigrFree(screen);
}
