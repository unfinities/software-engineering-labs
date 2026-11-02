// Copyright 2026 CampusMarket contributors
#include "market/service.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "market/security.h"

namespace market {
namespace {
std::string Field(const Row& row, const std::string& key) {
  auto it = row.find(key);
  return it == row.end() ? "" : it->second;
}
void Need(bool condition, const std::string& message, ErrorKind kind = ErrorKind::kConflict) {
  if (!condition) {
    throw Error(kind, message);
  }
}
bool Active(const std::string& state) { return state == "waiting" || state == "handover"; }
void Text(const std::string& value, size_t limit, bool required) {
  Need(value.size() <= limit, "文字超过允许长度", ErrorKind::kInput);
  bool visible = false;
  for (unsigned char ch : value) {
    Need(ch >= 32 || ch == '\n' || ch == '\r', "文字含控制字符", ErrorKind::kInput);
    if (ch > 32) {
      visible = true;
    }
  }
  Need(!required || visible, "请填写全部必填字段", ErrorKind::kInput);
}
const char* kSchema = R"sql(
CREATE TABLE IF NOT EXISTS users(
 id TEXT PRIMARY KEY, username TEXT NOT NULL UNIQUE, salt TEXT NOT NULL,
 password_hash TEXT NOT NULL, admin INTEGER NOT NULL CHECK(admin IN (0,1)));
CREATE TABLE IF NOT EXISTS invite(
 id INTEGER PRIMARY KEY CHECK(id=1), digest TEXT NOT NULL, used INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS items(
 id TEXT PRIMARY KEY, seller_id TEXT NOT NULL REFERENCES users(id),
 title TEXT NOT NULL, category TEXT NOT NULL, description TEXT NOT NULL,
 quality TEXT NOT NULL, location TEXT NOT NULL,
 price_fen INTEGER NOT NULL CHECK(price_fen BETWEEN 0 AND 9999999),
 status TEXT NOT NULL CHECK(status IN
 ('draft','pending','rejected','published','reserved','handover','sold','offline')),
 locked INTEGER NOT NULL DEFAULT 0, version INTEGER NOT NULL DEFAULT 1,
 created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS reservations(
 id TEXT PRIMARY KEY, item_id TEXT NOT NULL REFERENCES items(id),
 buyer_id TEXT NOT NULL REFERENCES users(id),
 status TEXT NOT NULL CHECK(status IN
 ('waiting','handover','cancelled','rejected','terminated','completed')),
 title TEXT NOT NULL, price_fen INTEGER NOT NULL, location TEXT NOT NULL,
 buyer_private TEXT NOT NULL, seller_private TEXT NOT NULL DEFAULT '',
 reason TEXT NOT NULL DEFAULT '', created_at INTEGER NOT NULL,
 completed_at INTEGER, version INTEGER NOT NULL DEFAULT 1);
CREATE UNIQUE INDEX IF NOT EXISTS one_active_reservation ON reservations(item_id)
 WHERE status IN ('waiting','handover');
CREATE INDEX IF NOT EXISTS item_scope ON items(seller_id,status);
CREATE INDEX IF NOT EXISTS item_public ON items(status,locked,category,price_fen);
CREATE INDEX IF NOT EXISTS reservation_buyer ON reservations(buyer_id,created_at);
CREATE TABLE IF NOT EXISTS moderation(
 id TEXT PRIMARY KEY, item_id TEXT NOT NULL REFERENCES items(id), actor_id TEXT NOT NULL,
 action TEXT NOT NULL, reason TEXT NOT NULL, created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS item_versions(
 id TEXT PRIMARY KEY, item_id TEXT NOT NULL REFERENCES items(id), title TEXT NOT NULL,
 category TEXT NOT NULL, description TEXT NOT NULL, quality TEXT NOT NULL,
 location TEXT NOT NULL, price_fen INTEGER NOT NULL, version INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS receipts(
 actor_id TEXT NOT NULL REFERENCES users(id), request_id TEXT NOT NULL,
 fingerprint TEXT NOT NULL, result_id TEXT NOT NULL,
 PRIMARY KEY(actor_id,request_id));
CREATE TABLE IF NOT EXISTS logs(
 id TEXT PRIMARY KEY, created_at INTEGER NOT NULL, actor_id TEXT,
 action TEXT NOT NULL, object_id TEXT NOT NULL, request_id TEXT NOT NULL,
 result TEXT NOT NULL, before_state TEXT NOT NULL, after_state TEXT NOT NULL,
 reason TEXT NOT NULL);
CREATE TRIGGER IF NOT EXISTS logs_no_update BEFORE UPDATE ON logs
 BEGIN SELECT RAISE(ABORT,'append only'); END;
CREATE TRIGGER IF NOT EXISTS logs_no_delete BEFORE DELETE ON logs
 BEGIN SELECT RAISE(ABORT,'append only'); END;
CREATE TRIGGER IF NOT EXISTS sold_no_reverse BEFORE UPDATE OF status ON items
 WHEN OLD.status='sold' AND NEW.status<>'sold'
 BEGIN SELECT RAISE(ABORT,'sold is final'); END;
)sql";
const std::set<std::string> kItemActions{
    "save",    "submit",      "withdraw", "takedown",
    "approve", "reject_item", "violate",  "unlock"};  // NOLINT(whitespace/indent_namespace)
const std::set<std::string> kTradeActions = {"reserve", "accept", "reject", "cancel", "complete"};
}  // namespace

Service::Service(const std::filesystem::path& directory)
    : db_(directory / "market.sqlite3"), directory_(directory) {
  db_.Script(kSchema);
}

std::string Service::BootstrapInvite() {
  std::lock_guard lock(mutex_);
  Transaction transaction(&db_);
  Need(db_.Query("SELECT id FROM users WHERE admin=1").empty(),
       "管理员已初始化，不能再次生成初始邀请码", ErrorKind::kForbidden);
  std::string code = RandomId();
  db_.Exec(
      "INSERT INTO invite VALUES(1,?,0) ON CONFLICT(id) DO UPDATE SET "
      "digest=excluded.digest,used=0",
      {Digest(code)});
  transaction.Commit();
  return code;
}

void Service::Register(const std::string& username, const std::string& password,
                       const std::string& invitation) {
  std::lock_guard lock(mutex_);
  Command command{"register", "", RandomId(), {}};
  try {
    Text(username, 72, true);
    Need(username.size() >= 2 && username.find_first_of(" \r\n\t") == std::string::npos,
         "用户名至少两个字符且不能包含空白", ErrorKind::kInput);
    Need(password.size() >= 10 && password.size() <= 128, "密码须为 10 至 128 字节",
         ErrorKind::kInput);
    auto salt = RandomId();
    auto hash = HashPassword(password, salt);
    Transaction transaction(&db_);
    Need(db_.Query("SELECT id FROM users WHERE username=?", {username}).empty(), "用户名已被使用",
         ErrorKind::kInput);
    bool admin = !invitation.empty();
    if (admin) {
      db_.Exec("UPDATE invite SET used=1 WHERE id=1 AND used=0 AND digest=?", {Digest(invitation)});
      Need(db_.Changes() == 1, "邀请码无效或已使用", ErrorKind::kForbidden);
    }
    std::string id = RandomId();
    db_.Exec("INSERT INTO users VALUES(?,?,?,?,?)", {id, username, salt, hash, admin ? "1" : "0"});
    Success(Session{"", id, username, admin}, command, id, "");
    transaction.Commit();
  } catch (const Error& error) {
    Failure("", command, error.kind);
    throw;
  }
}

Session Service::Login(const std::string& username, const std::string& password) {
  std::lock_guard lock(mutex_);
  Command command{"login", "", RandomId(), {}};
  try {
    Need(username.size() <= 72 && password.size() <= 128, "用户名或密码错误", ErrorKind::kAuth);
    auto rows = db_.Query("SELECT * FROM users WHERE username=?", {username});
    std::string salt = rows.empty() ? std::string(48, '0') : rows[0].at("salt");
    auto candidate = HashPassword(password, salt);
    Need(!rows.empty() && ConstantEqual(rows[0].at("password_hash"), candidate), "用户名或密码错误",
         ErrorKind::kAuth);
    Session session{RandomId(), rows[0].at("id"), username, rows[0].at("admin") == "1"};
    Success(session, command, session.user_id, "");
    sessions_[session.token] = session;
    return session;
  } catch (const Error& error) {
    Failure("", command, error.kind);
    throw;
  }
}

void Service::Logout(const std::string& token) {
  std::lock_guard lock(mutex_);
  sessions_.erase(token);
}
Session Service::Current(const std::string& token) {
  std::lock_guard lock(mutex_);
  if (token.empty()) {
    return {};
  }
  return Require(token);
}
Session Service::Require(const std::string& token) {
  auto it = sessions_.find(token);
  Need(it != sessions_.end(), "请先登录，原会话可能已退出", ErrorKind::kAuth);
  auto rows = db_.Query("SELECT id,admin FROM users WHERE id=?", {it->second.user_id});
  Need(!rows.empty(), "当前账号不可用", ErrorKind::kAuth);
  it->second.admin = rows[0].at("admin") == "1";
  return it->second;
}
Row Service::ItemRaw(const std::string& id) {
  auto rows = db_.Query("SELECT * FROM items WHERE id=?", {id});
  Need(!rows.empty(), "商品不存在或无权访问", ErrorKind::kForbidden);
  return rows[0];
}
Row Service::ReservationRaw(const std::string& id) {
  auto rows = db_.Query(
      "SELECT r.*,i.seller_id FROM reservations r "
      "JOIN items i ON i.id=r.item_id WHERE r.id=?",
      {id});
  Need(!rows.empty(), "预约不存在或无权访问", ErrorKind::kForbidden);
  return rows[0];
}

void Service::Authorize(const Session& actor, const Command& command) {
  const auto& action = command.action;
  Need(kItemActions.contains(action) || kTradeActions.contains(action), "不支持的操作",
       ErrorKind::kInput);
  if (action == "save" && command.object_id.empty()) {
    return;
  }
  bool admin =
      action == "approve" || action == "reject_item" || action == "violate" || action == "unlock";
  if (admin) {
    Need(actor.admin, "只有管理员可以执行此操作", ErrorKind::kForbidden);
    ItemRaw(command.object_id);
  } else if (action == "reserve") {
    auto item = ItemRaw(command.object_id);
    Need(item.at("seller_id") != actor.user_id, "不能预约自己的商品", ErrorKind::kForbidden);
  } else if (kItemActions.contains(action)) {
    Need(ItemRaw(command.object_id).at("seller_id") == actor.user_id, "只能管理自己的商品",
         ErrorKind::kForbidden);
  } else {
    auto reservation = ReservationRaw(command.object_id);
    bool seller = reservation.at("seller_id") == actor.user_id;
    bool buyer = reservation.at("buyer_id") == actor.user_id;
    Need(action == "cancel" ? (seller || buyer) : seller, "当前账号无权处理该预约",
         ErrorKind::kForbidden);
  }
}

std::string Service::Execute(const std::string& token, const Command& command) {
  std::lock_guard lock(mutex_);
  std::string actor_id;
  try {
    auto actor = Require(token);
    actor_id = actor.user_id;
    Need(command.request_id.size() == 48 &&
             command.request_id.find_first_not_of("0123456789abcdef") == std::string::npos,
         "请求编号格式无效", ErrorKind::kInput);
    Authorize(actor, command);  // Replays never bypass current authorization.
    std::string payload = command.action + "\n" + command.object_id;
    for (const auto& [key, value] : command.fields) {
      payload += "\n" + std::to_string(key.size()) + ":" + key + std::to_string(value.size()) +
                 ":" + value;
    }
    auto fingerprint = Digest(payload);
    Transaction transaction(&db_);
    auto receipts = db_.Query("SELECT * FROM receipts WHERE actor_id=? AND request_id=?",
                              {actor.user_id, command.request_id});
    if (!receipts.empty()) {
      Need(receipts[0].at("fingerprint") == fingerprint, "同一请求编号不能用于不同内容，请重新提交",
           ErrorKind::kInput);
      auto result = receipts[0].at("result_id");
      transaction.Commit();
      return result;  // Only an identifier, never cached protected content.
    }
    Authorize(actor, command);
    std::string before;
    if (!command.object_id.empty()) {
      before = kItemActions.contains(command.action) || command.action == "reserve"
                   ? ItemRaw(command.object_id).at("status")
                   : ReservationRaw(command.object_id).at("status");
      const auto expected = Field(command.fields, "version");
      const auto object = kItemActions.contains(command.action) || command.action == "reserve"
                              ? ItemRaw(command.object_id)
                              : ReservationRaw(command.object_id);
      Need(expected.empty() || object.at("version") == expected, "页面已过时，请刷新后重新操作");
    }
    auto result = Apply(actor, command);
    Success(actor, command, result, before);
    db_.Exec("INSERT INTO receipts VALUES(?,?,?,?)",
             {actor.user_id, command.request_id, fingerprint, result});
    transaction.Commit();
    return result;
  } catch (const Error& error) {
    Failure(actor_id, command, error.kind);
    throw;
  }
}

void Service::ValidateDraft(const Row& fields, bool complete) {
  Text(Field(fields, "title"), 180, complete);
  Text(Field(fields, "category"), 60, complete);
  Text(Field(fields, "description"), 6000, complete);
  Text(Field(fields, "quality"), 120, complete);
  Text(Field(fields, "location"), 300, complete);
  auto price = Field(fields, "price");
  if (complete || !price.empty()) {
    ParseMoney(price);
  }
}

std::string Service::Apply(const Session& actor, const Command& command) {
  return kItemActions.contains(command.action) ? ApplyItem(actor, command)
                                               : ApplyTrade(actor, command);
}

std::string Service::ApplyItem(const Session& actor, const Command& command) {
  const auto& action = command.action;
  auto id = command.object_id;
  if (action == "save") {
    ValidateDraft(command.fields, false);
    if (id.empty()) {
      id = RandomId();
      db_.Exec(
          "INSERT INTO items(id,seller_id,title,category,description,quality,location,"
          "price_fen,status,created_at) VALUES(?,?,?,?,?,?,?,?,'draft',?)",
          {id, actor.user_id, Field(command.fields, "title"), Field(command.fields, "category"),
           Field(command.fields, "description"), Field(command.fields, "quality"),
           Field(command.fields, "location"),
           Field(command.fields, "price").empty()
               ? "0"
               : std::to_string(ParseMoney(Field(command.fields, "price"))),
           std::to_string(Now())});
    } else {
      auto item = ItemRaw(id);
      Need(item.at("locked") == "0", "商品已锁定，需管理员解除锁定");
      Need(item.at("status") == "draft" || item.at("status") == "rejected" ||
               item.at("status") == "offline",
           "请先撤回为草稿，预约中和已售出不可编辑");
      db_.Exec(
          "INSERT INTO item_versions SELECT ?,id,title,category,description,quality,"
          "location,price_fen,version FROM items WHERE id=?",
          {RandomId(), id});
      db_.Exec(
          "UPDATE items SET title=?,category=?,description=?,quality=?,location=?,"
          "price_fen=?,status='draft',version=version+1 WHERE id=?",
          {Field(command.fields, "title"), Field(command.fields, "category"),
           Field(command.fields, "description"), Field(command.fields, "quality"),
           Field(command.fields, "location"),
           Field(command.fields, "price").empty()
               ? "0"
               : std::to_string(ParseMoney(Field(command.fields, "price"))),
           id});
    }
    return id;
  }
  auto item = ItemRaw(id);
  const auto& state = item.at("status");
  auto reason = Field(command.fields, "reason");
  Text(reason, 1200, false);
  std::string next = state;
  if (action == "submit") {
    Need(state == "draft" && item.at("locked") == "0", "仅未锁定草稿可提交");
    auto fields = item;
    fields["price"] = Money(std::stoll(item.at("price_fen")));
    ValidateDraft(fields, true);
    next = "pending";
  } else if (action == "withdraw") {
    Need((state == "pending" || state == "published") && item.at("locked") == "0",
         "仅待审核或已发布商品可撤回草稿");
    next = "draft";
  } else if (action == "approve" || action == "reject_item") {
    Need(state == "pending" && item.at("locked") == "0", "商品当前不是待审核状态");
    if (action == "reject_item") {
      Text(reason, 1200, true);
    }
    next = action == "approve" ? "published" : "rejected";
  } else if (action == "unlock") {
    Need(item.at("locked") == "1", "商品未锁定");
    Text(reason, 1200, true);
    db_.Exec("UPDATE items SET locked=0 WHERE id=?", {id});
  } else if (action == "takedown" || action == "violate") {
    bool violation = action == "violate";
    if (violation) {
      Text(reason, 1200, true);
    }
    Need(state == "published" || state == "reserved" || state == "handover" ||
             (violation && (state == "pending" || state == "sold")),
         "当前状态不允许下架");
    if (state != "sold") {
      auto active = db_.Query(
          "SELECT id FROM reservations WHERE item_id=? AND "
          "status IN ('waiting','handover')",
          {id});
      if (!active.empty()) {
        Text(reason, 1200, true);
        EndReservation(ReservationRaw(active[0].at("id")), "terminated", reason, "offline");
      }
      next = "offline";
      if (violation) {
        db_.Exec("UPDATE items SET locked=1 WHERE id=?", {id});
      }
    }
  }
  if (action == "approve" || action == "reject_item" || action == "violate" || action == "unlock") {
    db_.Exec("INSERT INTO moderation VALUES(?,?,?,?,?,?)",
             {RandomId(), id, actor.user_id, action, reason, std::to_string(Now())});
  }
  db_.Exec("UPDATE items SET status=?,version=version+1 WHERE id=?", {next, id});
  return id;
}

void Service::EndReservation(const Row& reservation, const std::string& state,
                             const std::string& reason, const std::string& item_state) {
  db_.Exec("UPDATE reservations SET status=?,reason=?,version=version+1 WHERE id=?",
           {state, reason, reservation.at("id")});
  db_.Exec("UPDATE items SET status=?,version=version+1 WHERE id=?",
           {item_state, reservation.at("item_id")});
}

std::string Service::ApplyTrade(const Session& actor, const Command& command) {
  const auto& action = command.action;
  const auto& id = command.object_id;
  auto contact = Field(command.fields, "contact");
  auto reason = Field(command.fields, "reason");
  Text(reason, 1200, false);
  if (action == "reserve") {
    auto item = ItemRaw(id);
    Need(item.at("status") == "published" && item.at("locked") == "0",
         "商品已被预约或当前不可预约");
    Text(contact, 300, true);
    auto reservation_id = RandomId();
    db_.Exec(
        "UPDATE items SET status='reserved',version=version+1 "
        "WHERE id=? AND status='published' AND locked=0",
        {id});
    Need(db_.Changes() == 1, "商品状态已变化，请刷新");
    db_.Exec(
        "INSERT INTO reservations(id,item_id,buyer_id,status,title,price_fen,"
        "location,buyer_private,created_at) VALUES(?,?,?,'waiting',?,?,?,?,?)",
        {reservation_id, id, actor.user_id, item.at("title"), item.at("price_fen"),
         item.at("location"), Protect(contact), std::to_string(Now())});
    return reservation_id;
  }
  auto reservation = ReservationRaw(id);
  auto item = ItemRaw(reservation.at("item_id"));
  const auto& state = reservation.at("status");
  Need(Active(state), "预约已结束，不能再次改变交易状态");
  Need(item.at("status") == (state == "waiting" ? "reserved" : "handover"), "商品与预约状态冲突");
  if (action == "accept") {
    Need(state == "waiting", "仅等待确认的预约可接受");
    Text(contact, 300, true);
    db_.Exec(
        "UPDATE reservations SET status='handover',seller_private=?,version=version+1 "
        "WHERE id=?",
        {Protect(contact), id});
    db_.Exec("UPDATE items SET status='handover',version=version+1 WHERE id=?",
             {reservation.at("item_id")});
  } else if (action == "reject") {
    Need(state == "waiting", "仅等待确认的预约可拒绝");
    Text(reason, 1200, true);
    EndReservation(reservation, "rejected", reason, "published");
  } else if (action == "cancel") {
    Need(state != "waiting" || actor.user_id == reservation.at("buyer_id"),
         "等待确认时仅买家可取消", ErrorKind::kForbidden);
    if (state == "handover") {
      Text(reason, 1200, true);
    }
    EndReservation(reservation, "cancelled", reason, "published");
  } else if (action == "complete") {
    Need(state == "handover", "卖家接受后才能确认完成");
    Need(Field(command.fields, "confirmed") == "yes", "必须先确认已完成线下交接",
         ErrorKind::kInput);
    EndReservation(reservation, "completed", "线下交接完成", "sold");
    db_.Exec("UPDATE reservations SET completed_at=? WHERE id=?", {std::to_string(Now()), id});
  }
  return id;
}

Rows Service::Items(const std::string& token, const Search& search) {
  std::lock_guard lock(mutex_);
  Need(search.page >= 0 && search.page <= 100000, "页码无效", ErrorKind::kInput);
  Session actor;
  if (!token.empty()) {
    actor = Require(token);
  }
  std::string where;
  std::vector<std::string> args;
  if (search.scope == "mine") {
    actor = Require(token);
    where = "seller_id=?";
    args.push_back(actor.user_id);
  } else if (search.scope == "admin") {
    actor = Require(token);
    Need(actor.admin, "只有管理员可查看全部商品", ErrorKind::kForbidden);
    where = "1=1";
  } else {
    Need(search.scope == "public", "查询范围无效", ErrorKind::kInput);
    where =
        "locked=0 AND status IN ('published','reserved','handover','sold') "
        "AND NOT (status='sold' AND EXISTS(SELECT 1 FROM moderation m "
        "WHERE m.item_id=items.id AND m.action='violate'))";
  }
  int64_t minimum = 0, maximum = 9999999;
  auto price = [](const std::string& value) {
    return value == "0" || value == "0.00" || value == "0.0" ? 0 : ParseMoney(value);
  };
  if (!search.minimum.empty()) {
    minimum = price(search.minimum);
  }
  if (!search.maximum.empty()) {
    maximum = price(search.maximum);
  }
  Need(minimum <= maximum, "最低价不能高于最高价", ErrorKind::kInput);
  where += " AND price_fen BETWEEN ? AND ?";
  args.push_back(std::to_string(minimum));
  args.push_back(std::to_string(maximum));
  if (!search.keyword.empty()) {
    Text(search.keyword, 180, false);
    where += " AND (instr(lower(title),lower(?))>0 OR instr(lower(description),lower(?))>0)";
    args.push_back(search.keyword);
    args.push_back(search.keyword);
  }
  if (!search.category.empty()) {
    where += " AND category=?";
    args.push_back(search.category);
  }
  if (!search.status.empty()) {
    where += " AND status=?";
    args.push_back(search.status);
  }
  args.push_back(std::to_string(search.page * 20));
  return db_.Query(
      "SELECT id,seller_id,title,category,description,quality,location,price_fen,"
      "status,locked,version,created_at FROM items WHERE " +
          where + " ORDER BY created_at DESC,id LIMIT 20 OFFSET ?",
      args);
}

Row Service::Item(const std::string& token, const std::string& id) {
  std::lock_guard lock(mutex_);
  auto item = ItemRaw(id);
  bool public_item = item.at("locked") == "0" &&
                     (item.at("status") == "published" || item.at("status") == "reserved" ||
                      item.at("status") == "handover" || item.at("status") == "sold");
  if (item.at("status") == "sold" &&
      !db_.Query("SELECT id FROM moderation WHERE item_id=? AND action='violate'", {id}).empty()) {
    public_item = false;
  }
  if (!public_item) {
    auto actor = Require(token);
    Need(actor.admin || actor.user_id == item.at("seller_id"), "该商品不在可见范围",
         ErrorKind::kForbidden);
  }
  return item;
}

Rows Service::Reservations(const std::string& token, bool incoming) {
  std::lock_guard lock(mutex_);
  auto actor = Require(token);
  // A list DTO deliberately excludes encrypted and decrypted contact fields.
  return db_.Query(
      "SELECT r.id,r.item_id,r.title,r.price_fen,r.location,r.status,"
      "r.reason,r.created_at,r.completed_at,r.version,i.seller_id,r.buyer_id "
      "FROM reservations r JOIN items i ON i.id=r.item_id WHERE " +
          std::string(incoming ? "i.seller_id=?" : "r.buyer_id=?") +
          " ORDER BY r.created_at DESC,r.id",
      {actor.user_id});
}

Row Service::Reservation(const std::string& token, const std::string& id) {
  std::lock_guard lock(mutex_);
  auto actor = Require(token);
  auto reservation = ReservationRaw(id);
  bool buyer = reservation.at("buyer_id") == actor.user_id;
  bool seller = reservation.at("seller_id") == actor.user_id;
  Need(actor.admin || buyer || seller, "无权访问此预约", ErrorKind::kForbidden);
  std::string buyer_private = reservation.at("buyer_private");
  std::string seller_private = reservation.at("seller_private");
  reservation.erase("buyer_private");
  reservation.erase("seller_private");
  auto state = reservation.at("status");
  if (actor.admin || (Active(state) && seller)) {
    reservation["buyer_contact"] = Unprotect(buyer_private);
  }
  if (actor.admin || (state == "handover" && buyer)) {
    reservation["seller_contact"] = Unprotect(seller_private);
  }
  return reservation;
}

Rows Service::History(const std::string& token, const std::string& item_id) {
  std::lock_guard lock(mutex_);
  auto actor = Require(token);
  auto item = ItemRaw(item_id);
  Need(actor.admin || actor.user_id == item.at("seller_id"), "只有卖家和管理员可查看商品内部历史",
       ErrorKind::kForbidden);
  return db_.Query(
      "SELECT created_at,action,reason FROM moderation WHERE item_id=? "
      "ORDER BY created_at,rowid",
      {item_id});
}

Rows Service::Statistics(const std::string& token, const std::string& start,
                         const std::string& end) {
  std::lock_guard lock(mutex_);
  auto actor = Require(token);
  auto first = DateStart(start), last = DateStart(end) + 86400;
  Need(first < last, "开始日期不能晚于结束日期", ErrorKind::kInput);
  std::vector<std::string> args;
  std::string where = "1=1";
  if (!actor.admin) {
    where = "i.seller_id=?";
    args.push_back(actor.user_id);
  }
  auto counts = db_.Query(
      "SELECT status,COUNT(*) AS quantity FROM items i WHERE " + where + " GROUP BY status", args);
  Rows result;
  for (const auto& state :
       {"draft", "pending", "rejected", "published", "reserved", "handover", "sold", "offline"}) {
    std::string count = "0";
    for (const auto& row : counts) {
      if (row.at("status") == state) {
        count = row.at("quantity");
      }
    }
    result.push_back({{"metric", state}, {"value", count}});
  }
  args.push_back(std::to_string(first));
  args.push_back(std::to_string(last));
  auto totals = db_.Query(
      "SELECT COUNT(*) AS quantity,COALESCE(SUM(r.price_fen),0) AS total "
      "FROM reservations r JOIN items i ON i.id=r.item_id WHERE " +
          where + " AND r.status='completed' AND r.completed_at>=? AND r.completed_at<?",
      args);
  result.push_back({{"metric", "completed_count"}, {"value", totals[0].at("quantity")}});
  result.push_back(
      {{"metric", "completed_amount"}, {"value", Money(std::stoll(totals[0].at("total")))}});
  return result;
}

void Service::Success(const Session& actor, const Command& command, const std::string& object,
                      const std::string& before) {
  std::string after;
  if (kItemActions.contains(command.action)) {
    after = ItemRaw(object).at("status");
  } else if (kTradeActions.contains(command.action)) {
    after = ReservationRaw(object).at("status");
  }
  // User-entered reasons stay in the authorized business history, never in logs.
  db_.Exec("INSERT INTO logs VALUES(?,?,?,?,?,?,?,?,?,?)",
           {RandomId(), std::to_string(Now()), actor.user_id, command.action, object,
            command.request_id, "success", before, after,
            command.fields.contains("reason") ? "business_reason_in_history" : "ok"});
}

void Service::Failure(const std::string& actor, const Command& command, ErrorKind kind) {
  const char* kinds[] = {"invalid_input",  "not_authenticated", "forbidden",
                         "state_conflict", "storage_failure",   "processing"};
  std::string reason = kinds[static_cast<size_t>(kind)];
  // Only known actions and opaque IDs may enter the audit sink.
  std::string action = kItemActions.contains(command.action) ||
                               kTradeActions.contains(command.action) ||
                               command.action == "register" || command.action == "login"
                           ? command.action
                           : "unknown";
  auto opaque = [](const std::string& id) {
    return id.size() == 48 && id.find_first_not_of("0123456789abcdef") == std::string::npos ? id
                                                                                            : "";
  };
  Row row{{"id", RandomId()},
          {"created_at", std::to_string(Now())},
          {"actor_id", opaque(actor)},
          {"action", action},
          {"object_id", opaque(command.object_id)},
          {"request_id", opaque(command.request_id)},
          {"result", "failure"},
          {"before_state", ""},
          {"after_state", ""},
          {"reason", reason}};
  try {
    db_.Exec("INSERT INTO logs VALUES(?,?,?,?,?,?,?,?,?,?)",
             {row.at("id"), row.at("created_at"), row.at("actor_id"), action, row.at("object_id"),
              row.at("request_id"), "failure", "", "", reason});
  } catch (...) {
    // Cross-process append uses one Win32 file write in the platform helper below.
    std::ostringstream line;
    for (const auto& key : {"id", "created_at", "actor_id", "action", "object_id", "request_id",
                            "result", "reason"}) {
      line << row.at(key) << '\t';
    }
    line << '\n';
    AppendFailure(directory_ / "failures.tsv", line.str());
  }
}

Rows Service::Logs(const std::string& token, const Row& filters) {
  std::lock_guard lock(mutex_);
  auto actor = Require(token);
  Need(actor.admin, "仅管理员可以查询日志", ErrorKind::kForbidden);
  Rows rows = db_.Query("SELECT * FROM logs ORDER BY created_at DESC,id DESC");
  std::ifstream input(directory_ / "failures.tsv");
  std::string line;
  while (std::getline(input, line)) {
    Row row;
    std::istringstream stream(line);
    for (const auto& key : {"id", "created_at", "actor_id", "action", "object_id", "request_id",
                            "result", "reason"}) {
      std::getline(stream, row[key], '\t');
    }
    row["source"] = "local_fallback";
    rows.push_back(row);
  }
  Rows result;
  for (const auto& row : rows) {
    bool match = true;
    for (const auto& key : {"actor_id", "object_id", "action", "result"}) {
      auto value = Field(filters, key);
      if (!value.empty() && Field(row, key) != value) {
        match = false;
      }
    }
    if (!Field(filters, "start").empty() &&
        std::stoll(row.at("created_at")) < DateStart(filters.at("start"))) {
      match = false;
    }
    if (!Field(filters, "end").empty() &&
        std::stoll(row.at("created_at")) >= DateStart(filters.at("end")) + 86400) {
      match = false;
    }
    if (match) {
      result.push_back(row);
    }
  }
  return result;
}
}  // namespace market
