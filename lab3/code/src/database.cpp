// Copyright 2026 CampusMarket contributors
#include "market/database.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace market {
namespace {
void Check(int code) {
  if (code == SQLITE_BUSY || code == SQLITE_LOCKED) {
    throw Error(ErrorKind::kProcessing, "操作处理中，请稍后使用同一请求重试");
  }
  if (code != SQLITE_OK && code != SQLITE_DONE && code != SQLITE_ROW) {
    throw Error(ErrorKind::kStorage, "保存或读取失败，业务未提交，请稍后重试");
  }
}
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
}  // namespace

Database::Database(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  const auto name = path.u8string();
  int code =
      sqlite3_open_v2(reinterpret_cast<const char*>(name.c_str()), &handle_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
  if (code != SQLITE_OK) {
    if (handle_) {
      sqlite3_close(handle_);
    }
    handle_ = nullptr;
    Check(code);
  }
  sqlite3_busy_timeout(handle_, 5000);
  Script("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;");
}

Database::~Database() {
  if (handle_) {
    sqlite3_close(handle_);
  }
}

Rows Database::Query(const std::string& sql, const std::vector<std::string>& args) {
  sqlite3_stmt* raw = nullptr;
  Check(sqlite3_prepare_v2(handle_, sql.c_str(), -1, &raw, nullptr));
  Statement statement(raw, sqlite3_finalize);
  if (sqlite3_bind_parameter_count(raw) != static_cast<int>(args.size())) {
    throw Error(ErrorKind::kStorage, "查询参数数量不匹配");
  }
  for (size_t i = 0; i < args.size(); ++i) {
    Check(sqlite3_bind_text(raw, static_cast<int>(i + 1), args[i].data(),
                            static_cast<int>(args[i].size()), SQLITE_TRANSIENT));
  }
  Rows rows;
  for (;;) {
    int code = sqlite3_step(raw);
    Check(code);
    if (code == SQLITE_DONE) {
      break;
    }
    Row row;
    for (int i = 0; i < sqlite3_column_count(raw); ++i) {
      const auto* value = sqlite3_column_text(raw, i);
      row[sqlite3_column_name(raw, i)] = value ? reinterpret_cast<const char*>(value) : "";
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

void Database::Exec(const std::string& sql, const std::vector<std::string>& args) {
  Query(sql, args);
}

void Database::Script(const std::string& sql) {
  Check(sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, nullptr));
}

int Database::Changes() const { return sqlite3_changes(handle_); }

Transaction::Transaction(Database* db) : db_(db) { db_->Exec("BEGIN IMMEDIATE"); }
Transaction::~Transaction() {
  if (!committed_) {
    try {
      db_->Exec("ROLLBACK");
    } catch (...) { /* Never throw during unwind. */
    }
  }
}
void Transaction::Commit() {
  db_->Exec("COMMIT");
  committed_ = true;
}
}  // namespace market
