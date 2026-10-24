"""Structural readiness checks; human review must verify the actual evidence."""
import argparse
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FIELDS = ["类型", "来源", "优先级", "状态", "需求描述", "前置条件", "正常结果", "异常或边界情况", "验收方法", "关联需求"]
UNFINISHED = re.compile(r"待确认|待填写|待访谈|\[|未约定|暂无|待补充")


def check():
    requirements = (ROOT / "requirements.md").read_text(encoding="utf-8")
    chat = (ROOT / "chat.txt").read_text(encoding="utf-8")
    blocks = list(re.finditer(r"(?m)^## (REQ-\d{3,})\s+(.+)$", requirements))
    errors, confirmed = [], []
    ids = [match.group(1) for match in blocks]
    if len(ids) != len(set(ids)):
        errors.append("需求编号重复")
    for index, match in enumerate(blocks):
        rid = match.group(1)
        body = requirements[match.end():blocks[index + 1].start() if index + 1 < len(blocks) else len(requirements)]
        pairs = re.findall(r"(?m)^- ([^：\n]+)：(.*)$", body)
        values = dict(pairs)
        if [key for key, _ in pairs] != FIELDS:
            errors.append(f"{rid} 十字段缺失、多出或顺序错误")
        if values.get("类型") not in {"功能需求", "非功能需求", "约束"}:
            errors.append(f"{rid} 类型无效")
        if values.get("优先级") not in {"高", "中", "低"}:
            errors.append(f"{rid} 优先级无效")
        if values.get("状态") not in {"已确认", "待确认", "已拒绝", "已变更"}:
            errors.append(f"{rid} 状态无效")
        if values.get("状态") != "已确认":
            continue
        confirmed.append(values)
        if any(not value.strip() for _, value in pairs):
            errors.append(f"{rid} 已确认条目存在空字段")
        if any(UNFINISHED.search(values.get(key, "")) for key in FIELDS if key != "关联需求"):
            errors.append(f"{rid} 已确认条目仍有占位或待确认文本")
        sources = re.findall(r"INT-\d{2}-L\d{3,}|COURSE-\d{3}", values.get("来源", ""))
        if not sources:
            errors.append(f"{rid} 缺少可追踪来源")
        for source in sources:
            if source.startswith("INT-") and not re.search(r"(?m)^" + re.escape(source) + r"\s+\S", chat):
                errors.append(f"{rid} 来源 {source} 没有实际对话行")
            if source.startswith("COURSE-") and source not in (ROOT / "docs/course-requirements.md").read_text(encoding="utf-8"):
                errors.append(f"{rid} 来源 {source} 不存在")
        related = re.findall(r"REQ-\d{3,}", values.get("关联需求", ""))
        if any(other not in ids or other == rid for other in related):
            errors.append(f"{rid} 关联需求不存在或关联自身")
    functional = sum(item.get("类型") == "功能需求" for item in confirmed)
    nonfunctional = sum(item.get("类型") == "非功能需求" for item in confirmed)
    exceptions = sum(bool(item.get("异常或边界情况", "").strip())
                     and item.get("异常或边界情况", "").strip() not in {"无", "不适用"}
                     and not UNFINISHED.search(item.get("异常或边界情况", "")) for item in confirmed)
    for actual, minimum, label in [(len(confirmed), 15, "已确认需求"), (functional, 10, "功能需求"),
                                    (nonfunctional, 3, "非功能需求"), (exceptions, 5, "异常/边界处理需求")]:
        if actual < minimum:
            errors.append(f"{label} {actual}/{minimum}")
    headings = ["一、访谈与补充沟通", "二、AI 交互记录", "三、人工审查记录"]
    positions = [chat.find(heading) for heading in headings]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        errors.append("chat.txt 三部分缺失或顺序错误")
    for interview in ["01", "02"]:
        lines = re.findall(r"(?m)^INT-" + interview + r"-L\d{3,}\s+(.+)$", chat)
        if len(lines) < 2:
            errors.append(f"INT-{interview} 尚无至少一问一答的实际记录")
    if "## REQ-001 " not in chat:
        errors.append("chat.txt 尚未保存首次完整 AI 需求输出")
    review = chat.split("三、人工审查记录", 1)[-1]
    reviews = re.findall(r"(?ms)^## REV-\d{3,}\s*\n(.*?)(?=^## REV-|\Z)", review)
    complete = [part for part in reviews if all(re.search(r"(?m)^- " + field + r"：\S", part)
                for field in ["相关需求", "AI 原始内容", "发现的问题", "核对依据", "最终处理", "处理理由"])
                and not re.search(r"(?m)^- [^\n]+：[\[待]", part)]
    if len(complete) < 3:
        errors.append(f"实际人工审查记录 {len(complete)}/3")
    print(f"需求总数 {len(blocks)}；已确认 {len(confirmed)}；功能 {functional}；非功能 {nonfunctional}；异常/边界 {exceptions}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ready", action="store_true", help="Require readiness (also used by packaging)")
    parser.parse_args()
    errors = check()
    for error in errors:
        print("待完成：" + error)
    print("本工具仅检查结构和部分数量，不能核实访谈真实性、确认事实、非功能指标质量或人工审查实质。")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
