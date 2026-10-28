"""Check the lab2 artifact structure and requirement-to-module coverage."""
from pathlib import Path
import json
import hashlib
import re
from xml.etree import ElementTree as ET
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]/'lab2'
sources=sorted((ROOT/'UML').glob('*.puml'))
assert len(sources)==4
for file in sources:
    text=file.read_text('utf-8')
    assert text.count('@startuml')==1 and text.count('@enduml')==1
    assert 'title ' in text
    assert file.with_suffix('.png').is_file()
screens=json.loads((ROOT/'screens.json').read_text('utf-8'))
assert [s['id'] for s in screens]==[f'UI{i:02}' for i in range(1,15)]
diagram=ET.parse(ROOT/'UI/校园二手交易系统_UI.drawio').getroot()
assert len(diagram.findall('diagram'))==14
assert all(d.find('mxGraphModel') is not None for d in diagram.findall('diagram'))
assert not diagram.findall('.//mxCell[@style="image"]')
for image in list((ROOT/'UML').glob('*.png'))+list((ROOT/'UI').glob('*.png')):
    with Image.open(image) as im:
        assert im.format=='PNG' and not im.info
        im.verify()
modules=json.loads((ROOT/'modules.json').read_text('utf-8'))
assert len(modules)==7
covered=set()
for module in modules:
    for item in module[2].split('、'):
        nums=[int(n) for n in re.findall(r'\d+',item)]
        covered.update(range(nums[0],nums[-1]+1))
assert set(range(1,23))<=covered
manifest=json.loads((ROOT/'delivery-manifest.json').read_text('utf-8'))
for filename,expected in manifest['files'].items():
    b=(ROOT/filename).read_bytes()
    if Path(filename).suffix in {'.puml','.drawio'}:b=b.replace(b'\r\n',b'\n')
    assert hashlib.sha256(b).hexdigest()==expected,filename
print('Lab2: four UML types, fourteen editable UI pages, seven modules covering all 22 functional requirements; artifact hashes verified.')
