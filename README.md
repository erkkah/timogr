# TIMOGR - TIny MObile GRaphics starter template

![](./app/src/main/assets/timogr.png)

TIMOGR is a tiny Android starter template for creating fullscreen 2D apps using the
[TIGR](https://github.com/erkkah/tigr) library.

Like TIGR, TIMOGR is designed to be small and mostly independent.
The "hello world" compressed arm64-v8a release bundle is less than 350kB.

TIMOGR takes care of the Android life-cycle stuff and lets you write the fun part of your little app.
Oh, and you don't need Android Studio - it works just fine with the plain Android SDK + NDK.

## Getting started

* [Generate](https://github.com/erkkah/timogr/generate) your own repo based on this template
* Clone your new repo to get the files to your machine
* Install Android SDK (or Android Studio)
* If not already in place, install the Android NDK (Use sdkmanager or Android Studio)
* Make sure `ANDROID_SDK_ROOT` is set, and `JAVA_HOME` if gradle cannot find the correct version below.
* Run a test build: `./gradlew build` (that's `gradlew.bat build` on Windows)
* Tweak `app/build/build.gradle` if the test build complains about `compileSdkVersion` or `ndkVersion`
* Plug in your device (or run an emulator), and run `./gradlew installDebug` to install the template TIMOGR app
* Run the app (and report back if there are problems)

Now, replace the meat of `app/src/main/cpp/main.c` with your TIGR code, and you're done.

Just like with desktop TIGR, the code can be as tiny as this:

```C
#include "tigr.h"

void tigrMain()
{
    Tigr *screen = tigrWindow(320, 240, "Hello", TIGR_4X);
    while (!tigrClosed(screen))
    {
        tigrClear(screen, tigrRGB(0x80, 0x90, 0xa0));
        tigrPrint(screen, tfont, 120, 110, tigrRGB(0xff, 0xff, 0xff), "Hello, world.");
        tigrUpdate(screen);
    }
    tigrFree(screen);
}
```

## Some details and pointers

### Input
* Keyboard input using the virtual keyboard is supported. Use `tigrShowKeyboard()` to show or hide the keyboard.
* The "back" button produces a TK_ESCAPE keypress.
* The "buttons" reported by `tigrMouse` is the number of detected touch points, the position is always of the last triggered touch point.
* Use `tigrTouch` to process multi-touch input.

### Threads and extending the Android side
The `tigrMain` entry point runs on a rendering thread separate from the Activity main thread. If you need to do more Android specifics, check out the Activity implementation in `tigractivity.cpp`.

### Files and assets
Reading "files" by using `tigrLoadImage` for example, reads from the corresponding asset path. Putting `image.png` at `app/src/main/assets` will make it loadable from `/image.png`.

Writing files (`tigrSaveImage`) will write to the given path. TIMOGR will not redirect writes to the app internal storage.

### C/C++
You can of course replace main.c with main.cpp if you want. Just update `CMakeLists.txt` and declare `tigrMain` as `extern "C"`:

```C++
extern "C" void tigrMain() {
    // ...
}
```

### Building a "real" app
You can make a distributable app based on TIMOGR. There is plenty of information of the steps involved on the [Android developer site](https://developer.android.com/studio/publish). Start by changing the application ID in `app/build.gradle`.

The Android developer site also describes how to [sign your app from the command line](https://developer.android.com/studio/build/building-cmdline#sign_cmdline) or by using Gradle.

### Adding more activities
There can be only one instance of the TIMOGR native activity. If you need another instance, you could theoretically add another native library target in `CMakeLists.txt` and refer to that in the second activity declaration in `AndroidManifest.xml`.
