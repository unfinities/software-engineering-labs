"""Regression: 等待确认 is a real reservation state, not an unresolved placeholder."""
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("check_lab1", Path(__file__).resolve().parents[1] / "scripts/check_lab1.py")
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class ReadinessTests(unittest.TestCase):
    def test_confirmed_reservation_state_is_not_placeholder(self):
        self.assertIsNone(CHECK.UNFINISHED.search("预约处于等待确认状态"))

    def test_unresolved_business_rule_is_placeholder(self):
        self.assertIsNotNone(CHECK.UNFINISHED.search("测试环境待确认"))

    def test_explicit_scope_boundary_is_not_placeholder(self):
        self.assertIsNone(CHECK.UNFINISHED.search("不规定原材料未约定的角色优先级"))

    def test_review_can_quote_a_pending_requirement(self):
        review = "\n".join([
            "- 相关需求：REQ-026",
            "- AI 原始内容：性能条件明确。状态：待确认。",
            "- 发现的问题：环境未固定，保留该状态符合原文。",
            "- 核对依据：INT-02-L014",
            "- 最终处理：保留",
            "- 处理理由：不把未确定环境写成普遍承诺。",
        ])
        self.assertTrue(CHECK.review_is_complete(review))
        self.assertFalse(CHECK.review_is_complete(review.replace("- 最终处理：保留", "- 最终处理：[未填写]")))


if __name__ == "__main__":
    unittest.main()
