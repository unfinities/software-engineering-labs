// Copyright 2026 CampusMarket contributors
#include "market/security.h"

#include <windows.h>
// Windows SDK crypto headers depend on the Win32 base types.
// clang-format off
#include <bcrypt.h>
#include <wincrypt.h>
// clang-format on

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "market/database.h"

namespace market {
namespace {
void CryptoCheck(bool okay) {
  if (!okay) {
    throw Error(ErrorKind::kStorage, "系统加密服务不可用");
  }
}
std::string Hex(const unsigned char* data, size_t size) {
  const char* digits = "0123456789abcdef";
  std::string result;
  for (size_t i = 0; i < size; ++i) {
    result += digits[data[i] >> 4];
    result += digits[data[i] & 15];
  }
  return result;
}
std::vector<unsigned char> Unhex(std::string_view text) {
  CryptoCheck(text.size() % 2 == 0);
  std::vector<unsigned char> result;
  for (size_t i = 0; i < text.size(); i += 2) {
    unsigned int value = 0;
    std::istringstream stream(std::string(text.substr(i, 2)));
    stream >> std::hex >> value;
    CryptoCheck(!stream.fail());
    result.push_back(static_cast<unsigned char>(value));
  }
  return result;
}
class Algorithm {
 public:
  explicit Algorithm(ULONG flags) {
    CryptoCheck(BCryptOpenAlgorithmProvider(&value, BCRYPT_SHA256_ALGORITHM, nullptr, flags) == 0);
  }
  ~Algorithm() { BCryptCloseAlgorithmProvider(value, 0); }
  BCRYPT_ALG_HANDLE value = nullptr;
};
}  // namespace

std::string RandomId() {
  unsigned char bytes[24];
  CryptoCheck(BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0);
  return Hex(bytes, sizeof(bytes));
}

std::string Digest(std::string_view input) {
  Algorithm algorithm(0);
  unsigned char output[32];
  CryptoCheck(BCryptHash(algorithm.value, nullptr, 0,
                         reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
                         static_cast<ULONG>(input.size()), output, 32) == 0);
  return Hex(output, 32);
}

std::string HashPassword(std::string_view password, std::string_view salt) {
  Algorithm algorithm(BCRYPT_ALG_HANDLE_HMAC_FLAG);
  unsigned char output[32];
  CryptoCheck(BCryptDeriveKeyPBKDF2(algorithm.value,
                                    reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
                                    static_cast<ULONG>(password.size()),
                                    reinterpret_cast<PUCHAR>(const_cast<char*>(salt.data())),
                                    static_cast<ULONG>(salt.size()), 600000, output, 32, 0) == 0);
  return Hex(output, 32);
}

bool ConstantEqual(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  unsigned char difference = 0;
  for (size_t i = 0; i < left.size(); ++i) {
    difference |= left[i] ^ right[i];
  }
  return difference == 0;
}

std::string Protect(std::string_view input) {
  DATA_BLOB source{static_cast<DWORD>(input.size()),
                   reinterpret_cast<BYTE*>(const_cast<char*>(input.data()))};
  DATA_BLOB output{};
  CryptoCheck(CryptProtectData(&source, L"CampusMarket contact", nullptr, nullptr, nullptr,
                               CRYPTPROTECT_UI_FORBIDDEN, &output));
  std::string result = Hex(output.pbData, output.cbData);
  LocalFree(output.pbData);
  return result;
}

std::string Unprotect(const std::string& input) {
  if (input.empty()) {
    return "";
  }
  auto bytes = Unhex(input);
  DATA_BLOB source{static_cast<DWORD>(bytes.size()), bytes.data()}, output{};
  CryptoCheck(CryptUnprotectData(&source, nullptr, nullptr, nullptr, nullptr,
                                 CRYPTPROTECT_UI_FORBIDDEN, &output));
  std::string result(reinterpret_cast<char*>(output.pbData), output.cbData);
  SecureZeroMemory(output.pbData, output.cbData);
  LocalFree(output.pbData);
  return result;
}

std::wstring Wide(const std::string& text) {
  if (text.empty()) {
    return L"";
  }
  int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0);
  CryptoCheck(size > 0);
  std::wstring result(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
  return result;
}

std::string Utf8(const std::wstring& text) {
  if (text.empty()) {
    return "";
  }
  int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                 nullptr, nullptr);
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size,
                      nullptr, nullptr);
  return result;
}

int64_t Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

int64_t ParseMoney(const std::string& input) {
  if (input.empty() || input.size() > 8) {
    throw Error(ErrorKind::kInput, "价格须为 0.01 至 99999.99 元，最多两位小数");
  }
  int64_t whole = 0, fraction = 0;
  int decimals = -1;
  bool digit = false;
  for (char ch : input) {
    if (ch == '.' && decimals == -1 && digit) {
      decimals = 0;
      continue;
    }
    if (ch < '0' || ch > '9' || decimals >= 2) {
      throw Error(ErrorKind::kInput, "价格格式错误，最多两位小数");
    }
    digit = true;
    if (decimals < 0) {
      whole = whole * 10 + ch - '0';
    } else {
      fraction = fraction * 10 + ch - '0';
      ++decimals;
    }
  }
  if (decimals == 0) {
    throw Error(ErrorKind::kInput, "小数点后须填写数字");
  }
  if (decimals == 1) {
    fraction *= 10;
  }
  int64_t value = whole * 100 + fraction;
  if (value < 1 || value > 9999999) {
    throw Error(ErrorKind::kInput, "价格超出 0.01 至 99999.99 元范围");
  }
  return value;
}

std::string Money(int64_t fen) {
  std::ostringstream out;
  out << fen / 100 << '.' << std::setw(2) << std::setfill('0') << fen % 100;
  return out.str();
}

int64_t DateStart(const std::string& date) {
  if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
    throw Error(ErrorKind::kInput, "日期格式应为 YYYY-MM-DD");
  }
  for (size_t i = 0; i < date.size(); ++i) {
    if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) {
      throw Error(ErrorKind::kInput, "日期含非法字符");
    }
  }
  int year = std::stoi(date.substr(0, 4));
  unsigned month = std::stoul(date.substr(5, 2));
  unsigned day = std::stoul(date.substr(8, 2));
  std::chrono::year_month_day value{std::chrono::year(year), std::chrono::month(month),
                                    std::chrono::day(day)};
  if (year < 1970 || year > 2100 || !value.ok()) {
    throw Error(ErrorKind::kInput, "日期无效，年份应在 1970 至 2100 之间");
  }
  auto seconds = std::chrono::sys_days(value).time_since_epoch();
  return std::chrono::duration_cast<std::chrono::seconds>(seconds).count() - 28800;
}

std::string BeijingTime(int64_t epoch) {
  time_t adjusted = static_cast<time_t>(epoch + 28800);
  tm local{};
  gmtime_s(&local, &adjusted);
  std::ostringstream out;
  out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
  return out.str();
}
std::string Today() { return BeijingTime(Now()).substr(0, 10); }
void AppendFailure(const std::filesystem::path& path, const std::string& line) {
  HANDLE file =
      CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    throw Error(ErrorKind::kStorage, "业务已拒绝，但故障日志也无法写入，请检查数据目录权限");
  }
  DWORD written = 0;
  bool okay = WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
  CloseHandle(file);
  if (!okay || written != line.size()) {
    throw Error(ErrorKind::kStorage, "故障日志未能完整保存，请检查磁盘");
  }
}
}  // namespace market
