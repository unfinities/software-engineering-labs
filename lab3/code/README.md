# CampusMarket Windows C++桌面应用

要求 Windows 10或11 x64、C++20 编译器和 CMake 3.24以上。已在 Windows 上用 w64devkit 2.11.0 / GCC 16.2.0、CMake 4.4.5、SQLite 3.54.1 实际构建。MSVC构建另由仓库 Windows CI 验证。

## 构建和运行

在已设置编译器 PATH 的 PowerShell 中执行：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
.\build\CampusMarket.exe
```

Visual Studio 开发者终端可执行 `cmake -S . -B build`、`cmake --build build --config Release`、`ctest --test-dir build -C Release --output-on-failure`。应用在 `build/Release/CampusMarket.exe`。

首次构建从 SQLite 官方网站下载校验固定 SHA3-256 的源码。离线时提供 `-DSQLITE_SOURCE_DIR=包含sqlite3.c和sqlite3.h的目录`；本地待提交目录将带 `vendor/sqlite`，CMake也会自动使用该目录。SQLite第三方代码不计入自主实现行数。程序使用系统 Win32、CNG、DPAPI，无 Qt 或其他运行时安装要求；MinGW交付版本静态链接 C++运行库。

## 使用

1. 双击启动。注册窗口中点击“初始化管理员”，填用户名及自己的密码后注册。邀请码只在该窗口自动填入，数据库仅保存摘要；生成新邀请码会让尚未使用的旧邀请码失效，管理员初始化完成后不能再生成。
2. 分别注册普通卖家和买家。一个普通账号兼任买卖，无固定内置凭据。
3. 卖家在“我的商品”中新建并保存草稿，选中后提交审核。管理员在“审核与违规”中选中并通过，商品进入广场。
4. 买家双击商品详情，填写本人联系方式预约。卖家在“收到的预约”中接受并提供联系方式，线下交接后由卖家二次确认完成。
5. 取消原因在预约详情页内填写。预约结束后双方的详情查询不再返回联系方式；管理员按管理需要可用日志页“预约编号查询”查阅私有记录。
6. 多开程序可独立登录，共享同一数据库。跨窗口操作后点击“查询/刷新”；旧页面提交带版本号，冲突时保留输入并提示刷新。

数据默认在 `%LOCALAPPDATA%/CampusMarket`。可通过 `CampusMarket.exe --data-dir "独立目录"` 指定同机测试目录。不要把运行数据目录、数据库、故障日志、演示访问文件上传到公开仓库。DPAPI绑定同一Windows用户和机器，复制数据库到其他Windows账号不能直接解密；本机有文件访问权限者属于可信部署边界。本项目不提供对抗本机操作系统管理员的隔离。

商品筛选包含关键词、分类、价格、状态；分类精确匹配，关键词按标题或描述包含匹配，其他条件同时满足。每页20条，价格按分保存。统计与日志日期格式 YYYY-MM-DD，按北京时间解释并包含结束日全天。日志类型使用代码如 reserve、complete，结果代码为 success 或 failure。

## 测试与风格

```powershell
python -m pip install cpplint==2.0.2 clang-format==23.1.3
python ../../scripts/check_lab3.py --lint
```

检查采用Google风格、100列和C++20。cpplint的旧标准禁止项关闭，头文件路径按CMake include目录解析；一处集合初始化续行使用定点NOLINT处理检查器误报，其余规则继续检查。

`market_tests` 是独立测试程序，使用随机临时账号和隔离数据库，不操作正常数据。默认测试包括10轮每轮20连接竞争、取消与完成竞争、重复请求、权限、阶段隐私、事务故障及重启。可用 `market_tests --seed "空目录"` 创建虚构演示库；随机密码只写入该本地目录的 demo-access.txt，不存在硬编码默认密码。

`MARKET_UI_SMOKE=ON`额外构建独立界面测试程序，读取其旁边 ui-test-data 演示库的随机访问记录；此自动登录逻辑不编入正常 CampusMarket.exe，运行包不含该测试程序。

代码许可与第三方：本课程工程未额外指定项目开源许可；SQLite采用其[公有领域声明](https://www.sqlite.org/copyright.html)。参考[SQLite事务](https://www.sqlite.org/lang_transaction.html)、[WAL](https://www.sqlite.org/wal.html)、[Windows DPAPI](https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata)、[cpplint](https://github.com/cpplint/cpplint)。
