package main

import (
	"io/fs"
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
