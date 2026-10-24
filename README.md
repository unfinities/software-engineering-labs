# 校园二手交易系统 · 软件工程实验（2026 秋）

公开仓库：https://github.com/unfinities/software-engineering-labs-2026

当前进行实验 1：需求分析，选题为校园二手交易系统。已根据2026-10-24甲方第一版书面说明及第二轮七组答复，整理首次完整AI需求初稿：27条需求，26条有甲方材料支持，REQ-026因性能测试环境未固定保留待确认。本人审查尚待进行，暂不创建 lab1-final。

## 文件导航

| 文件 | 用途 |
| --- | --- |
| [requirements.md](requirements.md) | 27条首次完整AI需求，十字段及来源编号 |
| [chat.txt](chat.txt) | 访谈、AI 交互、人工审查三部分原始记录 |
| [lab1/report.md](lab1/report.md) | 可编辑的报告源文件 |
| [lab1/report.pdf](lab1/report.pdf) | 报告预览；最终版本须在本人审查及最终标签固定后生成 |
| [lab1/interview-summary.md](lab1/interview-summary.md) | 两轮书面沟通精简记录，保留七组跟进问答 |
| [lab1/materials/](lab1/materials/) | 经检查的甲方书面原材料，不含身份与凭据 |
| [docs/course-requirements.md](docs/course-requirements.md) | 从任务网页与模板核对的完成标准 |
| [docs/project-options.md](docs/project-options.md) | 五个允许选题及各自最低范围 |
| [docs/interview-guide.md](docs/interview-guide.md) | 正式访谈与跟进沟通提纲 |
| [docs/analysis-worksheets.md](docs/analysis-worksheets.md) | 权限、状态、数据、异常与验收工作表 |
| [docs/templates.md](docs/templates.md) | 十字段需求、审查记录与 AI 初稿提示词模板 |
| [docs/workflow.md](docs/workflow.md) | 从访谈、标签到最终打包的顺序 |
| [docs/publication-policy.md](docs/publication-policy.md) | 公开仓库的脱敏与同步规则 |

`private/`、`work/`、`dist/` 和原始下载材料均不纳入 Git。后续实验继续使用本仓库，在相应实验目录增补文档或代码。

## 下一步

1. 固定第一次完整AI初稿与全部输入输出，保存 lab1-ai-draft；不把先前通用框架当初稿。
2. 本人核对需求来源、遗漏、矛盾及验收，记录至少三项实质核对；AI候选意见不冒充人工审查。
3. 补充具体测试环境，或明确保留REQ-026待确认；待确认项不计入已确认数量。
4. 全部真实记录完成后提交文本并创建lab1-final，再补全报告版本信息和本地三文件提交包。

具体命令见 [操作流程](docs/workflow.md)。当前不得提前创建lab1-final，标签一经创建不得移动、删除或覆盖。

## 本地工具

需要 Python 3.10+ 和 Git。文档工具依赖在 requirements-tools.txt 中：

```console
python -m pip install -r requirements-tools.txt
git config core.hooksPath .githooks
python scripts/scan_public.py --tracked
python scripts/check_lab1.py
python scripts/build_report.py
```

check_lab1.py会列出未完成的人工审查等事项。报告重建后应检查版面。提交和推送钩子会进行敏感内容检查；GitHub Actions同时扫描提交历史。自动检查不能代替本人核对证据。

每次获授权的项目更新完成后，先检查、再提交，并推送到 origin/main。认证使用本机凭据管理器；仓库中不保存密码、令牌、私钥、带认证的 URL 或私有接口配置。提交者使用 GitHub noreply 邮箱。
