"""Build the editable Markdown report as a CJK PDF; final mode requires fixed tags."""
import argparse
from html import escape
from pathlib import Path
import re
import subprocess
import sys

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.cidfonts import UnicodeCIDFont
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, CondPageBreak

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = "https://github.com/unfinities/software-engineering-lab1-submission-2026"
PLACEHOLDERS = re.compile(r"\[(?:待|实际|仅在)[^\]]*\]|\{\{[^}]+\}\}|当前阶段：访谈前框架|当前两个标签尚未创建")


def tag_commit(tag):
    result = subprocess.run(["git", "rev-parse", "--verify", tag + "^{commit}"], cwd=ROOT, capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


def font_name():
    for path in [Path("C:/Windows/Fonts/msyh.ttc"), Path("C:/Windows/Fonts/simsun.ttc")]:
        if path.exists():
            pdfmetrics.registerFont(TTFont("ReportCJK", str(path), subfontIndex=0))
            return "ReportCJK"
    pdfmetrics.registerFont(UnicodeCIDFont("STSong-Light"))
    return "STSong-Light"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--final", action="store_true")
    parser.add_argument("--student-id")
    parser.add_argument("--name")
    parser.add_argument("--output", default="lab1/report.pdf")
    args = parser.parse_args()
    draft, final = tag_commit("lab1-ai-draft"), tag_commit("lab1-final")
    if args.final and (not draft or not final or not args.student_id or not args.name):
        sys.exit("正式报告需要两个已固定标签及真实学号姓名；当前不生成完成版。")
    text = (ROOT / "lab1/report.md").read_text(encoding="utf-8")
    substitutions = {
        "{{REPOSITORY_URL}}": REPOSITORY,
        "{{AI_DRAFT_COMMIT}}": draft or "[待创建；当前未开展访谈]",
        "{{FINAL_COMMIT}}": final or "[待创建；当前未完成审查]",
        "{{STUDENT_ID}}": args.student_id or "[仅在本地提交版填写]",
        "{{STUDENT_NAME}}": args.name or "[仅在本地提交版填写]",
    }
    for key, value in substitutions.items():
        text = text.replace(key, value)
    if args.final and PLACEHOLDERS.search(text):
        sys.exit("报告仍包含框架占位内容，请先按真实分析补全 lab1/report.md。")
    output = (ROOT / args.output).resolve()
    if (args.student_id or args.name) and not output.is_relative_to(ROOT / "private"):
        sys.exit("含学号姓名的报告只能输出到被忽略的 private/。")
    output.parent.mkdir(parents=True, exist_ok=True)
    font = font_name()
    styles = getSampleStyleSheet()
    for style in styles.byName.values():
        style.fontName = font
    body = ParagraphStyle("CJKBody", fontName=font, fontSize=10.2, leading=16.5, spaceAfter=7, wordWrap="CJK")
    heading = ParagraphStyle("CJKHeading", parent=body, fontSize=13, leading=20, spaceBefore=12, spaceAfter=7, keepWithNext=True)
    subheading = ParagraphStyle("CJKSubheading", parent=body, fontSize=11.5, leading=18, spaceBefore=8, keepWithNext=True)
    title = ParagraphStyle("CJKTitle", parent=body, fontSize=20, leading=30, alignment=TA_CENTER, spaceAfter=18)
    cell = ParagraphStyle("CJKCell", parent=body, fontSize=9, leading=14, spaceAfter=0)
    doc = SimpleDocTemplate(str(output), pagesize=A4, rightMargin=48, leftMargin=48,
                            topMargin=42, bottomMargin=42, title="需求分析报告", author="", subject="软件工程实验 1")
    story, lines, index = [], text.splitlines(), 0
    width = A4[0] - 96
    while index < len(lines):
        line = lines[index].strip()
        if not line:
            index += 1
            continue
        if line.startswith("|"):
            rows = []
            while index < len(lines) and lines[index].strip().startswith("|"):
                values = [value.strip() for value in lines[index].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-+:?", value) for value in values):
                    rows.append([Paragraph(escape(value), cell) for value in values])
                index += 1
            weights = {
                "阶段": [.85, 2.2, 1.95], "对象": [.85, 2.1, 2.05],
                "商品状态": [.8, 2.4, 1.8], "预约阶段": [1.4, 1.8, 1.8],
                "类别及需求": [.95, 3.3, .75], "场景": [1.25, 3.75],
            }.get(rows[0][0].getPlainText(), [1] * len(rows[0]))
            if len(weights) != len(rows[0]):
                weights = [1] * len(rows[0])
            table = Table(rows, colWidths=[width * value / sum(weights) for value in weights], repeatRows=1, hAlign="LEFT")
            table.setStyle(TableStyle([
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#eef2f5")),
                ("GRID", (0, 0), (-1, -1), .4, colors.HexColor("#bcc5cf")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("TOPPADDING", (0, 0), (-1, -1), 6),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
            ]))
            story.extend([table, Spacer(1, 8)])
            continue
        if line.startswith("## 3 "):
            story.append(CondPageBreak(200))
        if line.startswith("# "):
            style, value = title, line[2:]
        elif line.startswith("## "):
            style, value = heading, line[3:]
        elif line.startswith("### "):
            style, value = subheading, line[4:]
        else:
            style, value = body, line
        story.append(Paragraph(escape(value), style))
        index += 1

    def footer(canvas, document):
        canvas.saveState()
        canvas.setFont(font, 9)
        canvas.setFillColor(colors.HexColor("#667080"))
        canvas.drawCentredString(A4[0] / 2, 24, str(document.page))
        canvas.restoreState()

    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print("已生成：" + str(output.relative_to(ROOT)))
    print("请打开 PDF 核对中文、表格、分页；自动生成不代替视觉检查。")


if __name__ == "__main__":
    main()
