"""Scan actual Git blobs without printing matched secrets. Fail closed on binaries."""
import argparse
import io
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DENIED_DIRS = {"private", "originals", "dist", "work", "node_modules", ".venv"}
DENIED_SUFFIXES = {".zip", ".7z", ".rar", ".gz", ".tar", ".htm", ".pem", ".key",
                   ".p12", ".pfx", ".db", ".sqlite", ".sqlite3", ".wav", ".mp3", ".m4a", ".mp4"}
TEXT_SUFFIXES = {".md", ".txt", ".py", ".yml", ".yaml", ".toml", ".json", ".csv",
                 ".js", ".ts", ".tsx", ".jsx", ".css", ".html", ".sh", ".ps1", ".xml"}
RULES = {
    "github-token": re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})\b"),
    "api-secret-token": re.compile(r"\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{20,}\b"),
    "aws-access-key": re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b"),
    "slack-token": re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{20,}\b"),
    "private-key": re.compile(r"-----BEGIN (?:[A-Z0-9 ]+ )?PRIVATE KEY-----"),
    "credential-in-url": re.compile(r"[a-z][a-z0-9+.-]*://[^\s/:@]+:[^\s/@]+@", re.I),
    "jwt": re.compile(r"\beyJ[A-Za-z0-9_-]{12,}\.[A-Za-z0-9_-]{12,}\.[A-Za-z0-9_-]{12,}\b"),
    "auth-cookie": re.compile(r"(?im)^\s*(?:authorization\s*:\s*bearer\s+|cookie\s*:)\S{8,}"),
}
ASSIGNMENT = re.compile(
    r'''(?im)^\s*(?:export\s+)?["']?(?:password|passwd|api[_-]?key|access[_-]?token|client[_-]?secret|secret[_-]?key|密码|口令|令牌)["']?\s*[:=：]\s*["']?([^\s"'#,;]{8,})'''
)
PLACEHOLDER = re.compile(r"(?i)(example|placeholder|changeme|your[_-]|待|填写|脱敏|^\[|^\$|^<|^\{|^none$|^null$)")


def git(*args, input_data=None):
    return subprocess.run(["git", "-c", "core.quotePath=false", *args], cwd=ROOT,
                          input=input_data, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          check=True).stdout


def scan_text(text):
    issues = [name for name, rule in RULES.items() if rule.search(text)]
    if any(not PLACEHOLDER.search(m.group(1)) for m in ASSIGNMENT.finditer(text)):
        issues.append("credential-assignment")
    return issues


def scan_content(name, data):
    path = PurePosixPath(name)
    basename = path.name.lower()
    issues = []
    if (any(part.lower() in DENIED_DIRS for part in path.parts)
            or path.suffix.lower() in DENIED_SUFFIXES
            or (basename.startswith(".env") and basename != ".env.example")
            or basename.startswith(("credentials", "secrets", "id_rsa", "id_ed25519"))):
        issues.append("private-or-raw-file")
    if path.suffix.lower() == ".pdf":
        try:
            from pypdf import PdfReader
            reader = PdfReader(io.BytesIO(data), strict=True)
            if reader.is_encrypted:
                return issues + ["encrypted-pdf"]
            if reader.attachments:
                issues.append("pdf-attachment")
            text = str(reader.metadata or {})
            root = reader.trailer["/Root"]
            if root.get("/Metadata"):
                text += root["/Metadata"].get_data().decode("utf-8", "replace")
            for page in reader.pages:
                text += "\n" + (page.extract_text() or "")
                for ref in page.get("/Annots", []):
                    annotation = ref.get_object()
                    text += "\n" + str(annotation.get("/Contents", ""))
                    if annotation.get("/A"):
                        text += str(annotation["/A"])
            issues += scan_text(text)
        except ImportError:
            issues.append("pdf-parser-missing-install-requirements-tools")
        except Exception:
            issues.append("unreadable-pdf")
        return sorted(set(issues))
    allowed = (path.suffix.lower() in TEXT_SUFFIXES or basename in {".gitignore", ".gitattributes", ".env.example"}
               or (len(path.parts) == 2 and path.parts[0] == ".githooks"))
    if not allowed:
        issues.append("unreviewed-file-type")
    try:
        text = data.decode("utf-8-sig")
        if "\x00" in text:
            issues.append("binary-content")
        issues += scan_text(text)
    except UnicodeDecodeError:
        issues.append("non-utf8-content")
    return sorted(set(issues))


def index_entries():
    for record in git("ls-files", "--stage", "-z").split(b"\0"):
        if not record:
            continue
        fields, path = record.split(b"\t", 1)
        mode, oid, stage = fields.split()
        name = path.decode("utf-8")
        if mode not in {b"100644", b"100755"} or stage != b"0":
            yield name, None, ["nonregular-or-unmerged-file"]
        else:
            yield name, oid.decode(), []


def history_entries():
    records = git("rev-list", "--objects", "--all").decode("utf-8").splitlines()
    if not records:
        return
    oids = [record.split(" ", 1)[0] for record in records]
    kinds = git("cat-file", "--batch-check=%(objecttype)", input_data=("\n".join(oids) + "\n").encode()).decode().splitlines()
    for record, kind in zip(records, kinds):
        if kind == "blob":
            oid, _, name = record.partition(" ")
            yield name, oid, []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--staged", action="store_true", help="Scan the complete index, including staged content")
    mode.add_argument("--tracked", action="store_true", help="Scan current working copies of tracked files")
    mode.add_argument("--history", action="store_true", help="Scan all reachable historical blobs")
    args = parser.parse_args()
    entries = history_entries() if args.history else index_entries()
    count = failures = 0
    for name, oid, issues in entries:
        count += 1
        if oid:
            if args.tracked:
                path = ROOT / name
                if path.is_symlink():
                    issues += ["symlink"]
                elif path.exists():
                    issues += scan_content(name, path.read_bytes())
                else:
                    issues += ["tracked-file-missing"]
            else:
                issues += scan_content(name, git("cat-file", "blob", oid))
        if issues:
            failures += 1
            print(f"BLOCK {name}: {', '.join(issues)}")
    print(f"Public scan: {count} files/blobs checked; {failures} blocked. Matched values are never printed.")
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError:
        sys.exit("Git inspection failed; no credential or raw command output was logged.")
