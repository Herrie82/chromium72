// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/shell_switches.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"

#if !defined(USE_CBE)
#include "content/shell/common/layout_test/layout_test_switches.h"
#endif

namespace switches {

// Makes Content Shell use the given path for its data directory.
const char kContentShellDataPath[] = "data-path";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// Exposes the window.internals object to JavaScript for interactive development
// and debugging of layout tests that rely on it.
const char kExposeInternalsForTesting[] = "expose-internals-for-testing";

// Registers additional font files on Windows (for fonts outside the usual
// %WINDIR%\Fonts location). Multiple files can be used by separating them
// with a semicolon (;).
const char kRegisterFontFiles[] = "register-font-files";

// Size for the content_shell's host window (i.e. "800x600").
const char kContentShellHostWindowSize[] = "content-shell-host-window-size";

// Hides toolbar from content_shell's host window.
const char kContentShellHideToolbar[] = "content-shell-hide-toolbar";

std::vector<std::string> GetSideloadFontFiles() {
  std::vector<std::string> files;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRegisterFontFiles)) {
    files = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kRegisterFontFiles),
        ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  return files;
}

#if !defined(USE_CBE)
bool IsRunWebTestsSwitchPresent() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRunWebTests);
}
#endif

}  // namespace switches
