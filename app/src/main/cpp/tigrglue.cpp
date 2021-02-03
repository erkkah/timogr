#include "tigrglue.h"

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <android/looper.h>

#include "swappy/swappyGL.h"

#include "log.h"

extern "C" void tigrMain();

namespace {

typedef enum {
    MAIN_ID,
    INPUT_ID,
} LooperID;

pthread_t tigrThread;
pthread_mutex_t tokenLock;
pthread_cond_t tokenCond;

int messageReadFd;
int messageWriteFd;

AInputQueue* inputQueue;
ALooper* looper;

void waitForTigrThread() {
    pthread_mutex_lock(&tokenLock);
    pthread_cond_wait(&tokenCond, &tokenLock);
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

void startTigrThread() {
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

            tigrMain();

            return nullptr;
        },
        nullptr);
}

void stopTigrThread() {
    writeTigrMessage(TigrMessageData{
        .message = DESTROY,
    });
}

void setTigrWindow(ANativeWindow* window) {
    writeTigrMessage(TigrMessageData{
        .message = SET_WINDOW,
        .window = window,
    });
}

void resetTigrWindow(ANativeWindow* window) {
    writeTigrMessage(TigrMessageData{
        .message = RESET_WINDOW,
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
