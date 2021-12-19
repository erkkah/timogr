package main

import (
	"flag"
	"fmt"
	"io"
	"os"
	"path"
	"strings"
)

func fatal(message string) {
	fmt.Println(message)
	os.Exit(1)
}

type Options struct {
	Manifest     string
	Package      string
	JDWPPort     int
	DebuggerPort int
	Device       string
	SDKRoot      string
	Build        bool
	Debug        bool
}

func usage() {
	fmt.Fprintf(flag.CommandLine.Output(), "Usage: %s [options] <devices|emulators|emulator <id>|start|stop>\nOptions:\n", os.Args[0])
	flag.PrintDefaults()
}

func parseOptions() Options {
	var options Options

	flag.StringVar(&options.Manifest, "manifest", "app/src/main/AndroidManifest.xml", "Android manifest file")
	flag.StringVar(&options.Package, "package", "", "Application package")
	flag.StringVar(&options.SDKRoot, "sdkroot", "", "Android SDK root")
	flag.IntVar(&options.DebuggerPort, "port", 6666, "Debugger communication port")
	flag.IntVar(&options.JDWPPort, "jdwpport", 6667, "JDWP communication port")
	flag.StringVar(&options.Device, "device", "", "Device identifier")
	flag.BoolVar(&options.Build, "build", false, "Build before debug")
	flag.BoolVar(&options.Debug, "debug", true, "Start debug session")
	flag.Usage = usage
	flag.Parse()

	if options.SDKRoot == "" {
		found := false
		options.SDKRoot, found = os.LookupEnv("ANDROID_SDK_ROOT")
		if !found {
			options.SDKRoot, found = os.LookupEnv("ANDROID_HOME")
		}
		if !found {
			fatal("ANDROID_SDK_ROOT not set, and not specified using -sdkroot")
		}
	}

	return options
}

func main() {
	options := parseOptions()

	stopOnly := false

	switch flag.Arg(0) {
	case "devices":
		devices, err := ListDevices()
		if err != nil {
			fatal("Failed to list devices")
		}
		if len(devices) == 0 {
			fatal("No devices found")
		}
		for _, d := range devices {
			id := strings.Split(d, " ")[0]
			fmt.Printf("%v\n", id)
		}
		os.Exit(0)
	case "emulators":
		emulators, err := ListEmulators(options.SDKRoot)
		if err != nil {
			fatal("Failed to list emulators")
		}
		for _, e := range emulators {
			if e != "" {
				fmt.Println(e)
			}
		}
		os.Exit(0)
	case "emulator":
		emulator := flag.Arg(1)
		if emulator == "" {
			emulators, err := ListEmulators(options.SDKRoot)
			if err != nil {
				fatal("Failed to list emulators")
			}
			if len(emulators) == 0 {
				fatal("No emulators found")
			}
			emulator = emulators[0]
		}
		err := LaunchEmulator(options.SDKRoot, emulator)
		if err != nil {
			fatal(fmt.Sprintf("Failed to launch emulator (output from emulator tool):\n%v", err))
		}
		os.Exit(0)
	case "stop":
		stopOnly = true
		break
	case "start":
		break
	default:
		usage()
		os.Exit(1)
	}

	if options.Build {
		fmt.Println("Building...")
		err := buildAndInstall()
		if err != nil {
			fatal(fmt.Sprintf("Build failed: %v", err))
		}
	}

	manifest, err := parseManifest(options.Manifest)
	if err != nil {
		fatal("Failed to parse manifest")
	}
	if options.Package == "" {
		options.Package, err = getApplicationID()
		if err != nil {
			fatal("Failed to get package using gradle")
		}
	}
	if options.Package == "" {
		fatal("Application package not specified")
	}

	devices, err := ListDevices()
	if err != nil {
		fatal("Failed to list devices")
	}

	if len(devices) == 0 {
		fatal("No device or simulator found")
	}

	device := devices[0]
	if options.Device == "" {
		fmt.Printf("No device specified, using first found device\n")
	} else {
		foundIndex := -1
		for i, d := range devices {
			if strings.HasPrefix(d, options.Device) {
				foundIndex = i
				device = d
				break
			}
		}
		if foundIndex < 0 {
			fatal(fmt.Sprintf("Specified device %q not found\n", options.Device))
		}
	}

	adb := NewADB(device, options.Package)

	fmt.Printf("Connecting to %q\n", adb.Model())

	adb.KillOldProcesses()

	if stopOnly {
		os.Exit(0)
	}

	arch, err := adb.Arch()
	if err != nil {
		fatal(fmt.Sprintf("Failed to get device arch: %v", err))
	}

	fmt.Println("Starting application")
	if err = adb.StartApplication(manifest.mainActivityIntent(), options.Debug); err != nil {
		fatal(fmt.Sprintf("Failed to start application: %v", err))
	}

	fmt.Printf("Application running with PID %v\n", adb.PID)

	if !options.Debug {
		os.Exit(0)
	}

	server := findDebugServer(options.SDKRoot, arch)
	fmt.Printf("Using debug server at %q\n", server)

	packageHome := fmt.Sprintf("/data/data/%s", options.Package)
	if err = adb.InstallFile(server, fmt.Sprintf("%s/lldb/bin", packageHome)); err != nil {
		fatal("Failed to install debug server")
	}

	if err = collectDebugBinaries(adb); err != nil {
		fatal("Failed to collect debug binaries")
	}

	debugSocket := fmt.Sprintf("/%v/debug.sock", options.Package)
	if err = setupForwards(adb, options.DebuggerPort, options.JDWPPort, debugSocket); err != nil {
		fatal("Failed to setup port forwards")
	}

	fmt.Printf("Connecting to java debug server at %v\n", options.JDWPPort)
	jDebugger := NewJDWP(options.JDWPPort)
	if err = jDebugger.Connect(); err != nil {
		fatal("Failed to connect to java debug server")
	}
	defer jDebugger.Close()

	fmt.Printf("Starting debug server\n")
	serverError := startDebugServer(adb, debugSocket)

	if err = jDebugger.Resume(); err != nil {
		fatal("Failed to resume VM")
	}

	fmt.Println("Ready for debugger")

	tailError := adb.TailLog()

	select {
	case err = <-serverError:
		if err != nil {
			fatal("Debug server error")
		}
	case err = <-tailError:
		if err != nil {
			fatal("Logcat error")
		}
	}

	fmt.Println("Closing debug session")
	adb.KillOldProcesses()
}

func startDebugServer(adb *ADB, socket string) <-chan error {
	errors := make(chan error)
	go func() {
		_, err := adb.remote("mkdir -p lldb/log && cat /dev/null > lldb/log/gdbserver.log")
		if err == nil {
			_, err = adb.remote(fmt.Sprintf("lldb/bin/lldb-server gdbserver unix-abstract://%s "+
				"--native-regs --attach %v --log-file lldb/log/gdbserver.log --log-channels %q",
				socket, adb.PID, "lldb process:gdb-remote"))
		}
		errors <- err
		close(errors)
	}()
	return errors
}

func setupForwards(adb *ADB, port int, jPort int, socket string) error {
	var err error

	if _, err = adb.command("forward", "--remove-all"); err != nil {
		return err
	}
	if _, err = adb.command("forward", fmt.Sprintf("tcp:%v", port), fmt.Sprintf("localabstract:%v", socket)); err != nil {
		return err
	}
	if _, err = adb.command("forward", fmt.Sprintf("tcp:%v", jPort), fmt.Sprintf("jdwp:%v", adb.PID)); err != nil {
		return err
	}
	return nil
}

func collectDebugBinaries(adb *ADB) error {
	err := os.MkdirAll(".debug", 775)
	if err != nil {
		return err
	}

	nativeLib, err := findNativeLibrary(adb)
	err = copyFile(nativeLib, path.Join(".debug", "libnative-activity.so"))
	if err != nil {
		return err
	}

	nativeBin, err := findNativeBinary(adb)
	if err != nil {
		return err
	}

	err = adb.pull(nativeBin, path.Join(".debug", "exe"))

	return err
}

func findNativeBinary(adb *ADB) (string, error) {
	output, err := adb.remote(fmt.Sprintf("ls -l /proc/%v/exe", adb.PID))
	if err != nil {
		return "", err
	}
	binary := strings.Split(output, ">")[1]
	return strings.TrimSpace(binary), nil
}

func findNativeLibrary(adb *ADB) (string, error) {
	output, err := adb.remote("getprop ro.product.cpu.abi")
	if err != nil {
		return "", err
	}
	abi := strings.TrimSpace(output)

	expectedSuffix := fmt.Sprintf("/%s/libnative-activity.so", abi)
	return findPath("app/build/intermediates/cmake/debug", expectedSuffix), nil
}

func copyFile(src string, dest string) error {
	sourceFile, err := os.Open(src)
	if err != nil {
		return err
	}
	destFile, err := os.OpenFile(dest, os.O_CREATE|os.O_WRONLY, 664)
	if err != nil {
		return err
	}
	_, err = io.Copy(destFile, sourceFile)
	return err
}
