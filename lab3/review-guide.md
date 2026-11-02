# 本人代码检查指南

状态：待本人实际阅读与运行。以下不是已完成记录，请逐文件检查后自行记录问题及判断。

1. 从include/market/service.h阅读接口，结合implementation.md与四张UML核对七模块。
2. 阅读src/service.cpp每一行，重点检查Authorize、Execute、ApplyItem、ApplyTrade、Reservation与Failure；核对所有状态与权限边界。
3. 阅读database.cpp的参数绑定、事务析构和错误映射；检查成功日志失败时业务不会单独保存。
4. 阅读security.cpp及main.cpp，检查随机盐、密码哈希、DPAPI、本机数据路径及无默认账号；确认测试入口不编入正常应用。
5. 阅读ui.cpp与全部头文件，亲自走一遍注册、草稿、审核、预约、接受、取消和卖家完成；核对二次确认、错误输入保留和页内原因。
6. 阅读tests/service_test.cpp及CMakeLists.txt；运行构建、CTest和风格检查，记录与当前环境相关的差异。性能正式验收尚未完成，不应填写成达标。
7. 在自己的报告中写出UML到代码的理解、AI生成代码是否符合预期、实际修正过程及自己的判断。仅凭本清单不能声明已逐行审查。

用户后续直接修改的源码和报告应保留；修订提交独立保存，不移动已有实验标签。
