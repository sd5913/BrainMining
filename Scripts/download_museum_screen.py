"""Fetch the CC0 Poly Haven screen and verify its published file hashes."""
import json,pathlib,hashlib,subprocess,concurrent.futures
root=pathlib.Path('SourceAssets/MuseumRefresh/chinese_screen_panels');root.mkdir(parents=True,exist_ok=True)
def fetch(url,path):
 if not path.exists():subprocess.run(['curl.exe','-fL','--retry','2','-A','Mozilla/5.0',url,'-o',str(path)],check=True,capture_output=True)
 return path
meta=json.loads(fetch('https://api.polyhaven.com/files/chinese_screen_panels',root/'files.json').read_text())
jobs=[]
for kind,key,ext in [('model','fbx','fbx'),('base','Diffuse','jpg'),('normal','nor_dx','png'),('orm','arm','jpg')]:
 node=meta[key]['2k'][ext];jobs.append((kind,node))
report=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
 futures=[(kind,node,ex.submit(fetch,node['url'],root/node['url'].split('/')[-1])) for kind,node in jobs]
 for kind,node,f in futures:
  p=f.result();assert hashlib.md5(p.read_bytes()).hexdigest()==node['md5'];report.append(dict(kind=kind,path=str(p),url=node['url'],md5=node['md5'],license='CC0-1.0',author='Kirill Sannikov'))
(root/'downloads.json').write_text(json.dumps(report,indent=2));print('SCREEN_DOWNLOADED',[(r['kind'],pathlib.Path(r['path']).stat().st_size) for r in report])
