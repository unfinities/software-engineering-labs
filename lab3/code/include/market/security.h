// Copyright 2026 CampusMarket contributors
#ifndef LAB3_CODE_INCLUDE_MARKET_SECURITY_H_
#define LAB3_CODE_INCLUDE_MARKET_SECURITY_H_
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace market {
std::string RandomId();
std::string Digest(std::string_view input);
std::string HashPassword(std::string_view password, std::string_view salt);
bool ConstantEqual(std::string_view left, std::string_view right);
std::string Protect(std::string_view input);
std::string Unprotect(const std::string& input);
std::wstring Wide(const std::string& text);
std::string Utf8(const std::wstring& text);
int64_t Now();
int64_t ParseMoney(const std::string& input);
std::string Money(int64_t fen);
int64_t DateStart(const std::string& date);
std::string BeijingTime(int64_t epoch);
std::string Today();
void AppendFailure(const std::filesystem::path& path, const std::string& line);
}  // namespace market
#endif  // LAB3_CODE_INCLUDE_MARKET_SECURITY_H_
