#include "tigrglue.h"

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <android/looper.h>
#include <android/asset_manager.h>

#include "swappy/swappyGL.h"

#include "src/tigr_android.h"
#include "log.h"
#include "jniglue.h"

extern "C" void tigrMain();

namespace {

/// Global state, Highlander style.
/// There can be only one instance of the TIGR native activity.

typedef enum {
    MAIN_ID,
    INPUT_ID,
} LooperID;

pthread_t tigrThread = 0;
pthread_mutex_t tokenLock;
pthread_cond_t tokenCond;
pthread_cond_t resumeCond;

typedef enum {
    TIGR_ALIVE,
    TIGR_PAUSED,
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

typedef enum {
    SET_WINDOW,
    RESET_WINDOW,
    SET_INPUT,
    RESET_INPUT,
    CLOSE,
    PAUSE,
} TigrMessage;

struct TigrMessageData {
    TigrMessage message;
    ANativeWindow* window;
    AInputQueue* queue;
};

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

void waitForResume() {
    pthread_mutex_lock(&tokenLock);
    pthread_cond_wait(&resumeCond, &tokenLock);
    pthread_mutex_unlock(&tokenLock);
}

void notifyResume() {
    pthread_mutex_lock(&tokenLock);
    tigrThreadState = TIGR_ALIVE;
    pthread_cond_broadcast(&resumeCond);
    pthread_mutex_unlock(&tokenLock);
}

void writeTigrMessage(TigrMessageData messageData) {
    if (tigrThreadState == TIGR_DEAD || tigrThreadState == TIGR_PAUSED) {
        LOGD("Skipping message %d to dead or paused thread", messageData.message);
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
    assert(activity == 0);
    activity = a;

    tigr_android_create();

    int fds[2];
    if (pipe(fds) != 0) {
        JNIGlue glue(activity->vm);
        glue.throwException("Failed to create communication pipe");
        return;
    }
    messageReadFd = fds[0];
    messageWriteFd = fds[1];

    pthread_mutex_init(&tokenLock, NULL);
    pthread_cond_init(&tokenCond, NULL);
    pthread_cond_init(&resumeCond, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(
        &tigrThread, &attr,
        [](void*) -> void* {
            looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
            ALooper_addFd(looper, messageReadFd, MAIN_ID, ALOOPER_EVENT_INPUT, nullptr, nullptr);

            JNIEnv* threadEnv;
            activity->vm->AttachCurrentThread(&threadEnv, nullptr);

            tigrThreadState = TIGR_ALIVE;

            tigrMain();

            activity->vm->DetachCurrentThread();

            if (inputQueue != 0) {
                AInputQueue_detachLooper(inputQueue);
            }

            pthread_mutex_lock(&tokenLock);
            if (tigrThreadState == TIGR_ALIVE) {
                tigrThreadState = TIGR_DYING;
                ANativeActivity_finish(activity);
            }
            pthread_mutex_unlock(&tokenLock);

            tigrThreadState = TIGR_DEAD;
            tigrThread = 0;

            return nullptr;
        },
        nullptr);

    pthread_setname_np(tigrThread, "TIGR render thread");
#ifndef NDEBUG
    pid_t threadID = pthread_gettid_np(tigrThread);
    LOGD("Render thread %d started", threadID);
#endif
}

void pauseTigr() {
    LOGD("Pausing");
    writeTigrMessage(TigrMessageData{
        .message = PAUSE,
    });
    LOGD("Paused");
}

void resumeTigr() {
    notifyResume();
}

void stopTigr() {
    LOGD("Stopping tigr");

    // Wake the tigr, if paused
    notifyResume();

    pthread_mutex_lock(&tokenLock);
    if (tigrThreadState == TIGR_ALIVE) {
        tigrThreadState = TIGR_DYING;
    }
    pthread_mutex_unlock(&tokenLock);

    if (tigrThread != 0) {
        writeTigrMessage(TigrMessageData{
            .message = CLOSE,
        });

        LOGD("Waiting for tigr thread");
        void* retval = 0;
        int res = pthread_join(tigrThread, &retval);
        if (res != 0) {
            LOGE("Failed to join: %s", strerror(res));
        }
        LOGD("Tigr is dead");
    }

    pthread_cond_destroy(&resumeCond);
    pthread_cond_destroy(&tokenCond);
    pthread_mutex_destroy(&tokenLock);
    close(messageReadFd);
    close(messageWriteFd);

    inputQueue = nullptr;
    looper = nullptr;
    activity = nullptr;
    window = nullptr;

    tigr_android_destroy();
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

/// TIGR interface, called from render thread ///

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
                case PAUSE: {
                    tigrThreadState = TIGR_PAUSED;

                    // Notify the main thread to let it continue
                    notifyMainThread();

                    // Make tigr stop using the current window
                    if (window != nullptr) {
                        eventHandler(
                            AndroidEvent{
                                .type = AE_WINDOW_DESTROYED,
                                .window = window,
                            },
                            userData);
                    }

                    // Now, wait for the _main_ thread to notify us.
                    // This pauses the render thread while we are paused.
                    waitForResume();

                    // Get "now" timestamp
                    struct timespec ts;
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    double now = (double)ts.tv_sec + (ts.tv_nsec / 1000000000.0);

                    // Make tigr resume "now"
                    eventHandler(
                        AndroidEvent{
                            .type = AE_RESUME,
                            .time = now,
                        },
                        userData);

                    // Make tigr use current window, if any
                    if (window != nullptr) {
                        eventHandler(
                            AndroidEvent{
                                .type = AE_WINDOW_CREATED,
                                .window = window,
                            },
                            userData);
                    }

                    break;
                }
                case CLOSE:
                    eventHandler(
                        AndroidEvent{
                            .type = AE_CLOSE,
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

extern "C" void* android_loadAsset(const char* filename, int* oLength) {
    LOGD("Loading asset \"%s\"", filename);
    char* assetBuffer = 0;

    AAsset* asset = AAssetManager_open(activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (asset == 0) {
        LOGI("Failed to load asset \"%s\"", filename);
        *oLength = 0;
        return assetBuffer;
    }

    const void* assetData = AAsset_getBuffer(asset);
    if (assetData != 0) {
        size_t length = AAsset_getLength(asset);
        assetBuffer = (char*)malloc(length + 1);
        if (assetBuffer != 0) {
            assetBuffer[length] = 0;
            memcpy(assetBuffer, assetData, length);
            *oLength = length;
        } else {
            LOGE("Failed to allocate asset data");
        }
    } else {
        LOGE("Failed to get asset buffer");
    }

    AAsset_close(asset);
    return assetBuffer;
}

extern "C" void tigrShowKeyboard(int show) {
    JavaVM* vm = activity->vm;
    JNIGlue glue(vm);
    
    jobject javaActivity = activity->clazz;

    glue.callVoidMethod(javaActivity, "showKeyboard", "(Z)V", show);
}
