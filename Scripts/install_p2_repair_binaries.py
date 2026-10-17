from pathlib import Path
import shutil,subprocess,hashlib,json
root=Path(r'E:\涌现taptap2026\BrainMining');build=Path(r'E:\BrainMining_MergeReview_20261016_2330');backup=Path(r'E:\BrainMining_P2Repair_20261017')
assert b'UnrealEditor.exe' not in subprocess.check_output(['tasklist','/FI','IMAGENAME eq UnrealEditor.exe','/NH'])
assert 'Result: Succeeded' in (backup/'build2.log').read_text(errors='replace')
for folder in ['Source','Plugins/CardRoom/Source','Plugins/EchoPassage/Source']:
 for src in (root/folder).rglob('*'):
  if src.is_file():assert src.read_bytes().replace(b'\r\n',b'\n')==(build/src.relative_to(root)).read_bytes().replace(b'\r\n',b'\n'),str(src)
rows=[]
for folder in ['Binaries/Win64','Plugins/CardRoom/Binaries/Win64','Plugins/EchoPassage/Binaries/Win64']:
 for src in (build/folder).iterdir():
  if src.suffix not in ['.dll','.pdb','.modules']:continue
  rel=src.relative_to(build);dst=root/rel;bak=backup/'BinariesBefore'/rel
  assert subprocess.run(['git','ls-files','--error-unmatch','--',rel.as_posix()],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
  if dst.exists():bak.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(dst,bak)
  shutil.copy2(src,dst);rows.append(rel.as_posix())
(backup/'binaries_installed.json').write_text(json.dumps(rows,indent=2))
print('Installed '+str(len(rows))+' generated binary files; original binaries backed up. Git refs/index unchanged.')
