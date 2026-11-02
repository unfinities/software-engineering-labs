// Copyright 2026 CampusMarket contributors
#include "market/ui.h"

#include <commctrl.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "market/security.h"
#include "market/service.h"

namespace market {
namespace {
HINSTANCE g_instance;
HFONT g_font;
HFONT g_heading;
std::string Value(const Row& row, const std::string& key) {
  auto it = row.find(key);
  return it == row.end() ? "" : it->second;
}
std::string State(const std::string& state) {
  static const Row names{{"draft", "草稿"},
                         {"pending", "待审核"},
                         {"rejected", "已驳回"},
                         {"published", "已发布"},
                         {"reserved", "已预约"},
                         {"handover", "待交接"},
                         {"sold", "已售出"},
                         {"offline", "已下架"},
                         {"waiting", "等待确认"},
                         {"cancelled", "已取消"},
                         {"terminated", "已终止"},
                         {"completed", "已完成"},
                         {"completed_count", "期间成交笔数"},
                         {"completed_amount", "期间成交金额（元）"}};
  return names.contains(state) ? names.at(state) : state;
}
HWND Control(HWND parent, const wchar_t* kind, const std::string& caption, int id, int x, int y,
             int width, int height, DWORD style = 0) {
  HWND handle = CreateWindowExW(kind == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, kind,
                                Wide(caption).c_str(), WS_CHILD | WS_VISIBLE | style, x, y, width,
                                height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                g_instance, nullptr);
  SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
  return handle;
}
std::string Read(HWND handle) {
  int length = GetWindowTextLengthW(handle);
  std::wstring text(length + 1, L'\0');
  GetWindowTextW(handle, text.data(), length + 1);
  text.resize(length);
  return Utf8(text);
}
void Set(HWND handle, const std::string& text) { SetWindowTextW(handle, Wide(text).c_str()); }
void ErrorBox(HWND parent, const std::exception& error) {
  MessageBoxW(parent, Wide(error.what()).c_str(), L"操作未完成", MB_OK | MB_ICONWARNING);
}

struct Input {
  std::string key;
  std::string label;
  std::string value;
  bool secret = false;
  bool multiline = false;
  bool readonly = false;
};
struct Form {
  HWND window = nullptr;
  std::vector<Input> inputs;
  std::vector<HWND> edits;
  std::vector<std::pair<std::string, std::function<bool(const Row&)>>> buttons;
  std::function<std::string()> live_details;
  HWND details = nullptr;
  HWND notice = nullptr;
  bool done = false;
  static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto* form = reinterpret_cast<Form*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      form = static_cast<Form*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
      form->window = window;
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(form));
    }
    if (!form) {
      return DefWindowProcW(window, message, w, l);
    }
    if (message == WM_CLOSE) {
      form->done = true;
      return 0;
    }
    if (message == WM_TIMER && form->live_details) {
      try {
        std::string fresh = form->live_details();
        if (Read(form->details) != fresh) {
          Set(form->details, fresh);
        }
      } catch (const std::exception&) {
        Set(form->details, "当前无权读取详情，请关闭后刷新");
      }
      return 0;
    }
    if (message == WM_COMMAND && HIWORD(w) == BN_CLICKED) {
      int index = LOWORD(w) - 200;
      if (index >= 0 && index < static_cast<int>(form->buttons.size())) {
        Row fields;
        for (size_t i = 0; i < form->inputs.size(); ++i) {
          fields[form->inputs[i].key] = Read(form->edits[i]);
        }
        try {
          if (form->buttons[index].second(fields)) {
            form->done = true;
          }
        } catch (const std::exception& error) {
          Set(form->notice, error.what());
        }
        return 0;
      }
    }
    return DefWindowProcW(window, message, w, l);
  }
  void Show(HWND parent, const std::string& title) {
    int height = 110;
    if (live_details) {
      height += 245;
    }
    for (const auto& input : inputs) {
      height += input.multiline ? 128 : 64;
    }
    height = std::min(height + 90, 900);
    window = CreateWindowExW(WS_EX_DLGMODALFRAME, L"MarketForm", Wide(title).c_str(),
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                             760, height, parent, nullptr, g_instance, this);
    int y = 15;
    if (live_details) {
      details = Control(window, L"EDIT", "", 150, 20, y, 700, 228,
                        ES_MULTILINE | ES_READONLY | WS_VSCROLL);
      Set(details, live_details());
      SetTimer(window, 1, 1000, nullptr);
      y += 245;
    }
    for (size_t i = 0; i < inputs.size(); ++i) {
      const auto& input = inputs[i];
      Control(window, L"STATIC", input.label, 0, 20, y, 700, 23);
      DWORD style = WS_TABSTOP | ES_AUTOHSCROLL;
      if (input.secret) {
        style |= ES_PASSWORD;
      }
      if (input.multiline) {
        style = WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
      }
      if (input.readonly) {
        style |= ES_READONLY;
      }
      auto edit = Control(window, L"EDIT", input.value, static_cast<int>(100 + i), 20, y + 23, 700,
                          input.multiline ? 88 : 28, style);
      SendMessageW(edit, EM_SETLIMITTEXT, 6000, 0);
      edits.push_back(edit);
      y += input.multiline ? 128 : 64;
    }
    notice = Control(window, L"STATIC", "", 0, 20, y, 700, 45);
    y += 44;
    for (size_t i = 0; i < buttons.size(); ++i) {
      Control(window, L"BUTTON", buttons[i].first, static_cast<int>(200 + i),
              20 + static_cast<int>(i) * 170, y, 160, 32, WS_TABSTOP);
    }
    EnableWindow(parent, FALSE);
    ShowWindow(window, SW_SHOW);
    if (!edits.empty()) {
      SetFocus(edits[0]);
    }
    MSG message{};
    while (!done && GetMessageW(&message, nullptr, 0, 0) > 0) {
      if (!IsDialogMessageW(window, &message)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
    KillTimer(window, 1);
    DestroyWindow(window);
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
  }
};

class Application {
 public:
  explicit Application(const std::filesystem::path& directory) : service_(directory) {
#ifdef MARKET_UI_SMOKE
    // Separate opt-in executable. Real login is still verified by Service.
    std::ifstream input(directory / "demo-access.txt");
    std::string ignored, secret;
    for (int i = 0; i < 3; ++i) {
      std::getline(input, ignored);
    }
    std::getline(input, secret);
    session_ = service_.Login("demo_admin", secret);
#endif
  }
  int Run();
  static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM w, LPARAM l);

 private:
  void Build();
  void Layout();
  void Navigate(int page);
  void Refresh();
  void Authenticate(bool registration);
  void EditItem(bool create);
  void Details();
  void ItemAction(const std::string& action);
  void TradeDetails(const std::string& id);
  void SetTable(const Rows& rows, const std::vector<std::pair<std::string, std::string>>& columns);
  Row Selected();
  std::string ItemText(const Row& item);
  std::string ReservationText(const Row& reservation);
  std::string Execute(const std::string& action, const Row& row, const Row& fields = {});
  Service service_;
  Session session_;
  HWND window_ = nullptr, title_ = nullptr, identity_ = nullptr, list_ = nullptr, status_ = nullptr;
  HWND keyword_ = nullptr, category_ = nullptr, low_ = nullptr, high_ = nullptr, state_ = nullptr;
  HWND start_ = nullptr, end_ = nullptr;
  std::vector<HWND> filters_, actions_, navigation_;
  Rows rows_;
  int page_ = 0, offset_ = 0;
};

LRESULT CALLBACK Application::Procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
  auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    app = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
    app->window_ = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
  }
  if (!app) {
    return DefWindowProcW(window, message, w, l);
  }
  if (message == WM_SIZE) {
    if (app->list_) {
      app->Layout();
    }
    return 0;
  }
  if (message == WM_GETMINMAXINFO) {
    auto* info = reinterpret_cast<MINMAXINFO*>(l);
    info->ptMinTrackSize = {1150, 740};
    return 0;
  }
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  if (message == WM_NOTIFY && reinterpret_cast<NMHDR*>(l)->code == NM_DBLCLK) {
    try {
      app->Details();
    } catch (const std::exception& error) {
      ErrorBox(window, error);
    }
    return 0;
  }
  if (message == WM_COMMAND && HIWORD(w) == BN_CLICKED) {
    int id = LOWORD(w);
    try {
      if (id >= 10 && id <= 17) {
        app->Navigate(id - 10);
      } else if (id == 1) {
        app->Authenticate(false);
      } else if (id == 2) {
        app->Authenticate(true);
      } else if (id == 3) {
        app->service_.Logout(app->session_.token);
        app->session_ = {};
        app->Navigate(0);
      } else if (id == 4) {
        app->offset_ = 0;
        app->Refresh();
      } else if (id == 5) {
        if (app->offset_ > 0) {
          --app->offset_;
        }
        app->Refresh();
      } else if (id == 6) {
        ++app->offset_;
        app->Refresh();
      } else if (id == 30) {
        app->Details();
      } else if (id == 31) {
        app->EditItem(true);
      } else if (id == 32) {
        app->EditItem(false);
      } else if (id >= 33 && id <= 39) {
        static const char* actions[] = {"submit",      "withdraw", "takedown", "approve",
                                        "reject_item", "violate",  "unlock"};
        app->ItemAction(actions[id - 33]);
      } else if (id == 40) {
        Form form;
        form.inputs = {{"id", "预约完整编号", ""}};
        form.buttons.push_back({"查看预约", [&](const Row& fields) {
                                  app->TradeDetails(fields.at("id"));
                                  return true;
                                }});
        form.Show(window, "管理员预约查询");
      }
    } catch (const std::exception& error) {
      ErrorBox(window, error);
    }
    return 0;
  }
  return DefWindowProcW(window, message, w, l);
}

void Application::Build() {
  title_ = Control(window_, L"STATIC", "校园二手交易", 0, 24, 22, 380, 42);
  SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(g_heading), TRUE);
  identity_ = Control(window_, L"STATIC", "访客", 0, 450, 30, 380, 30);
  Control(window_, L"BUTTON", "登录", 1, 850, 24, 90, 32, WS_TABSTOP);
  Control(window_, L"BUTTON", "注册", 2, 950, 24, 90, 32, WS_TABSTOP);
  Control(window_, L"BUTTON", "退出登录", 3, 1050, 24, 110, 32, WS_TABSTOP);
  const char* nav[] = {"商品广场",   "我的商品", "我的预约", "收到的预约",
                       "审核与违规", "成交统计", "操作日志", "使用说明"};
  for (int i = 0; i < 8; ++i) {
    navigation_.push_back(
        Control(window_, L"BUTTON", nav[i], 10 + i, 22, 96 + i * 55, 160, 42, WS_TABSTOP));
  }
  keyword_ = Control(window_, L"EDIT", "", 50, 210, 122, 200, 29, WS_TABSTOP | ES_AUTOHSCROLL);
  category_ = Control(window_, L"EDIT", "", 51, 425, 122, 110, 29, WS_TABSTOP | ES_AUTOHSCROLL);
  low_ = Control(window_, L"EDIT", "", 52, 550, 122, 90, 29, WS_TABSTOP | ES_AUTOHSCROLL);
  high_ = Control(window_, L"EDIT", "", 53, 655, 122, 90, 29, WS_TABSTOP | ES_AUTOHSCROLL);
  state_ = Control(window_, L"COMBOBOX", "", 54, 760, 122, 150, 400, WS_TABSTOP | CBS_DROPDOWNLIST);
  const char* states[] = {"全部状态", "草稿",   "待审核", "已驳回", "已发布",
                          "已预约",   "待交接", "已售出", "已下架"};
  for (auto text : states) {
    SendMessageW(state_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Wide(text).c_str()));
  }
  SendMessageW(state_, CB_SETCURSEL, 0, 0);
  int x[] = {210, 425, 550, 655, 760};
  int label_widths[] = {200, 110, 90, 90, 150};
  const char* labels[] = {"关键词或操作者编号", "分类或动作", "最低价", "最高价", "商品状态"};
  for (int i = 0; i < 5; ++i) {
    filters_.push_back(Control(window_, L"STATIC", labels[i], 0, x[i], 97, label_widths[i], 23));
  }
  Control(window_, L"BUTTON", "查询 / 刷新", 4, 930, 119, 130, 32, WS_TABSTOP);
  Control(window_, L"STATIC", "开始日期", 0, 210, 164, 100, 24);
  start_ =
      Control(window_, L"EDIT", Today().substr(0, 7) + "-01", 55, 300, 162, 150, 29, WS_TABSTOP);
  Control(window_, L"STATIC", "结束日期", 0, 470, 164, 100, 24);
  end_ = Control(window_, L"EDIT", Today(), 56, 560, 162, 150, 29, WS_TABSTOP);
  const char* buttons[] = {"详情 / 处理", "新建商品", "编辑",     "提交审核", "撤回草稿",    "下架",
                           "审核通过",    "驳回",     "违规处理", "解除锁定", "预约编号查询"};
  for (int i = 0; i < 11; ++i) {
    actions_.push_back(Control(window_, L"BUTTON", buttons[i], 30 + i, 210 + (i % 7) * 132,
                               214 + (i / 7) * 40, 122, 31, WS_TABSTOP));
  }
  list_ = Control(window_, WC_LISTVIEWW, "", 70, 210, 307, 950, 400,
                  WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS);
  ListView_SetExtendedListViewStyle(list_,
                                    LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
  Control(window_, L"BUTTON", "上一页", 5, 210, 720, 100, 30, WS_TABSTOP);
  Control(window_, L"BUTTON", "下一页", 6, 325, 720, 100, 30, WS_TABSTOP);
  status_ = Control(window_, L"STATIC", "", 0, 210, 765, 1000, 45);
  Layout();
  Navigate(0);
}

void Application::Layout() {
  RECT rect{};
  GetClientRect(window_, &rect);
  MoveWindow(list_, 210, 307, std::max(900L, rect.right - 235), std::max(260L, rect.bottom - 410),
             TRUE);
  MoveWindow(GetDlgItem(window_, 5), 210, rect.bottom - 90, 100, 30, TRUE);
  MoveWindow(GetDlgItem(window_, 6), 325, rect.bottom - 90, 100, 30, TRUE);
  MoveWindow(status_, 210, rect.bottom - 48, rect.right - 230, 40, TRUE);
}

void Application::Navigate(int page) {
  if (page > 0 && page < 7 && session_.token.empty()) {
    throw Error(ErrorKind::kAuth, "请先登录");
  }
  if ((page == 4 || page == 6) && !session_.admin) {
    throw Error(ErrorKind::kForbidden, "该模块仅管理员可用");
  }
  page_ = page;
  offset_ = 0;
  Set(filters_[0], page == 6 ? "操作者编号" : "关键词 标题或描述");
  Set(filters_[1], page == 6 ? "操作类型" : "分类");
  Set(filters_[2], page == 6 ? "对象编号" : "最低价");
  Set(filters_[3], page == 6 ? "结果代码" : "最高价");
  Set(keyword_, "");
  Set(category_, "");
  Set(low_, "");
  Set(high_, "");
  SendMessageW(state_, CB_SETCURSEL, 0, 0);
  for (size_t i = 0; i < actions_.size(); ++i) {
    bool show = i == 0 && page < 5;
    if (page == 1 && i >= 1 && i <= 5) {
      show = true;
    }
    if (page == 4 && i >= 6 && i <= 9) {
      show = true;
    }
    if (page == 6 && i == 10) {
      show = true;
    }
    ShowWindow(actions_[i], show ? SW_SHOW : SW_HIDE);
  }
  Set(identity_, session_.token.empty()
                     ? "访客 · 可浏览公开商品"
                     : session_.username + (session_.admin ? " · 管理员" : " · 普通用户"));
  Refresh();
}

void Application::SetTable(const Rows& rows,
                           const std::vector<std::pair<std::string, std::string>>& columns) {
  rows_ = rows;
  SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
  ListView_DeleteAllItems(list_);
  while (ListView_DeleteColumn(list_, 0)) {
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    auto label = Wide(columns[i].second);
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = label.data();
    column.cx = i == 0 ? 230 : (columns[i].first == "reason" ? 260 : 145);
    ListView_InsertColumn(list_, static_cast<int>(i), &column);
  }
  for (size_t i = 0; i < rows.size(); ++i) {
    for (size_t j = 0; j < columns.size(); ++j) {
      auto key = columns[j].first;
      auto text = Value(rows[i], key);
      if (key == "status" || key == "metric") {
        text = State(text);
      }
      if (key == "price_fen") {
        text = Money(std::stoll(text));
      }
      if (key == "created_at" && !text.empty()) {
        text = BeijingTime(std::stoll(text));
      }
      if (key == "locked") {
        text = text == "1" ? "锁定" : "正常";
      }
      if (key == "id") {
        text = text.substr(0, 12);
      }
      auto wide = Wide(text);
      if (j == 0) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = wide.data();
        ListView_InsertItem(list_, &item);
      } else {
        ListView_SetItemText(list_, static_cast<int>(i), static_cast<int>(j), wide.data());
      }
    }
  }
  SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
  RedrawWindow(list_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void Application::Refresh() {
  auto start = std::chrono::steady_clock::now();
  if (page_ == 0 || page_ == 1 || page_ == 4) {
    Search search;
    search.scope = page_ == 0 ? "public" : (page_ == 1 ? "mine" : "admin");
    search.keyword = Read(keyword_);
    search.category = Read(category_);
    search.minimum = Read(low_);
    search.maximum = Read(high_);
    search.page = offset_;
    int selected = static_cast<int>(SendMessageW(state_, CB_GETCURSEL, 0, 0));
    const char* states[] = {"",         "draft",    "pending", "rejected", "published",
                            "reserved", "handover", "sold",    "offline"};
    if (selected >= 0 && selected <= 8) {
      search.status = states[selected];
    }
    SetTable(service_.Items(session_.token, search), {{"title", "商品标题"},
                                                      {"category", "分类"},
                                                      {"price_fen", "价格（元）"},
                                                      {"status", "状态"},
                                                      {"locked", "锁定"},
                                                      {"id", "编号前缀"}});
  } else if (page_ == 2 || page_ == 3) {
    SetTable(service_.Reservations(session_.token, page_ == 3), {{"title", "预约商品快照"},
                                                                 {"price_fen", "预约价格"},
                                                                 {"status", "预约状态"},
                                                                 {"reason", "结束原因"},
                                                                 {"created_at", "预约时间"}});
  } else if (page_ == 5) {
    SetTable(service_.Statistics(session_.token, Read(start_), Read(end_)),
             {{"metric", session_.admin ? "全平台统计" : "本人卖出统计"}, {"value", "数量或金额"}});
  } else if (page_ == 6) {
    Row filters{{"actor_id", Read(keyword_)}, {"object_id", Read(low_)}, {"result", Read(high_)},
                {"action", Read(category_)},  {"start", Read(start_)},   {"end", Read(end_)}};
    SetTable(service_.Logs(session_.token, filters), {{"action", "操作类型"},
                                                      {"result", "结果"},
                                                      {"reason", "结果原因代码"},
                                                      {"created_at", "北京时间"},
                                                      {"object_id", "对象完整编号"},
                                                      {"actor_id", "操作者编号"}});
  } else {
    SetTable(
        {{{"step", "1  注册账号"},
          {"help", "首次在注册页生成一次性邀请码以初始化管理员；普通注册无需邀请码"}},
         {{"step", "2  发布商品"}, {"help", "卖家保存草稿 → 提交审核 → 管理员通过后公开"}},
         {{"step", "3  线下交易"}, {"help", "买家预约 → 卖家接受 → 线下交接 → 卖家二次确认完成"}},
         {{"step", "4  多窗口"},
          {"help", "再次启动程序即可独立登录；操作前刷新以获取其他窗口的新状态"}},
         {{"step", "5  隐私与数据"},
          {"help", "结束预约后联系方式关闭；本机数据位于 LocalAppData/CampusMarket"}},
         {{"step", "6  日期与筛选"},
          {"help", "日期 YYYY-MM-DD，包含结束日全天；商品分页每页20条；分类精确匹配"}}},
        {{"step", "使用步骤"}, {"help", "说明"}});
    ListView_SetColumnWidth(list_, 1, 740);
  }
  auto milliseconds =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  Set(status_, "第 " + std::to_string(offset_ + 1) + " 页 · " + std::to_string(rows_.size()) +
                   " 条 · 查询至文字列表重绘 " + std::to_string(milliseconds).substr(0, 6) +
                   " ms · 价格单位元，时间为北京时间");
}

Row Application::Selected() {
  int selected = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
  if (selected < 0 || selected >= static_cast<int>(rows_.size())) {
    throw Error(ErrorKind::kInput, "请先选择一条记录");
  }
  return rows_[selected];
}

void Application::Authenticate(bool registration) {
  Form form;
  form.inputs = {{"username", "用户名 至少两个字符 不含空白", ""},
                 {"password", "密码 10至128字节", "", true}};
  if (registration) {
    form.inputs.push_back({"invitation", "管理员邀请码 普通账号留空", "", true});
  }
  form.buttons.push_back(
      {registration ? "注册账号" : "登录", [&](const Row& fields) {
         if (registration) {
           service_.Register(fields.at("username"), fields.at("password"), fields.at("invitation"));
           MessageBoxW(form.window, L"注册成功，请使用新账号登录", L"完成", MB_OK);
         } else {
           auto fresh = service_.Login(fields.at("username"), fields.at("password"));
           service_.Logout(session_.token);
           session_ = fresh;
         }
         return true;
       }});
  if (registration) {
    form.buttons.push_back(
        {"初始化管理员", [&](const Row&) {
           auto code = service_.BootstrapInvite();
           Set(form.edits[2], code);
           Set(form.notice, "已生成并填入一次性邀请码，仅数据库保存其摘要。填写账号密码后注册。");
           return false;
         }});
  }
  form.Show(window_, registration ? "注册账号" : "登录账号");
  Navigate(0);
}

std::string Application::Execute(const std::string& action, const Row& row, const Row& fields) {
  Row payload = fields;
  if (row.contains("version")) {
    payload["version"] = row.at("version");
  }
  Command command{action, Value(row, "id"), RandomId(), payload};
  // A retry after a storage/busy error reuses this exact command and request ID.
  for (;;) {
    try {
      return service_.Execute(session_.token, command);
    } catch (const Error& error) {
      if (error.kind != ErrorKind::kStorage && error.kind != ErrorKind::kProcessing) {
        throw;
      }
      if (MessageBoxW(window_, Wide(error.what()).c_str(), L"重试同次操作",
                      MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY) {
        throw;
      }
    }
  }
}

void Application::EditItem(bool create) {
  Row row;
  if (!create) {
    row = service_.Item(session_.token, Selected().at("id"));
  }
  Form form;
  form.inputs = {
      {"title", "商品标题", Value(row, "title")},
      {"category", "分类", Value(row, "category")},
      {"description", "商品描述", Value(row, "description"), false, true},
      {"quality", "新旧程度", Value(row, "quality")},
      {"price", "价格 元 可留空保存草稿",
       row.empty() || row.at("price_fen") == "0" ? "" : Money(std::stoll(row.at("price_fen")))},
      {"location", "线下交接地点", Value(row, "location")}};
  form.buttons.push_back({"保存草稿", [&](const Row& fields) {
                            Execute("save", row, fields);
                            return true;
                          }});
  form.Show(window_, create ? "新建商品" : "编辑商品 需处于可编辑状态");
  Refresh();
}

std::string Application::ItemText(const Row& item) {
  std::string text = "商品编号：" + item.at("id") + "\r\n标题：" + item.at("title") + "\r\n状态：" +
                     State(item.at("status")) + "  " +
                     (item.at("locked") == "1" ? "违规锁定" : "") + "\r\n价格：" +
                     Money(std::stoll(item.at("price_fen"))) + " 元\r\n分类：" +
                     item.at("category") + "\r\n新旧：" + item.at("quality") + "\r\n交接地点：" +
                     item.at("location") + "\r\n描述：" + item.at("description");
  if (!session_.token.empty() && (session_.admin || session_.user_id == item.at("seller_id"))) {
    for (const auto& row : service_.History(session_.token, item.at("id"))) {
      text += "\r\n处理历史：" + row.at("action") + "  " + row.at("reason");
    }
  }
  return text;
}

void Application::Details() {
  if (page_ == 2 || page_ == 3) {
    TradeDetails(Selected().at("id"));
    Refresh();
    return;
  }
  if (page_ != 0 && page_ != 1 && page_ != 4) {
    return;
  }
  auto item = service_.Item(session_.token, Selected().at("id"));
  Form form;
  form.live_details = [&, id = item.at("id")] {
    return ItemText(service_.Item(session_.token, id));
  };
  if (page_ == 0 && !session_.token.empty() && item.at("seller_id") != session_.user_id &&
      item.at("status") == "published") {
    form.inputs = {{"contact", "本人联系方式 预约成功后向卖家提供", ""}};
    form.buttons.push_back({"发起预约", [&](const Row& fields) {
                              Execute("reserve", item, fields);
                              return true;
                            }});
  } else {
    form.buttons.push_back({"关闭", [](const Row&) { return true; }});
  }
  form.Show(window_, "商品详情");
  Refresh();
}

void Application::ItemAction(const std::string& action) {
  auto row = service_.Item(session_.token, Selected().at("id"));
  if (action == "submit" || action == "withdraw" || action == "approve") {
    Execute(action, row);
  } else {
    Form form;
    form.live_details = [&] { return ItemText(service_.Item(session_.token, row.at("id"))); };
    form.inputs = {{"reason", "操作原因 有效预约下架 拒绝审核 违规与解锁时必填", "", false, true}};
    form.buttons.push_back({"确认操作", [&](const Row& fields) {
                              Execute(action, row, fields);
                              return true;
                            }});
    form.Show(window_, "商品处理");
  }
  Refresh();
}

std::string Application::ReservationText(const Row& reservation) {
  std::string text = "预约编号：" + reservation.at("id") + "\r\n商品快照：" +
                     reservation.at("title") + "\r\n预约价格：" +
                     Money(std::stoll(reservation.at("price_fen"))) + " 元\r\n交接地点：" +
                     reservation.at("location") + "\r\n预约状态：" +
                     State(reservation.at("status")) + "\r\n结束原因：" + reservation.at("reason");
  if (reservation.contains("buyer_contact")) {
    text += "\r\n买家联系方式：" + reservation.at("buyer_contact");
  }
  if (reservation.contains("seller_contact")) {
    text += "\r\n卖家联系方式：" + reservation.at("seller_contact");
  }
  if (!reservation.contains("buyer_contact") && !reservation.contains("seller_contact")) {
    text += "\r\n当前阶段不提供联系方式";
  }
  return text;
}

void Application::TradeDetails(const std::string& id) {
  auto reservation = service_.Reservation(session_.token, id);
  bool seller = reservation.at("seller_id") == session_.user_id;
  bool buyer = reservation.at("buyer_id") == session_.user_id;
  const auto& state = reservation.at("status");
  Form form;
  form.live_details = [&] { return ReservationText(service_.Reservation(session_.token, id)); };
  if ((buyer || seller) && (state == "waiting" || state == "handover")) {
    form.inputs = {{"reason", "取消或拒绝原因 待交接时取消必填", "", false, true}};
    if (seller && state == "waiting") {
      form.inputs.push_back({"contact", "卖家本人联系方式 接受时必填", ""});
    }
    auto action = [&](const std::string& name, const std::string& caption) {
      form.buttons.push_back(
          {caption, [&, name](const Row& fields) {
             Row payload = fields;
             if (name == "complete") {
               if (MessageBoxW(form.window, L"请确认已经完成线下交接，完成后不可撤销",
                               L"确认完成交易",
                               MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
                 return false;
               }
               payload["confirmed"] = "yes";
             }
             Execute(name, reservation, payload);
             return true;
           }});
    };
    if (seller && state == "waiting") {
      action("accept", "接受预约");
      action("reject", "拒绝预约");
    }
    if (buyer || state == "handover") {
      action("cancel", "取消预约");
    }
    if (seller && state == "handover") {
      action("complete", "确认完成");
    }
  } else {
    form.buttons.push_back({"关闭", [](const Row&) { return true; }});
  }
  form.Show(window_, "预约详情与处理");
}

int Application::Run() {
  window_ =
      CreateWindowExW(0, L"CampusMarket", L"校园二手交易系统  Campus Market", WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, 1300, 880, nullptr, nullptr, g_instance, this);
  Build();
  ShowWindow(window_, SW_SHOW);
  UpdateWindow(window_);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(window_, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  return static_cast<int>(message.wParam);
}
}  // namespace

int RunApplication(HINSTANCE instance, const std::filesystem::path& directory) {
  g_instance = instance;
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&controls);
  g_font =
      CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
  g_heading =
      CreateFontW(-28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
  WNDCLASSW main_class{};
  main_class.hInstance = instance;
  main_class.lpfnWndProc = Application::Procedure;
  main_class.lpszClassName = L"CampusMarket";
  main_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  main_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&main_class);
  WNDCLASSW form_class = main_class;
  form_class.lpfnWndProc = Form::Procedure;
  form_class.lpszClassName = L"MarketForm";
  form_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  RegisterClassW(&form_class);
  int result = Application(directory).Run();
  DeleteObject(g_font);
  DeleteObject(g_heading);
  return result;
}
}  // namespace market
