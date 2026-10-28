"""Package the lab2 PDF, PlantUML sources/renders, UI renders and editable UI original locally."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import os
import shutil
from zipfile import ZipFile, ZIP_DEFLATED
from pypdf import PdfReader

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--student-id',required=True)
    parser.add_argument('--name',required=True)
    parser.add_argument('--report',default='private/lab2/实验报告.pdf')
    parser.add_argument('--replace',action='store_true',help='Verify a new ZIP then atomically replace the current one, retaining a backup.')
    args=parser.parse_args()
    for v in [args.student_id,args.name]:
        if not v.strip() or re.search(r'[<>:"/\\|?*\x00-\x1f]',v) or v in {'.','..'}:
            raise SystemExit('Invalid identity field.')
    spec=importlib.util.spec_from_file_location('check_lab2',ROOT/'scripts/check_lab2.py')
    checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
    report=(ROOT/args.report).resolve()
    if not report.is_relative_to(ROOT/'private'):raise SystemExit('Personal report must remain private.')
    pdf=PdfReader(report)
    text=''.join(re.sub(r'\s+','',p.extract_text() or '') for p in pdf.pages)
    for value in [args.student_id,args.name,'软件设计与建模',
                  'https://github.com/unfinities/software-engineering-labs']:
        assert re.sub(r'\s+','',value) in text, 'Missing PDF identity/content field.'
    assert len(pdf.pages)>0 and not pdf.is_encrypted
    assert sum(len(list(page.images)) for page in pdf.pages)>=18, 'Report is missing design figures.'
    dirname=f'实验2_{args.student_id}_{args.name}'
    files={'实验报告.pdf':report.read_bytes()}
    for source in sorted((ROOT/'lab2/UML').glob('*.puml')):
        name='UML'+source.stem.split('_',1)[1]
        files['UML/'+name+'.puml']=source.read_bytes()
        files['UML/'+name+'.png']=source.with_suffix('.png').read_bytes()
    screens=json.loads((ROOT/'lab2/screens.json').read_text('utf-8'))
    for screen in screens:
        files['UI界面/'+screen['id']+'_'+screen['title']+'.png']=(ROOT/'lab2/UI'/(screen['id']+'.png')).read_bytes()
    files['UI界面/校园二手交易系统_UI.drawio']=(ROOT/'lab2/UI/校园二手交易系统_UI.drawio').read_bytes()
    assert len(files)==24
    out=ROOT/'private/submission'/(dirname+'.zip')
    out.parent.mkdir(exist_ok=True)
    if out.exists() and not args.replace:raise SystemExit('Existing package is not overwritten; use --replace to retain a backup and replace it.')
    candidate=out.with_name(out.name+'.replacing')
    with ZipFile(candidate,'x',ZIP_DEFLATED) as archive:
        for name,content in files.items():archive.writestr(dirname+'/'+name,content)
    with ZipFile(candidate) as archive:
        assert archive.testzip() is None and len(archive.namelist())==24
        for name,content in files.items():assert archive.read(dirname+'/'+name)==content
    if out.exists():
        previous_hash=hashlib.sha256(out.read_bytes()).hexdigest()
        backup=ROOT/'private/superseded'/('lab2-package-'+previous_hash[:12])
        backup.mkdir(parents=True,exist_ok=True)
        previous=backup/out.name
        if previous.exists():assert previous.read_bytes()==out.read_bytes()
        else:shutil.copy2(out,previous)
    os.replace(candidate,out)
    manifest={
        'package':out.name,'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),
        'pages':len(pdf.pages),'uml_types':4,'ui_screens':14,'file_count':24,
        'design_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),
        'files':{k:hashlib.sha256(v).hexdigest() for k,v in files.items()},
        'source_docx_sha256':hashlib.sha256((ROOT/'private/lab2/实验报告.docx').read_bytes()).hexdigest(),
        'review':'R2-01..03 retained; actual user report edits documented separately in R2-04; design diagrams unchanged.'}
    (ROOT/'private/lab2/submission-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),'utf-8')
    print('Verified package:',str(out.relative_to(ROOT)),out.stat().st_size,'bytes')

if __name__=='__main__':main()
