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


if __name__ == "__main__":
    unittest.main()
