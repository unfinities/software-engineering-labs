"""Produce exactly three submission files from the immutable final text tag."""
import argparse
from pathlib import Path
import re
import subprocess
import sys
from zipfile import ZipFile, ZIP_DEFLATED

from pypdf import PdfReader
from build_report import tag_commit, PLACEHOLDERS, REPOSITORY
from check_lab1 import check

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, check=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--student-id", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--report", default="private/report.pdf")
    args = parser.parse_args()
    for value in [args.student_id, args.name]:
        if not value.strip() or re.search(r'[<>:"/\\|?*\x00-\x1f]', value) or value in {".", ".."}:
            sys.exit("学号姓名不能包含路径或非法文件名字符。")
    draft, final = tag_commit("lab1-ai-draft"), tag_commit("lab1-final")
    if not draft or not final:
        sys.exit("尚无两个实际实验标签，不能生成正式提交包。")
    ancestry = subprocess.run(["git", "merge-base", "--is-ancestor", draft, final], cwd=ROOT, capture_output=True)
    if draft == final or ancestry.returncode:
        sys.exit("最终标签必须在初稿标签之后，且不能是同一个提交。")
    errors = check()
    if errors:
        print("\n".join(errors))
        sys.exit("需求或记录未达结构/数量要求，未打包。")
    payloads = {}
    for name in ["requirements.md", "chat.txt"]:
        frozen = git("show", f"lab1-final:{name}")
        if (ROOT / name).read_bytes() != frozen:
            sys.exit(name + " 当前字节与 lab1-final 不同，未打包。")
        payloads[name] = frozen
    report = (ROOT / args.report).resolve()
    if not report.is_file():
        sys.exit("请先完成并视觉检查正式 report.pdf。")
    reader = PdfReader(report)
    text = "\n".join(page.extract_text() or "" for page in reader.pages)
    if PLACEHOLDERS.search(text):
        sys.exit("PDF 仍有准备阶段占位文本，未打包。")
    for expected in [draft, final, REPOSITORY, args.student_id, args.name]:
        if expected not in text.replace("\n", ""):
            sys.exit("PDF 缺少正确的版本或身份信息，未打包。")
    payloads["report.pdf"] = report.read_bytes()
    dirname = f"实验1_{args.student_id}_{args.name}"
    directory = ROOT / "private" / "submission"
    directory.mkdir(parents=True, exist_ok=True)
    archive = directory / (dirname + ".zip")
    if archive.exists():
        sys.exit("目标提交包已存在；请先自行确认旧包，不自动覆盖。")
    with ZipFile(archive, "x", ZIP_DEFLATED) as zipped:
        for name in ["report.pdf", "requirements.md", "chat.txt"]:
            zipped.writestr(dirname + "/" + name, payloads[name])
    with ZipFile(archive) as zipped:
        assert len(zipped.namelist()) == 3
        for name, data in payloads.items():
            assert zipped.read(dirname + "/" + name) == data
    print("已生成并逐字节核验三文件提交包：" + str(archive.relative_to(ROOT)))
    print("公开仓库不会包含此包。提交前请本人确认访谈和审查真实性、PDF 版面及截止时间。")


if __name__ == "__main__":
    main()
