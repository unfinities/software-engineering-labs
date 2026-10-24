# 实验1提交仓库协作约定

- 用户已明确选择另建提交仓库，保留原仓库及标签。本仓库为https://github.com/unfinities/software-engineering-lab1-submission-2026；原仓库为https://github.com/unfinities/software-engineering-labs-2026，不得操作原仓库标签。
- 保留用户2026-10-24对chat.txt、requirements.md的修改，包括参与人姓名、版本标题和简化说明；只补充必要的当前仓库地址及真实后续过程，不恢复已删除文字。

- 本项目为用户的软件工程课程实验，公开 GitHub 仓库见 README.md。用户已授权此项目的后续更改在检查后提交并同步至同一仓库；不创建定时同步任务。
- 当前阶段：实验 1 校园二手交易系统；2026-10-24 两轮书面需求及跟进已整理，本人已于2026-10-24逐项核对三项并均保留，模型确认为GPT-6.1 Sol；完成最终文本和标签后，仅在之后更新报告，不移动实验标签。
- 不虚构甲方对话、访谈时间、已确认需求、人工审查意见或用户已经完成的操作。提纲使用 Q 编号，实际访谈才使用 INT 编号。
- requirements.md 的需求从 REQ-001 起，编号稳定，每条严格使用十个字段；未确认内容保持待确认。chat.txt 保持三部分结构，追加记录，不覆盖原始材料。
- 收到真实访谈材料后，先按 docs/workflow.md 保存完整 AI 初稿及交互，再创建 lab1-ai-draft。本人完成实质性审查后才创建 lab1-final。标签一旦存在不得移动、删除或覆盖。
- 不把原始 detail.htm、课程网页会话数据、下载 ZIP、原始录音、学号、未获用户保留指令的身份信息、密码、API 凭据、私钥、私有接口地址或生产数据提交至公开仓库。原始敏感材料留在被忽略的 private/；公开文本保留行号并用明确标记脱敏，不改变业务含义。
- 每次提交前人工查看差异并运行 python scripts/scan_public.py --staged；推送前检查历史。保持 .githooks 生效，不绕过检查。
- 不为了满足条数编造需求；不因检查工具报未完成就伪造证据。REQ-026的性能口径已获甲方说明，但设备、网络等环境未固定，整体保持待确认。
- 报告源文件为 lab1/report.md，生成 lab1/report.pdf 后需查看页面。chat.txt参与人姓名按用户本次修改保留；学号仅在本地个人报告填写；提交包输出在 private/submission/，不提交公开 Git。
- 最终压缩包仅含 实验1_学号_姓名/ 下的 report.pdf、requirements.md、chat.txt；两个文本必须逐字节等于 lab1-final。
- 没有用户明确调用时，不运行个人安装的 mattpocock/skills 工作流。
