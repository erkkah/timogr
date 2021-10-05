#!/bin/bash

# Preliminary timogr debug script

[[ -z "${ANDROID_SDK_ROOT:=$ANDROID_HOME}" ]] && (echo "Set ANDROID_SDK_ROOT" && exit 1)

firstDevice() {
    adb devices -l | tail +2 | head -1 | cut -f1
}

extractDeviceField() {
    local deviceLine=$1
    local field=$2

    local result=${deviceLine/*${field}:/}
    echo ${result/ */}
}

getDeviceArch() {
    local transport=$1
    adb -t $transport shell uname -m
}

findDebugServer() {
    local arch=$1
    ls -t $(find $ANDROID_SDK_ROOT/ndk -path \*/${arch}/lldb-server) | tail -1
}

findLLDB() {
    ls -t $(find $ANDROID_SDK_ROOT/ndk -name lldb.sh) | tail -1
}

remote() {
    $RSH "sh -c \"$*\""
}

findNativeLib() {
    echo /$(remote cat /proc/$PID/maps | grep native-activity.so | head -1 | cut -d/ -f2-)
}

PACKAGE=com.example.timogr
DEVICE=$(firstDevice)

if [[ -z "$DEVICE" ]]; then
echo No device or simulator found
exit 1
fi

MODEL=$(extractDeviceField "$DEVICE" model)
TRANSPORT_ID=$(extractDeviceField "$DEVICE" transport_id)

echo connecting to $MODEL
ARCH=$(getDeviceArch $TRANSPORT_ID)
SERVER=$(findDebugServer $ARCH)
ADB="adb -t $TRANSPORT_ID"
RSH="$ADB shell run-as $PACKAGE"

#$ADB shell am set-debug-app -w $PACKAGE
$ADB shell am start-activity -S -W $PACKAGE/android.app.NativeActivity
PID=$(remote ps | grep $PACKAGE | awk '{print $2}')

mkdir -p .debug
$ADB pull $(findNativeLib) .debug

$ADB push $SERVER /data/local/tmp

PACKAGE_HOME=$($RSH pwd)
SOCKET_FILE=${PACKAGE_HOME}/debug.sock
remote rm -f $SOCKET_FILE

$ADB forward --remove-all
$ADB forward tcp:6666 localfilesystem:$SOCKET_FILE
#$ADB forward tcp:6667 jdwp:$PID

cat <<EOF > .debug/setup.lldb
gdb-remote 6666
add-dsym .debug/libnative-activity.so
EOF

remote "killall lldb-server &> /dev/null"
remote "cat /data/local/tmp/lldb-server > $PACKAGE_HOME/lldb-server"
remote chmod 777 lldb-server
remote $PACKAGE_HOME/lldb-server gdbserver unix://$SOCKET_FILE --attach $PID &

$(findLLDB) -s .debug/setup.lldb


# https://www.rojtberg.net/465/debugging-native-code-with-ndk-gdb-using-standalone-cmake-toolchain/
# https://android.googlesource.com/platform/system/core/+/android-2.3.4_r1/adb/jdwp_service.c
# https://titanwolf.org/Network/Articles/Article?AID=bf4e20d6-fa06-4939-baa1-a09851c29a93#gsc.tab=0
# https://www.sh-zam.com/2019/05/debugging-krita-on-android.html
# jdb -attach localhost:6666 <<<"quit"
