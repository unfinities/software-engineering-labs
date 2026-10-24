# 校园二手交易系统 · 软件工程实验（2026 秋）

公开仓库：https://github.com/unfinities/software-engineering-labs-2026

当前进行实验 1：需求分析，选题为校园二手交易系统。已根据2026-10-24甲方第一版书面说明及第二轮七组答复，整理审查后的最终需求：27条需求，26条有甲方材料支持，REQ-026因性能测试环境未固定保留待确认。本人已逐项核对三项实质内容并全部保留；记录完成后固定lab1-final，正式报告和提交包从两个不可变标签生成。

## 文件导航

| 文件 | 用途 |
| --- | --- |
| [requirements.md](requirements.md) | 27条最终需求，十字段及来源编号 |
| [chat.txt](chat.txt) | 访谈、AI 交互、人工审查三部分原始记录 |
| [lab1/report.md](lab1/report.md) | 可编辑的报告源文件 |
| [lab1/report.pdf](lab1/report.pdf) | 不含学号姓名的公开报告；个人版及提交ZIP仅保存在private/ |
| [lab1/interview-summary.md](lab1/interview-summary.md) | 两轮书面沟通精简记录，保留七组跟进问答 |
| [lab1/review-candidates.md](lab1/review-candidates.md) | 本人三项实际核对及保留结论 |
| [lab1/materials/](lab1/materials/) | 经检查的甲方书面原材料，不含身份与凭据 |
| [docs/course-requirements.md](docs/course-requirements.md) | 从任务网页与模板核对的完成标准 |
| [docs/project-options.md](docs/project-options.md) | 五个允许选题及各自最低范围 |
| [docs/interview-guide.md](docs/interview-guide.md) | 正式访谈与跟进沟通提纲 |
| [docs/analysis-worksheets.md](docs/analysis-worksheets.md) | 权限、状态、数据、异常与验收工作表 |
| [docs/templates.md](docs/templates.md) | 十字段需求、审查记录与 AI 初稿提示词模板 |
| [docs/workflow.md](docs/workflow.md) | 从访谈、标签到最终打包的顺序 |
| [docs/publication-policy.md](docs/publication-policy.md) | 公开仓库的脱敏与同步规则 |

`private/`、`work/`、`dist/` 和原始下载材料均不纳入 Git。后续实验继续使用本仓库，在相应实验目录增补文档或代码。

## 实验1完成情况

两轮书面材料及七组跟进问答已整理，完整原文和首次AI输出保存在chat.txt；初稿固定为lab1-ai-draft。本人已核对下架原子性、联系方式阶段访问、性能待确认边界三项并保留，记录为REV-001至REV-003。用户确认所用模型为GPT-6.1 Sol。

27条需求中26条已确认，包括22条功能、3条非功能及1条范围约束。REQ-026的具体性能环境仍待甲方确认，不计入已确认数量。本人未要求修改的正文不为凑改动而改写。

完整文本保存并固定lab1-final后，报告写入两个完整提交编号。最终ZIP仅含三文件且两个文本逐字节来自lab1-final；个人身份和提交包不公开。后续性能环境及实现工作不移动或覆盖实验标签。

具体命令见[操作流程](docs/workflow.md)。课程网站的实际提交仍由本人完成。

## 本地工具

需要 Python 3.10+ 和 Git。文档工具依赖在 requirements-tools.txt 中：

```console
python -m pip install -r requirements-tools.txt
git config core.hooksPath .githooks
python scripts/scan_public.py --tracked
python scripts/check_lab1.py
python scripts/build_report.py
```

check_lab1.py检查需求结构、数量、来源及人工审查记录。报告重建后应检查版面。提交和推送钩子会进行敏感内容检查；GitHub Actions同时扫描提交历史。自动检查不能代替本人核对证据。

每次获授权的项目更新完成后，先检查、再提交，并推送到 origin/main。认证使用本机凭据管理器；仓库中不保存密码、令牌、私钥、带认证的 URL 或私有接口配置。提交者使用 GitHub noreply 邮箱。
