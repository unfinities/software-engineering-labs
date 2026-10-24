# 软件工程实验（2026 秋）

公开仓库：https://github.com/unfinities/software-engineering-labs-2026

当前进行实验 1：需求分析。甲方尚未选题，尚未开展访谈。本仓库先建立五个规定选题均可使用的通用框架；当前没有已确认业务需求，也没有实验初稿或最终标签。

## 文件导航

| 文件 | 用途 |
| --- | --- |
| [requirements.md](requirements.md) | 正式需求列表的入口；访谈后从 REQ-001 开始 |
| [chat.txt](chat.txt) | 访谈、AI 交互、人工审查三部分原始记录 |
| [lab1/report.md](lab1/report.md) | 可编辑的报告源文件 |
| [lab1/report.pdf](lab1/report.pdf) | 当前报告框架预览；尚不能作为完成版提交 |
| [docs/course-requirements.md](docs/course-requirements.md) | 从任务网页与模板核对的完成标准 |
| [docs/project-options.md](docs/project-options.md) | 五个允许选题及各自最低范围 |
| [docs/interview-guide.md](docs/interview-guide.md) | 正式访谈与跟进沟通提纲 |
| [docs/analysis-worksheets.md](docs/analysis-worksheets.md) | 权限、状态、数据、异常与验收工作表 |
| [docs/templates.md](docs/templates.md) | 十字段需求、审查记录与 AI 初稿提示词模板 |
| [docs/workflow.md](docs/workflow.md) | 从访谈、标签到最终打包的顺序 |
| [docs/publication-policy.md](docs/publication-policy.md) | 公开仓库的脱敏与同步规则 |

`private/`、`work/`、`dist/` 和原始下载材料均不纳入 Git。后续实验继续使用本仓库，在相应实验目录增补文档或代码。

## 下一步

1. 由甲方从五个项目中选择一个，明确参与角色与范围。
2. 按访谈提纲进行 INT-01 正式访谈，实际对话逐行编号；进行 INT-02 跟进沟通。
3. 将完整可公开材料交给 AI，保存第一次完整结果到 requirements.md，同时保存完整输入与输出到 chat.txt，然后提交并创建 lab1-ai-draft。
4. 由本人审查实际需求，记录至少三项实质性核对，再提交最终文本并创建 lab1-final。
5. 两个标签固定后，完成报告、验证文本与 lab1-final 完全一致，生成规定的三文件压缩包。

具体命令见 [操作流程](docs/workflow.md)。当前不要创建上述两个实验标签。

## 本地工具

需要 Python 3.10+ 和 Git。文档工具依赖在 requirements-tools.txt 中：

```console
python -m pip install -r requirements-tools.txt
git config core.hooksPath .githooks
python scripts/scan_public.py --tracked
python scripts/check_lab1.py
python scripts/build_report.py
```

check_lab1.py 在当前准备阶段会列出尚未完成的实验项，这是预期结果。报告重建后应检查版面。提交和推送钩子会进行敏感内容检查；GitHub Actions 同时扫描提交历史。自动检查不能代替人工阅读公开差异。

每次获授权的项目更新完成后，先检查、再提交，并推送到 origin/main。认证使用本机凭据管理器；仓库中不保存密码、令牌、私钥、带认证的 URL 或私有接口配置。提交者使用 GitHub noreply 邮箱。
