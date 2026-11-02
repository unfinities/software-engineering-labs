// Copyright 2026 CampusMarket contributors
#include "market/service.h"

#include <atomic>
#include <barrier>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "market/security.h"

namespace {
using market::Command;
using market::Database;
using market::Error;
using market::ErrorKind;
using market::RandomId;
using market::Row;
using market::Search;
using market::Service;
using market::Session;
int checks = 0;
void Check(bool value, const std::string& label) {
  if (!value) {
    throw std::runtime_error("FAILED: " + label);
  }
  ++checks;
}
template <typename Function>
void Denied(Function function, ErrorKind kind, const std::string& label) {
  try {
    function();
  } catch (const Error& error) {
    Check(error.kind == kind, label + " error category");
    return;
  }
  throw std::runtime_error("FAILED: expected rejection " + label);
}
Row Draft(const std::string& title = "二手算法教材") {
  return {{"title", title},
          {"category", "书籍"},
          {"description", "带有唯一关键词 spark，整洁无缺页"},
          {"quality", "九成新"},
          {"price", "35.50"},
          {"location", "图书馆门口"}};
}
struct Fixture {
  std::filesystem::path directory;
  Service service;
  std::string password = RandomId();
  Session admin, seller, buyer, stranger;
  explicit Fixture(const std::filesystem::path& path) : directory(path), service(path) {
    auto invite = service.BootstrapInvite();
    service.Register("demo_admin", password, invite);
    service.Register("demo_seller", password);
    service.Register("demo_buyer", password);
    service.Register("other_user", password);
    admin = service.Login("demo_admin", password);
    seller = service.Login("demo_seller", password);
    buyer = service.Login("demo_buyer", password);
    stranger = service.Login("other_user", password);
    Denied([&] { service.Register("second_admin", password, invite); }, ErrorKind::kForbidden,
           "invite reuse");
    Denied([&] { service.BootstrapInvite(); }, ErrorKind::kForbidden, "bootstrap after admin");
  }
  std::string Do(const Session& actor, const std::string& action, const std::string& id = "",
                 const Row& fields = {}) {
    return service.Execute(actor.token, {action, id, RandomId(), fields});
  }
  std::string Published(const std::string& title = "二手算法教材") {
    auto id = Do(seller, "save", "", Draft(title));
    Do(seller, "submit", id);
    Do(admin, "approve", id);
    return id;
  }
};

void InputAndVisibility(Fixture* f) {
  Check(market::ParseMoney("0.01") == 1, "minimum money");
  Check(market::ParseMoney("99999.99") == 9999999, "maximum money");
  for (const auto& value : {"0", "100000", "1.234", "-1", ".5", "1.", "nan"}) {
    Denied([&] { market::ParseMoney(value); }, ErrorKind::kInput, "invalid money");
  }
  Check(market::DateStart("2024-02-29") + 86400 == market::DateStart("2024-03-01"), "leap date");
  Denied([] { market::DateStart("2026-02-29"); }, ErrorKind::kInput, "invalid date");
  Check(market::DateStart("2026-10-24") == 1792771200, "Beijing UTC offset");
  auto private_item = f->Do(f->seller, "save", "", Draft("隐藏草稿"));
  Denied([&] { f->service.Item("", private_item); }, ErrorKind::kAuth, "guest draft");
  Denied([&] { f->service.Item(f->stranger.token, private_item); }, ErrorKind::kForbidden,
         "other draft");
  Denied([&] { f->Do(f->buyer, "save", private_item, Draft()); }, ErrorKind::kForbidden,
         "other edit");
  Denied([&] { f->Do(f->stranger, "submit", private_item); }, ErrorKind::kForbidden,
         "other submit");
  Denied([&] { f->Do(f->buyer, "approve", private_item); }, ErrorKind::kForbidden,
         "ordinary review");
  Denied([&] { f->Do(f->stranger, "violate", private_item, {{"reason", "x"}}); },
         ErrorKind::kForbidden, "ordinary moderation");
  auto id = f->Published();
  Search query;
  query.keyword = "spark";
  Check(!f->service.Items("", query).empty(), "description keyword OR title");
  query.minimum = query.maximum = "35.50";
  Check(!f->service.Items("", query).empty(), "inclusive price bounds");
  query.category = "电脑";
  Check(f->service.Items("", query).empty(), "AND category");
  query.minimum = "50";
  query.maximum = "10";
  Denied([&] { f->service.Items("", query); }, ErrorKind::kInput, "inverted range");
  Denied([&] { f->Do(f->seller, "reserve", id, {{"contact", RandomId()}}); }, ErrorKind::kForbidden,
         "self reserve");
  Denied([&] { f->service.Execute("", {"reserve", id, RandomId(), {{"contact", RandomId()}}}); },
         ErrorKind::kAuth, "guest reserve");
  Denied([&] { f->service.Execute("invalid", {"reserve", id, RandomId(), {}}); }, ErrorKind::kAuth,
         "invalid session reserve");
  Denied([&] { f->service.Logs(f->buyer.token, {}); }, ErrorKind::kForbidden, "ordinary logs");
  Denied([&] { f->service.Items(f->buyer.token, Search{"admin"}); }, ErrorKind::kForbidden,
         "ordinary global scope");
}

void LifecycleAndPrivacy(Fixture* f) {
  auto id = f->Published();
  std::string buyer_contact = RandomId(), seller_contact = RandomId();
  Command reserve{"reserve", id, RandomId(), {{"contact", buyer_contact}}};
  auto reservation = f->service.Execute(f->buyer.token, reserve);
  Check(f->service.Execute(f->buyer.token, reserve) == reservation, "lost-response replay");
  auto seller_view = f->service.Reservation(f->seller.token, reservation);
  Check(seller_view.at("buyer_contact") == buyer_contact, "seller reads buyer while waiting");
  Check(!f->service.Reservation(f->buyer.token, reservation).contains("seller_contact"),
        "buyer cannot read early seller contact");
  Check(!seller_view.contains("buyer_private"), "no ciphertext in DTO");
  Denied([&] { f->service.Reservation(f->stranger.token, reservation); }, ErrorKind::kForbidden,
         "unrelated contact");
  Denied([&] { f->service.Reservation("", reservation); }, ErrorKind::kAuth, "anonymous contact");
  Denied([&] { f->Do(f->seller, "complete", reservation, {{"confirmed", "yes"}}); },
         ErrorKind::kConflict, "cannot complete waiting");
  Denied([&] { f->Do(f->seller, "cancel", reservation); }, ErrorKind::kForbidden,
         "seller cannot cancel waiting");
  f->Do(f->seller, "accept", reservation, {{"contact", seller_contact}});
  Check(f->service.Reservation(f->buyer.token, reservation).at("seller_contact") == seller_contact,
        "buyer reads after acceptance");
  Denied([&] { f->Do(f->buyer, "complete", reservation, {{"confirmed", "yes"}}); },
         ErrorKind::kForbidden, "buyer cannot complete");
  Denied([&] { f->Do(f->seller, "complete", reservation); }, ErrorKind::kInput,
         "confirmation required");
  Denied([&] { f->Do(f->buyer, "cancel", reservation); }, ErrorKind::kInput,
         "handover cancellation reason");
  f->Do(f->seller, "complete", reservation, {{"confirmed", "yes"}});
  for (const auto& actor : {f->buyer, f->seller}) {
    auto view = f->service.Reservation(actor.token, reservation);
    Check(!view.contains("buyer_contact") && !view.contains("seller_contact"), "completed privacy");
  }
  Check(f->service.Reservation(f->admin.token, reservation).at("buyer_contact") == buyer_contact,
        "admin private audit");
  Check(f->service.Execute(f->buyer.token, reserve) == reservation,
        "replay after completion returns ID only");
  Denied([&] { f->Do(f->seller, "takedown", id); }, ErrorKind::kConflict, "sold cannot takedown");
  Denied([&] { f->Do(f->buyer, "cancel", reservation); }, ErrorKind::kConflict,
         "completed cannot cancel");
  f->Do(f->admin, "violate", id, {{"reason", "历史违规"}});
  Check(f->service.Item(f->seller.token, id).at("status") == "sold",
        "sold moderation does not reverse");
  auto stats = f->service.Statistics(f->seller.token, market::Today(), market::Today());
  Check(stats[8].at("value") == "1" && stats[9].at("value") == "35.50", "sale snapshot amount");
  Database inspect(f->directory / "market.sqlite3");
  Check(inspect.Query("SELECT COUNT(*) AS n FROM logs WHERE action='reserve' AND result='success' "
                      "AND object_id=?",
                      {reservation})[0]
                .at("n") == "1",
        "one success log on replay");
  Check(inspect.Query("SELECT buyer_private FROM reservations WHERE id=?", {reservation})[0]
                .at("buyer_private")
                .find(buyer_contact) == std::string::npos,
        "encrypted contact storage");
  auto account = inspect.Query("SELECT password_hash FROM users WHERE id=?", {f->buyer.user_id})[0];
  Check(account.at("password_hash") != f->password, "no plaintext password");
  auto old_token = f->buyer.token;
  f->service.Logout(old_token);
  Denied([&] { f->service.Execute(old_token, reserve); }, ErrorKind::kAuth,
         "logout replay rejected first");
  f->buyer = f->service.Login("demo_buyer", f->password);
  for (const auto& row : f->service.Logs(f->admin.token, {})) {
    for (const auto& [key, value] : row) {
      Check(value.find(buyer_contact) == std::string::npos &&
                value.find(seller_contact) == std::string::npos &&
                value.find(f->password) == std::string::npos,
            "audit redaction " + key);
    }
  }
}

void EndStatesAndRollback(Fixture* f) {
  Database inspect(f->directory / "market.sqlite3");
  for (const auto& stage : {"published", "waiting", "handover"}) {
    auto id = f->Published();
    std::string reservation;
    if (std::string(stage) != "published") {
      reservation = f->Do(f->buyer, "reserve", id, {{"contact", RandomId()}});
    }
    if (std::string(stage) == "handover") {
      f->Do(f->seller, "accept", reservation, {{"contact", RandomId()}});
    }
    f->Do(f->seller, "takedown", id, {{"reason", "停止出售"}});
    Check(f->service.Item(f->seller.token, id).at("status") == "offline", "three-stage takedown");
    if (!reservation.empty()) {
      auto view = f->service.Reservation(f->buyer.token, reservation);
      Check(view.at("status") == "terminated" && view.at("reason") == "停止出售" &&
                !view.contains("seller_contact"),
            "terminated reason and privacy");
    }
  }
  for (const auto& action : {"cancel", "reject"}) {
    auto id = f->Published();
    auto reservation = f->Do(f->buyer, "reserve", id, {{"contact", RandomId()}});
    f->Do(std::string(action) == "cancel" ? f->buyer : f->seller, action, reservation,
          {{"reason", "计划变更"}});
    auto view = f->service.Reservation(f->seller.token, reservation);
    Check(!view.contains("buyer_contact"), "ended privacy");
    Check(f->service.Item("", id).at("status") == "published", "cancel/reject relists");
    Check(f->Do(f->buyer, "reserve", id, {{"contact", RandomId()}}) != reservation,
          "new reservation after cancellation");
  }
  auto id = f->Published();
  Command reserve{"reserve", id, RandomId(), {{"contact", RandomId()}}};
  inspect.Script(
      "CREATE TRIGGER inject BEFORE INSERT ON logs WHEN NEW.result='success' BEGIN SELECT "
      "RAISE(ABORT,'injected'); END;");
  Denied([&] { f->service.Execute(f->buyer.token, reserve); }, ErrorKind::kStorage,
         "reserve rollback");
  Check(f->service.Item("", id).at("status") == "published", "item rolled back");
  Check(inspect.Query("SELECT id FROM reservations WHERE item_id=?", {id}).empty(),
        "reservation rolled back");
  Check(!f->service.Logs(f->admin.token, {{"result", "failure"}}).empty(),
        "failure survives rollback");
  inspect.Script("DROP TRIGGER inject");
  auto reservation = f->service.Execute(f->buyer.token, reserve);
  f->Do(f->seller, "accept", reservation, {{"contact", RandomId()}});
  inspect.Script(
      "CREATE TRIGGER inject BEFORE INSERT ON logs WHEN NEW.result='success' BEGIN SELECT "
      "RAISE(ABORT,'injected'); END;");
  Denied([&] { f->Do(f->seller, "complete", reservation, {{"confirmed", "yes"}}); },
         ErrorKind::kStorage, "complete rollback");
  Denied([&] { f->Do(f->seller, "takedown", id, {{"reason", "下架"}}); }, ErrorKind::kStorage,
         "takedown rollback");
  Check(f->service.Item("", id).at("status") == "handover" &&
            f->service.Reservation(f->seller.token, reservation).at("status") == "handover",
        "paired states rolled back");
  inspect.Script("DROP TRIGGER inject");
  inspect.Script(
      "CREATE TRIGGER inject_all BEFORE INSERT ON logs BEGIN SELECT RAISE(ABORT,'injected'); END;");
  Denied([&] { f->Do(f->buyer, "complete", reservation); }, ErrorKind::kForbidden,
         "fallback failure");
  Check(std::filesystem::exists(f->directory / "failures.tsv"), "local fallback persisted");
  inspect.Script("DROP TRIGGER inject_all");
  bool fallback = false;
  for (const auto& row : f->service.Logs(f->admin.token, {})) {
    if (row.contains("source")) {
      fallback = true;
    }
  }
  Check(fallback, "admin sees fallback after recovery");
  Service restarted(f->directory);
  auto seller = restarted.Login("demo_seller", f->password);
  Check(restarted.Item(seller.token, id).at("status") == "handover", "restart persistence");
}

void ConcurrentReservations(Fixture* f) {
  std::vector<std::unique_ptr<Service>> services;
  std::vector<Session> buyers;
  for (int i = 0; i < 20; ++i) {
    std::string name = "concurrent_" + std::to_string(i);
    f->service.Register(name, f->password);
    services.push_back(std::make_unique<Service>(f->directory));
    buyers.push_back(services.back()->Login(name, f->password));
  }
  for (int round = 0; round < 10; ++round) {
    auto id = f->Published();
    std::barrier start(20);
    std::atomic<int> winners{0}, unexpected{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 20; ++i) {
      threads.emplace_back([&, i] {
        start.arrive_and_wait();
        try {
          services[i]->Execute(buyers[i].token,
                               {"reserve", id, RandomId(), {{"contact", RandomId()}}});
          ++winners;
        } catch (const Error& error) {
          if (error.kind != ErrorKind::kConflict) {
            ++unexpected;
          }
        }
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
    Check(winners == 1 && unexpected == 0, "20-way reservation round " + std::to_string(round + 1));
    Database inspect(f->directory / "market.sqlite3");
    Check(
        inspect.Query("SELECT COUNT(*) AS n FROM reservations WHERE item_id=? AND status='waiting'",
                      {id})[0]
                .at("n") == "1",
        "database unique active reservation");
  }
}

void RevisionsAndCompetingCommands(Fixture* f) {
  auto id = f->Published();
  auto old = f->service.Item(f->seller.token, id);
  f->Do(f->seller, "withdraw", id);
  Denied([&] { f->Do(f->seller, "submit", id, {{"version", old.at("version")}}); },
         ErrorKind::kConflict, "stale page version");
  Command save{"save", id, RandomId(), Draft("已修改的教材")};
  f->service.Execute(f->seller.token, save);
  auto changed = save;
  changed.fields["title"] = "不能复用编号修改内容";
  Denied([&] { f->service.Execute(f->seller.token, changed); }, ErrorKind::kInput,
         "payload mismatch");
  f->Do(f->seller, "submit", id);
  f->Do(f->admin, "reject_item", id, {{"reason", "补充说明"}});
  Check(f->service.History(f->seller.token, id).back().at("reason") == "补充说明",
        "review reason history");
  f->Do(f->seller, "save", id, Draft());
  f->Do(f->seller, "submit", id);
  f->Do(f->admin, "violate", id, {{"reason", "违规物品"}});
  Denied([&] { f->Do(f->seller, "save", id, Draft()); }, ErrorKind::kConflict, "locked edit");
  f->Do(f->admin, "unlock", id, {{"reason", "已处理"}});
  Check(f->service.Item(f->seller.token, id).at("status") == "offline",
        "unlock never auto publishes");
  f->Do(f->seller, "save", id, Draft());
  f->Do(f->seller, "submit", id);
  f->Do(f->admin, "approve", id);
  auto reservation = f->Do(f->buyer, "reserve", id, {{"contact", RandomId()}});
  f->Do(f->seller, "accept", reservation, {{"contact", RandomId()}});
  Service second(f->directory);
  auto second_buyer = second.Login("demo_buyer", f->password);
  std::barrier start(2);
  std::atomic<int> succeeded{0}, rejected{0};
  std::thread cancel([&] {
    start.arrive_and_wait();
    try {
      second.Execute(second_buyer.token,
                     {"cancel", reservation, RandomId(), {{"reason", "取消交接"}}});
      ++succeeded;
    } catch (const Error& error) {
      if (error.kind == ErrorKind::kConflict) {
        ++rejected;
      }
    }
  });
  start.arrive_and_wait();
  try {
    f->Do(f->seller, "complete", reservation, {{"confirmed", "yes"}});
    ++succeeded;
  } catch (const Error& error) {
    if (error.kind == ErrorKind::kConflict) {
      ++rejected;
    }
  }
  cancel.join();
  Check(succeeded == 1 && rejected == 1, "cancel vs complete exactly one wins");
  auto state = f->service.Reservation(f->buyer.token, reservation).at("status");
  auto item_state = f->service.Item(f->seller.token, id).at("status");
  Check((state == "cancelled" && item_state == "published") ||
            (state == "completed" && item_state == "sold"),
        "race state pairing");
}

void Seed(Fixture* f) {
  f->Published("九成新机械键盘");
  f->Published("数据结构教材");
  auto pending = f->Do(f->seller, "save", "", Draft("待审核的台灯"));
  f->Do(f->seller, "submit", pending);
  auto id = f->Published("可线下交接的显示器");
  auto reservation = f->Do(f->buyer, "reserve", id, {{"contact", "示例联系方式 请勿用于真实联络"}});
  f->Do(f->seller, "accept", reservation, {{"contact", "示例卖家联系方式"}});
  std::ofstream file(f->directory / "demo-access.txt");
  file << "demo_admin\ndemo_seller\ndemo_buyer\n" << f->password << '\n';
  std::cout << "Seeded isolated demo data; runtime-generated access is local only.\n";
}
}  // namespace

int main(int argc, char** argv) {
  try {
    auto path = std::filesystem::temp_directory_path() / ("campus-market-tests-" + RandomId());
    if (argc == 3 && std::string(argv[1]) == "--seed") {
      path = std::filesystem::path(market::Wide(argv[2]));
    }
    Fixture fixture(path);
    if (argc == 3 && std::string(argv[1]) == "--seed") {
      Seed(&fixture);
      return 0;
    }
    InputAndVisibility(&fixture);
    LifecycleAndPrivacy(&fixture);
    EndStatesAndRollback(&fixture);
    ConcurrentReservations(&fixture);
    RevisionsAndCompetingCommands(&fixture);
    std::cout << "PASS " << checks
              << " assertions; 10 rounds x 20 independent connections; privacy, transactions, "
                 "replay, restart.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
