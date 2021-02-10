#ifndef TIGR_GLUE_H
#define TIGR_GLUE_H

#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>

void startTigr(ANativeActivity* activity);
void pauseTigr();
void resumeTigr();
void stopTigr();
void setTigrWindow(ANativeWindow* window);
void resetTigrWindow(ANativeWindow* window);
void refreshTigrWindow();
void setTigrInputQueue(AInputQueue* queue);
void resetTigrInputQueue(AInputQueue* queue);

#endif  // TIGR_GLUE_H
