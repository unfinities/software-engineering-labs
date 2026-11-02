// Copyright 2026 CampusMarket contributors
// clang-format off
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
// clang-format on

#include <filesystem>

#include "market/security.h"
#include "market/ui.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  SetProcessDPIAware();
  try {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
      return 1;
    }
    std::filesystem::path directory = std::filesystem::path(local) / L"CampusMarket";
    CoTaskMemFree(local);
#ifdef MARKET_UI_SMOKE
    wchar_t executable[MAX_PATH];
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    directory = std::filesystem::path(executable).parent_path() / L"ui-test-data";
#endif
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (count == 3 && std::wstring(args[1]) == L"--data-dir") {
      directory = args[2];
    }
    LocalFree(args);
    return market::RunApplication(instance, directory);
  } catch (const std::exception& error) {
    MessageBoxW(nullptr, market::Wide(error.what()).c_str(), L"启动失败", MB_ICONERROR);
    return 1;
  }
}
