// Copyright 2026 CampusMarket contributors
#ifndef LAB3_CODE_INCLUDE_MARKET_DATABASE_H_
#define LAB3_CODE_INCLUDE_MARKET_DATABASE_H_

#include <sqlite3.h>

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace market {
using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;
enum class ErrorKind { kInput, kAuth, kForbidden, kConflict, kStorage, kProcessing };

class Error : public std::runtime_error {
 public:
  Error(ErrorKind kind, const std::string& message) : std::runtime_error(message), kind(kind) {}
  ErrorKind kind;
};

class Database {
 public:
  explicit Database(const std::filesystem::path& path);
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Rows Query(const std::string& sql, const std::vector<std::string>& args = {});
  void Exec(const std::string& sql, const std::vector<std::string>& args = {});
  void Script(const std::string& sql);
  int Changes() const;

 private:
  sqlite3* handle_ = nullptr;
};

class Transaction {
 public:
  explicit Transaction(Database* db);
  ~Transaction();
  void Commit();
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

 private:
  Database* db_;
  bool committed_ = false;
};
}  // namespace market
#endif  // LAB3_CODE_INCLUDE_MARKET_DATABASE_H_
