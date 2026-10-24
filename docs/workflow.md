# 实验流程与 Git 操作

本次更新按用户决定建立独立提交仓库。原仓库及其标签全部保留，当前操作仅在新提交仓库进行。新lab1-final是在本仓库首次创建，绝不覆盖原仓库的同名标签。

## 0. 当前阶段

校园二手交易系统的两轮书面材料与首次完整AI初稿已整理。初稿已保存到lab1-ai-draft，本人已明确逐项核对三项并全部保留；实际意见已写入chat.txt，完整文本提交后固定lab1-final。REQ-026的具体性能环境尚待确认。

## 1. 真实访谈

INT-01 正式访谈和 INT-02 跟进沟通各至少一轮，均记录真实时间、完整对话、行号。保留敏感原件在 private/，发送 AI 前先脱敏。实际业务答案写入分析工作表，不将提纲充当证据。

## 2. 第一次 AI 完整需求

使用真实材料和 docs/templates.md 的提示词，保存完整输入输出至 chat.txt，原样写入 requirements.md。此时尚未进行人工修改。先查看差异并扫描，再执行：

```console
git add requirements.md chat.txt
python scripts/scan_public.py --staged
git diff --cached
git commit -m "docs(lab1): preserve first AI requirements draft"
git tag -a lab1-ai-draft -m "Lab 1 first complete AI requirements draft"
git push origin main
git push origin lab1-ai-draft
```

如果标签已存在，停止创建，不覆盖。保持首次 AI 输出的完整证据，不能将框架提交标成初稿。

## 3. 本人审查与最终文本

本人对照原始材料逐条核对。追加至少三项实际 REV 核对，以及真实后续沟通和 AI 交互。稳定需求编号，检查至少 15 条已确认、10 条功能、3 条非功能、5 条异常/边界处理。自动工具只能核对结构和部分数量，不能判断甲方是否真的确认。

```console
python scripts/check_lab1.py --ready
git add requirements.md chat.txt
python scripts/scan_public.py --staged
git diff --cached
git commit -m "docs(lab1): finalize reviewed requirements and evidence"
git tag -a lab1-final -m "Lab 1 reviewed final requirements and complete records"
git push origin main
git push origin lab1-final
```

标签不得移动、删除或覆盖。最终标签必须在初稿标签之后。不要为了把最终 SHA 写进 chat.txt 再改动冻结文本；SHA 放在之后完成的报告中。

## 4. 报告与提交包

两标签固定后，补全 lab1/report.md 的实质内容。模板保留原报告的项目概况、整体业务分析、实验总结、版本信息。报告生成器自动读取两个完整 SHA；学号姓名仅填入本地提交版，命令参数请替换为真实值：

```console
python scripts/build_report.py --final --student-id 实际学号 --name 实际姓名 --output private/report.pdf
python scripts/package_lab1.py --student-id 实际学号 --name 实际姓名 --report private/report.pdf
```

先人工打开生成的 PDF 查看中文、表格、分页，再使用生成的 private/submission/实验1_学号_姓名.zip。打包工具会阻止不完整材料、校验标签顺序、对比两个文本的字节内容，并从 lab1-final 取出文本，保证包内一致。ZIP 内仅有三份文件，学号姓名不因打包进入 Git 历史。

报告可以在 lab1-final 之后提交更新；已冻结的两个文本若需要实质更正，先向课程方确认处理方式，不移动标签。

## 5. 每次后续同步

```console
git status
git add <明确要上传的文件>
python scripts/scan_public.py --staged
git diff --cached
git commit -m "描述实际更改"
git push origin main
```

保持钩子：`git config core.hooksPath .githooks`。初始化本机可用 `git config lab.python <Python可执行文件绝对路径>` 指定钩子解释器；此路径只写入本地 .git/config。

不要使用盲目定时上传或推送后再检查的流程。用户已授权项目后续更新检查后继续同步到同一 GitHub 仓库；每次实际操作仍需验证远端和差异。
