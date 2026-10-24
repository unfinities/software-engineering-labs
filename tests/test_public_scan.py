import importlib.util
import io
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("scan_public", Path(__file__).resolve().parents[1] / "scripts/scan_public.py")
SCAN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCAN)


class PublicScanTests(unittest.TestCase):
    def test_public_content(self):
        self.assertEqual(SCAN.scan_content("requirements.md", "没有凭据的需求框架".encode()), [])

    def test_github_credential(self):
        candidate = "gh" + "p_" + "A" * 36
        self.assertIn("github-token", SCAN.scan_text(candidate))

    def test_api_credential(self):
        candidate = "sk-" + "proj-" + "a" * 48
        self.assertIn("api-secret-token", SCAN.scan_text(candidate))

    def test_password_assignment(self):
        candidate = "pass" + 'word = "' + "a" * 16 + '"'
        self.assertIn("credential-assignment", SCAN.scan_text(candidate))

    def test_placeholder_allowed(self):
        self.assertEqual(SCAN.scan_text("api_" + 'key = "YOUR_VALUE_HERE"'), [])

    def test_url_credential(self):
        candidate = "https://" + "account:" + "value@" + "example.test"
        self.assertIn("credential-in-url", SCAN.scan_text(candidate))

    def test_private_input_blocked(self):
        self.assertIn("private-or-raw-file", SCAN.scan_content("private/interview.txt", b"private"))
        self.assertIn("private-or-raw-file", SCAN.scan_content("detail.htm", b"page"))

    def test_binary_and_broken_pdf(self):
        self.assertIn("unreviewed-file-type", SCAN.scan_content("data.bin", b"\x00"))
        self.assertIn("unreadable-pdf", SCAN.scan_content("report.pdf", b"invalid"))

    def test_pdf_metadata_secret(self):
        from pypdf import PdfWriter
        writer = PdfWriter()
        writer.add_blank_page(width=200, height=200)
        writer.add_metadata({"/Subject": "gh" + "p_" + "B" * 36})
        output = io.BytesIO()
        writer.write(output)
        self.assertIn("github-token", SCAN.scan_content("report.pdf", output.getvalue()))

    def test_pdf_attachment_blocked(self):
        from pypdf import PdfWriter
        writer = PdfWriter()
        writer.add_blank_page(width=200, height=200)
        writer.add_attachment("raw.txt", b"raw input")
        output = io.BytesIO()
        writer.write(output)
        self.assertIn("pdf-attachment", SCAN.scan_content("report.pdf", output.getvalue()))


if __name__ == "__main__":
    unittest.main()
