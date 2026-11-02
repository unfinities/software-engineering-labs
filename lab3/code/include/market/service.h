// Copyright 2026 CampusMarket contributors
#ifndef LAB3_CODE_INCLUDE_MARKET_SERVICE_H_
#define LAB3_CODE_INCLUDE_MARKET_SERVICE_H_
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

#include "market/database.h"

namespace market {
struct Session {
  std::string token;
  std::string user_id;
  std::string username;
  bool admin = false;
};

struct Command {
  std::string action;
  std::string object_id;
  std::string request_id;
  Row fields;
};

struct Search {
  std::string scope = "public";
  std::string keyword;
  std::string category;
  std::string minimum;
  std::string maximum;
  std::string status;
  int page = 0;
};

class Service {
 public:
  explicit Service(const std::filesystem::path& directory);
  // An invitation is generated on demand only before the first admin exists.
  std::string BootstrapInvite();
  void Register(const std::string& username, const std::string& password,
                const std::string& invitation = "");
  Session Login(const std::string& username, const std::string& password);
  void Logout(const std::string& token);
  Session Current(const std::string& token);
  std::string Execute(const std::string& token, const Command& command);
  Rows Items(const std::string& token, const Search& search);
  Row Item(const std::string& token, const std::string& id);
  Rows Reservations(const std::string& token, bool incoming);
  Row Reservation(const std::string& token, const std::string& id);
  Rows History(const std::string& token, const std::string& item_id);
  Rows Statistics(const std::string& token, const std::string& start, const std::string& end);
  Rows Logs(const std::string& token, const Row& filters);
  // Test-only capability is compiled out of the application library by default.
  // Tests inject a SQLite trigger from their own connection instead.

 private:
  Session Require(const std::string& token);
  Row ItemRaw(const std::string& id);
  Row ReservationRaw(const std::string& id);
  void Authorize(const Session& actor, const Command& command);
  std::string Apply(const Session& actor, const Command& command);
  std::string ApplyItem(const Session& actor, const Command& command);
  std::string ApplyTrade(const Session& actor, const Command& command);
  void EndReservation(const Row& reservation, const std::string& state, const std::string& reason,
                      const std::string& item_state);
  void Failure(const std::string& actor, const Command& command, ErrorKind kind);
  void Success(const Session& actor, const Command& command, const std::string& object,
               const std::string& before);
  void ValidateDraft(const Row& fields, bool complete);
  Database db_;
  std::filesystem::path directory_;
  std::recursive_mutex mutex_;
  std::map<std::string, Session> sessions_;
};
}  // namespace market
#endif  // LAB3_CODE_INCLUDE_MARKET_SERVICE_H_
