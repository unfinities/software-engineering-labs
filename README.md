# 校园二手交易系统 · 实验1提交仓库

提交仓库：https://github.com/unfinities/software-engineering-lab1-submission-2026

本仓库用于用户修改后的实验1提交。原项目仓库及其lab1-ai-draft、lab1-final标签全部保留；这里导入相同初稿标签和完整提交历史，完成修订文本后首次创建本仓库自己的lab1-final，未覆盖原标签。版本关系见[提交历史说明](lab1/submission-history.md)。

## 提交材料

- [requirements.md](requirements.md)：保留用户“最终版”命名，更新当前仓库地址；27条需求正文和编号未改，26条已确认，含功能22条、非功能3条。REQ-026的具体性能环境继续待确认。
- [chat.txt](chat.txt)：保留用户编辑的参与人、日期、工具模型和记录说明，补记实际修订过程；原始材料、首次AI输出和三项人工审查仍在。
- [lab1/report.pdf](lab1/report.pdf)：公开报告，写入本仓库两个完整提交编号；个人版和ZIP在private/，不上传。
- [lab1/report.md](lab1/report.md)：报告可编辑源文件。
- [docs/course-requirements.md](docs/course-requirements.md)：课程完成标准与核对表。
- [docs/analysis-worksheets.md](docs/analysis-worksheets.md)：业务、状态、数据关系及验收分析。

用户本次加入chat.txt的两位参与人姓名按其保留修改的要求保存；学号、密码、令牌、私有接口配置、联系方式及个人提交ZIP不纳入公开仓库。原编辑文本在private/preserved-user-input/有逐字节备份。

## 生成及验证

需要Python 3.10+和Git，安装requirements-tools.txt中的依赖。设置`git config core.hooksPath .githooks`后，提交检查暂存区，推送和GitHub Actions扫描历史。常用命令：

```console
python scripts/check_lab1.py --ready
python scripts/scan_public.py --tracked
python scripts/build_report.py
```

两个标签固定后，使用真实学号姓名在本地生成个人报告和三文件包：

```console
python scripts/build_report.py --final --student-id 实际学号 --name 实际姓名 --output private/report.pdf
python scripts/package_lab1.py --student-id 实际学号 --name 实际姓名 --report private/report.pdf
```

提交ZIP只包含实验1_学号_姓名/下的report.pdf、requirements.md、chat.txt；两个文本逐字节来自本仓库lab1-final。报告写入当前仓库地址和两个完整提交编号，旧仓库不再被当作本次修订包的版本依据。

所有实验标签创建后不得移动、删除或覆盖。仓库至少保留到课程结束；课程网站由本人提交。
