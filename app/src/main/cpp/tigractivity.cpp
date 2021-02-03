#include <android/native_activity.h>
#include "swappy/swappyGL.h"

#include "jniglue.h"
#include "tigrglue.h"
#include "log.h"

void setImmersiveMode(JavaVM* vm, jobject activity) {
    JNIGlue glue(vm);

    jobject window =
        glue.callObjectMethod("android/app/NativeActivity", "getWindow", "()Landroid/view/Window;", activity);

    jobject decorView = glue.callObjectMethod("android/view/Window", "getDecorView", "()Landroid/view/View;", window);

    int flag = glue.getStaticIntField("android/view/View", "SYSTEM_UI_FLAG_LAYOUT_STABLE") |
               glue.getStaticIntField("android/view/View", "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION") |
               glue.getStaticIntField("android/view/View", "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN") |
               glue.getStaticIntField("android/view/View", "SYSTEM_UI_FLAG_HIDE_NAVIGATION") |
               glue.getStaticIntField("android/view/View", "SYSTEM_UI_FLAG_FULLSCREEN") |
               glue.getStaticIntField("android/view/View", "SYSTEM_UI_FLAG_IMMERSIVE_STICKY");

    glue.callVoidMethod("android/view/View", "setSystemUiVisibility", "(I)V", decorView, flag);
}

static void onStart(ANativeActivity* activity) {
}

static void onResume(ANativeActivity* activity) {
    setImmersiveMode(activity->vm, activity->clazz);
}

static void* onSaveInstanceState(ANativeActivity* activity, size_t* outSize) {
    return NULL;
}

static void onPause(ANativeActivity* activity) {
}

static void onStop(ANativeActivity* activity) {
}

static void onDestroy(ANativeActivity* activity) {
    stopTigrThread();
    SwappyGL_destroy();
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused) {
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
    SwappyGL_setWindow(window);
    setTigrWindow(window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window) {
    resetTigrWindow(window);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue) {
    setTigrInputQueue(queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue) {
    resetTigrInputQueue(queue);
}

static void onConfigurationChanged(ANativeActivity* activity) {
}

static void onLowMemory(ANativeActivity* activity) {
}

JNIEXPORT
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
    activity->callbacks->onStart = onStart;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onStop = onStop;
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;
    activity->callbacks->onLowMemory = onLowMemory;
    activity->instance = 0;

    setImmersiveMode(activity->vm, activity->clazz);

    JNIEnv* env;
    activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    SwappyGL_init(env, activity->clazz);
    SwappyGL_setSwapIntervalNS(SWAPPY_SWAP_30FPS);

    startTigrThread();
}
