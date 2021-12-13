package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path"
	"strings"
	"sync"
	"time"
)

func ListDevices() ([]string, error) {
	output, err := runADBCommand("devices", "-l")
	if err != nil {
		return nil, err
	}

	lines := strings.Split(output, "\n")

	var deviceList []string
	for _, line := range lines[1:] {
		line = strings.TrimSpace(line)
		if len(line) > 0 {
			deviceList = append(deviceList, line)
		}
	}
	return deviceList, nil
}

func findEmulatorTool(SDKRoot string) (string, error) {
	emulatorPath := path.Join(SDKRoot, "emulator", "emulator")
	if _, err := os.Stat(emulatorPath); err == nil {
		return emulatorPath, nil
	}
	return "", fmt.Errorf("emulator tool not found")
}

func ListEmulators(SDKRoot string) ([]string, error) {
	emulatorPath, err := findEmulatorTool(SDKRoot)
	if err != nil {
		return nil, err
	}
	output, err := runCommand(emulatorPath, "-list-avds")
	if err != nil {
		return nil, err
	}
	return strings.Split(output, "\n"), nil
}

func LaunchEmulator(SDKRoot string, emulator string) error {
	emulatorPath, err := findEmulatorTool(SDKRoot)
	if err != nil {
		return err
	}
	err = startCommand(emulatorPath, "-avd", emulator, "-dns-server", "8.8.8.8")
	if err != nil {
		return fmt.Errorf("Failed to launch emulator: %w", err)
	}
	return nil
}

func extractDeviceField(deviceLine string, field string) string {
	fieldPos := strings.Index(deviceLine, field+":")
	if fieldPos == -1 {
		return ""
	}

	fieldStart := deviceLine[fieldPos:]
	fieldValue := strings.Split(fieldStart, ":")[1]
	fieldValue = strings.Split(fieldValue, " ")[0]
	return fieldValue
}

type ADB struct {
	appPackage  string
	model       string
	transportID string
	PID         string
}

func NewADB(device string, appPackage string) *ADB {
	model := extractDeviceField(device, "model")
	transportID := extractDeviceField(device, "transport_id")

	return &ADB{
		appPackage:  appPackage,
		model:       model,
		transportID: transportID,
	}
}

func (adb *ADB) Arch() (string, error) {
	output, err := adb.shell("uname", "-m")
	return strings.TrimSpace(output), err
}

func (adb *ADB) Model() string {
	return adb.model
}

func (adb *ADB) KillOldProcesses() {
	adb.remote("killall lldb-server")
	adb.remote(fmt.Sprintf("killall %s", adb.appPackage))
}

func (adb *ADB) waitForPID() (string, error) {
	timeout := time.After(time.Second * 5)

	for {
		processList, err := adb.remote("ps")
		if err != nil {
			return "", err
		}
		processLines := strings.Split(processList, "\n")
		for _, line := range processLines {
			if strings.Contains(line, adb.appPackage) {
				columns := strings.Fields(line)
				return columns[1], nil
			}
		}
		select {
		case <-time.After(time.Millisecond * 100):
			break
		case <-timeout:
			return "", errors.New("Timeout")
		}
	}
}

func (adb *ADB) StartApplication(intent string, debug bool) error {
	debugArg := ""
	if debug {
		debugArg = "-D"
	}
	output, err := adb.shell(fmt.Sprintf("am start-activity -S %s %s/%s", debugArg, adb.appPackage, intent))
	if err != nil {
		return fmt.Errorf("%s, %w", string(output), err)
	}
	adb.PID, err = adb.waitForPID()
	return err
}

func (adb *ADB) TailLog() <-chan error {
	errors := make(chan error)
	cmd := exec.Command("adb", "-t", adb.transportID, "logcat", fmt.Sprintf("--pid=%v", adb.PID))
	stderr, err := cmd.StderrPipe()
	if err != nil {
		errors <- err
		return errors
	}
	stdin, err := cmd.StdoutPipe()
	if err != nil {
		errors <- err
		return errors
	}
	var wg sync.WaitGroup
	wg.Add(2)

	go func() {
		buf := make([]byte, 1024)
		for {
			bytesRead, err := stderr.Read(buf)
			if err != nil {
				break
			}
			fmt.Print(string(buf[0:bytesRead]))
		}
		wg.Done()
	}()
	go func() {
		buf := make([]byte, 1024)
		for {
			bytesRead, err := stdin.Read(buf)
			if err != nil {
				break
			}
			fmt.Print(string(buf[0:bytesRead]))
		}
		wg.Done()
	}()

	go func() {
		err = cmd.Start()
		if err != nil {
			errors <- err
			return
		}
		wg.Wait()
		err = cmd.Wait()
		if err != nil {
			errors <- err
		}
	}()

	return errors
}

func (adb *ADB) InstallFile(src string, destDir string) error {
	var err error

	result, err := adb.remote(fmt.Sprintf("mkdir -p %s", destDir))
	if err != nil {
		return fmt.Errorf("failed to create dir: %s", result)
	}

	fileName := path.Base(src)
	tmp := "/data/local/tmp"

	err = adb.push(src, tmp)
	if err != nil {
		return err
	}
	result, err = adb.remote(fmt.Sprintf("cat %s/%s > %s/%s", tmp, fileName, destDir, fileName))
	if err != nil {
		return fmt.Errorf("failed to copy file: %s", result)
	}

	_, err = adb.remote(fmt.Sprintf("chmod 777 %s/%s", destDir, fileName))
	return err
}

func (adb *ADB) push(src string, dest string) error {
	_, err := adb.command("push", src, dest)
	return err
}

func (adb *ADB) pull(src string, dest string) error {
	_, err := adb.command("pull", src, dest)
	return err
}

func (adb *ADB) remote(commandString string) (string, error) {
	return adb.shell("run-as", adb.appPackage, fmt.Sprintf("sh -c %q", commandString))
}

func (adb *ADB) shell(commands ...string) (string, error) {
	return adb.command(append([]string{"shell"}, commands...)...)
}

func (adb *ADB) command(args ...string) (string, error) {
	return runADBCommand(append([]string{"-t", adb.transportID}, args...)...)
}

func (adb *ADB) packageHome() string {
	return "/data/data/" + adb.appPackage
}

func runADBCommand(args ...string) (string, error) {
	return runCommand("adb", args...)
}

func runCommand(command string, args ...string) (string, error) {
	cmd := exec.Command(command, args...)
	var output []byte
	output, err := cmd.CombinedOutput()
	return string(output), err
}

func startCommand(command string, args ...string) error {
	cmd := exec.Command(command, args...)
	return cmd.Start()
}
