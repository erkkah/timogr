#ifndef TIGR_GLUE_H
#define TIGR_GLUE_H

#include "/Users/erik/src/tigr/src/tigr_android.h"

void startTigrThread();
void stopTigrThread();
void setTigrWindow(ANativeWindow* window);
void resetTigrWindow(ANativeWindow* window);
void setTigrInputQueue(AInputQueue* queue);
void resetTigrInputQueue(AInputQueue* queue);

#endif  // TIGR_GLUE_H
