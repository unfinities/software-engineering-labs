// Copyright 2026 CampusMarket contributors
#ifndef LAB3_CODE_INCLUDE_MARKET_UI_H_
#define LAB3_CODE_INCLUDE_MARKET_UI_H_
#include <windows.h>

#include <filesystem>
namespace market {
int RunApplication(HINSTANCE instance, const std::filesystem::path& directory);
}
#endif  // LAB3_CODE_INCLUDE_MARKET_UI_H_
