from pathlib import Path
import shutil,json
root=Path(r'E:\涌现taptap2026\BrainMining')
build=Path(r'E:\BrainMining_MergeReview_20261016_2330')
backup=Path(r'E:\BrainMining_P2Repair_20261017\BuildSourcesBefore')
changed=[]
for folder in ['Source','Plugins/CardRoom/Source','Plugins/EchoPassage/Source']:
 for src in (root/folder).rglob('*'):
  if not src.is_file():continue
  rel=src.relative_to(root);dst=build/rel
  if dst.exists() and src.read_bytes().replace(b'\r\n',b'\n')==dst.read_bytes().replace(b'\r\n',b'\n'):continue
  if dst.exists():
   bak=backup/rel;bak.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(dst,bak)
  dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst);changed.append(str(rel))
print(json.dumps(changed))
