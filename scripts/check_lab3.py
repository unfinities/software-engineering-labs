"""Check handwritten C++ scale and optionally run the documented style checks."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CODE = ROOT / 'lab3/code'
FILTER = '-build/include_subdir,-build/c++11,-build/c++17,-build/c++20'


def metrics():
    records = []
    for path in sorted(CODE.rglob('*')):
        if path.suffix not in {'.cpp', '.h'} or 'vendor' in path.parts or 'build' in path.parts:
            continue
        raw = path.read_text('utf-8')
        text = re.sub(r'/\*.*?\*/', '', raw, flags=re.S)
        text = re.sub(r'//[^\n]*', '', text)
        meaningful = sum(bool(line.strip()) and line.strip() not in {'{', '}', '};'}
                         for line in text.splitlines())
        records.append({'file': path.relative_to(CODE).as_posix(),
                        'physical': len(raw.splitlines()), 'noncomment_nonbrace': meaningful,
                        'test': 'tests' in path.parts})
    production = [row for row in records if not row['test']]
    return {'method': 'UTF-8 C++ files; tests and third-party excluded from production; '
                      'conservative nonblank noncomment nonbrace count',
            'production_physical': sum(row['physical'] for row in production),
            'production_noncomment_nonbrace': sum(row['noncomment_nonbrace'] for row in production),
            'files': records}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lint', action='store_true')
    parser.add_argument('--write-metrics', action='store_true')
    args = parser.parse_args()
    data = metrics()
    assert data['production_noncomment_nonbrace'] > 500, 'Insufficient production C++ source.'
    assert len(list((ROOT/'lab3/UML').glob('*.puml'))) == 4
    for row in data['files']:
        assert '\x00' not in (CODE/row['file']).read_text('utf-8')
    if args.write_metrics:
        (ROOT/'lab3/metrics.json').write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n', 'utf-8')
    print('Handwritten production C++:', data['production_physical'], 'physical lines;',
          data['production_noncomment_nonbrace'], 'noncomment/nonbrace lines; tests and SQLite excluded.')
    if args.lint:
        files = [str(CODE/row['file']) for row in data['files']]
        subprocess.run([sys.executable, '-m', 'cpplint', '--linelength=100', '--filter='+FILTER,
                        *files], check=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
