package main

import (
	"encoding/xml"
	"fmt"
	"io/fs"
	"io/ioutil"
	"path"
	"path/filepath"
	"strings"
	"time"
)

func findDebugServer(SDKRoot string, arch string) string {
	return findPath(SDKRoot, path.Join(arch, "lldb-server"))
}

func findPath(root string, pathSuffix string) string {
	newest := time.Unix(0, 0)
	found := ""

	filepath.WalkDir(root, func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if !entry.IsDir() && strings.HasSuffix(path, pathSuffix) {
			info, _ := entry.Info()
			if info.ModTime().After(newest) {
				found = path
				newest = info.ModTime()
			}
		}
		return nil
	})

	return found
}

func getApplicationID() (string, error) {
	output, err := runGradleCommand("printAppID")
	if err != nil {
		return "", err
	}
	lines := strings.Split(string(output), "\n")
	appID := ""
	for _, line := range lines {
		if strings.HasPrefix(line, "APPID:") {
			appID = strings.TrimSpace(strings.TrimPrefix(line, "APPID:"))
			break
		}
	}
	return appID, nil
}

func buildAndInstall() error {
	_, err := runGradleCommand("installDebug")
	return err
}

func runGradleCommand(args ...string) (string, error) {
	// ??? Fix windows case!
	output, err := runCommand("./gradlew", args...)
	if err != nil {
		return "", fmt.Errorf("%v", string(output))
	}
	return output, nil
}

type AndroidManifest struct {
	Package     string `xml:"package,attr"`
	Application struct {
		Activity struct {
			Name string `xml:"name,attr"`
		} `xml:"activity"`
	} `xml:"application"`
}

func (m *AndroidManifest) mainActivityIntent() string {
	activityName := m.Application.Activity.Name
	if strings.HasPrefix(activityName, ".") {
		activityName = m.Package + activityName
	}
	return activityName
}

func parseManifest(manifest string) (*AndroidManifest, error) {
	bytes, err := ioutil.ReadFile(manifest)
	if err != nil {
		return nil, err
	}
	var parsed AndroidManifest
	err = xml.Unmarshal(bytes, &parsed)
	if err != nil {
		return nil, err
	}
	return &parsed, nil
}
