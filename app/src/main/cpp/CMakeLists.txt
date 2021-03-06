#
# Copyright (C) The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

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
