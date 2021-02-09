#include "tigrglue.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <android/looper.h>
#include <android/asset_manager.h>

#include "swappy/swappyGL.h"

#include "/Users/erik/src/tigr/src/tigr_android.h"
#include "log.h"

extern "C" void tigrMain();

namespace {

typedef enum {
    MAIN_ID,
    INPUT_ID,
} LooperID;

pthread_t tigrThread = 0;
pthread_mutex_t tokenLock;
pthread_cond_t tokenCond;

typedef enum {
    TIGR_ALIVE,
    TIGR_DYING,
    TIGR_DEAD,
} ThreadState;

int tigrThreadState = TIGR_DEAD;

int messageReadFd;
int messageWriteFd;

AInputQueue* inputQueue;
ALooper* looper;
ANativeActivity* activity;
ANativeWindow* window;

void waitForTigrThread() {
    pthread_mutex_lock(&tokenLock);
    if (tigrThreadState == TIGR_ALIVE) {
        pthread_cond_wait(&tokenCond, &tokenLock);
    }
    pthread_mutex_unlock(&tokenLock);
}

void notifyMainThread() {
    pthread_mutex_lock(&tokenLock);
    pthread_cond_broadcast(&tokenCond);
    pthread_mutex_unlock(&tokenLock);
}

typedef enum {
    SET_WINDOW,
    RESET_WINDOW,
    SET_INPUT,
    RESET_INPUT,
    DESTROY,
} TigrMessage;

struct TigrMessageData {
    TigrMessage message;
    ANativeWindow* window;
    AInputQueue* queue;
};

void writeTigrMessage(TigrMessageData messageData) {
    if (tigrThreadState == TIGR_DEAD) {
        return;
    }
    if (write(messageWriteFd, &messageData, sizeof(messageData)) != sizeof(messageData)) {
        LOGE("Failed to write tigr message: %s", strerror(errno));
    } else {
        waitForTigrThread();
    }
}

void readTigrMessage(TigrMessageData* messageData) {
    if (read(messageReadFd, messageData, sizeof(TigrMessageData)) != sizeof(TigrMessageData)) {
        LOGE("Failed to read tigr message: %s", strerror(errno));
    }
}

}  // namespace

void startTigr(ANativeActivity* a) {
    activity = a;

    int fds[2];
    if (!pipe(fds)) {
        // handle error
    }
    messageReadFd = fds[0];
    messageWriteFd = fds[1];

    pthread_mutex_init(&tokenLock, NULL);
    pthread_cond_init(&tokenCond, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(
        &tigrThread, &attr,
        [](void*) -> void* {
            looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
            ALooper_addFd(looper, messageReadFd, MAIN_ID, ALOOPER_EVENT_INPUT, nullptr, nullptr);

            tigrThreadState = TIGR_ALIVE;
            tigrMain();

            if (inputQueue != 0) {
                AInputQueue_detachLooper(inputQueue);
            }

            if (tigrThreadState == TIGR_ALIVE) {
                tigrThreadState = TIGR_DYING;
                ANativeActivity_finish(activity);
            }

            tigrThreadState = TIGR_DEAD;
            tigrThread = 0;

            return nullptr;
        },
        nullptr);
}

void stopTigr() {
    if (tigrThreadState == TIGR_ALIVE) {
        tigrThreadState = TIGR_DYING;
        writeTigrMessage(TigrMessageData{
            .message = DESTROY,
        });
    }

    if (tigrThread != 0) {
        pthread_join(tigrThread, nullptr);
    }

    pthread_cond_destroy(&tokenCond);
    pthread_mutex_destroy(&tokenLock);
    close(messageReadFd);
    close(messageWriteFd);

    inputQueue = nullptr;
    looper = nullptr;
    activity = nullptr;
    window = nullptr;
}

void setTigrWindow(ANativeWindow* newWindow) {
    window = newWindow;
    writeTigrMessage(TigrMessageData{
        .message = SET_WINDOW,
        .window = window,
    });
}

void resetTigrWindow(ANativeWindow* oldWindow) {
    writeTigrMessage(TigrMessageData{
        .message = RESET_WINDOW,
        .window = window,
    });
    window = nullptr;
}

void refreshTigrWindow() {
    writeTigrMessage(TigrMessageData{
        .message = RESET_WINDOW,
        .window = window,
    });
    writeTigrMessage(TigrMessageData{
        .message = SET_WINDOW,
        .window = window,
    });
}

void setTigrInputQueue(AInputQueue* queue) {
    writeTigrMessage(TigrMessageData{
        .message = SET_INPUT,
        .queue = queue,
    });
}

void resetTigrInputQueue(AInputQueue* queue) {
    writeTigrMessage(TigrMessageData{
        .message = RESET_INPUT,
        .queue = queue,
    });
}

extern "C" void android_swap(EGLDisplay display, EGLSurface surface) {
    SwappyGL_swap(display, surface);
}

extern "C" int android_pollEvent(int (*eventHandler)(AndroidEvent event, void* userData), void* userData) {
    int events;
    int eventID = ALooper_pollOnce(0, nullptr, &events, nullptr);
    switch (eventID) {
        case MAIN_ID: {
            TigrMessageData messageData;
            readTigrMessage(&messageData);
            switch (messageData.message) {
                case SET_WINDOW:
                    eventHandler(
                        AndroidEvent{
                            .type = AE_WINDOW_CREATED,
                            .window = messageData.window,
                        },
                        userData);
                    break;
                case RESET_WINDOW:
                    eventHandler(
                        AndroidEvent{
                            .type = AE_WINDOW_DESTROYED,
                            .window = messageData.window,
                        },
                        userData);
                    break;
                case SET_INPUT:
                    if (inputQueue != nullptr) {
                        AInputQueue_detachLooper(inputQueue);
                    }
                    inputQueue = messageData.queue;
                    AInputQueue_attachLooper(inputQueue, looper, INPUT_ID, nullptr, nullptr);
                    break;
                case RESET_INPUT:
                    AInputQueue_detachLooper(messageData.queue);
                    inputQueue = nullptr;
                    break;
                case DESTROY:
                    eventHandler(
                        AndroidEvent{
                            .type = AE_ACTIVITY_DESTROYED,
                        },
                        userData);
                    break;
            }
            notifyMainThread();
            break;
        }

        case INPUT_ID: {
            AInputEvent* event = NULL;
            while (AInputQueue_getEvent(inputQueue, &event) >= 0) {
                if (AInputQueue_preDispatchEvent(inputQueue, event)) {
                    continue;
                }
                int32_t handled = eventHandler(
                    AndroidEvent{
                        .type = AE_INPUT,
                        .inputEvent = event,
                    },
                    userData);
                AInputQueue_finishEvent(inputQueue, event, handled);
            }
            break;
        }

        default:
            if (eventID >= 0) {
                LOGE("Unhandled event ID: %d", eventID);
            }
            return 0;
    }

    return eventID >= 0 ? 1 : 0;
}

extern "C" void* android_loadAsset(const char* filename, int* length) {
    void* assetBuffer = 0;

    AAsset* asset = AAssetManager_open(activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (asset == 0) {
        *length = 0;
        return assetBuffer;
    }

    const void* assetData = AAsset_getBuffer(asset);
    if (assetData != 0) {
        size_t length = AAsset_getLength(asset);
        char* assetBuffer = (char*)malloc(length + 1);
        if (assetBuffer != 0) {
            assetBuffer[length] = 0;
            memcpy(assetBuffer, assetData, length);
        }
    }

    AAsset_close(asset);
    return assetBuffer;
}
