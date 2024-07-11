project(timogr)

cmake_minimum_required(VERSION 3.4.1)

find_package(games-frame-pacing REQUIRED CONFIG)

set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS}")
add_library(tigr STATIC ../../../../tigr/tigr.c)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wall -Werror")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(native-activity SHARED main.c tigractivity.cpp tigrglue.cpp)

target_include_directories(native-activity PRIVATE ../../../../tigr)

# add lib dependencies
target_link_libraries(native-activity
    tigr
    games-frame-pacing::swappy_static
    android
    EGL
    GLESv3
    log)
