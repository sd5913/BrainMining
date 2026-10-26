import urllib.request,json,pathlib,hashlib,concurrent.futures,subprocess
root=pathlib.Path('SourceAssets/MuseumRefresh');manifest=[]
def fetch(url,p):
 p=root/p;p.parent.mkdir(parents=True,exist_ok=True)
 if not p.exists():
  subprocess.run(['curl.exe','-fL','--retry','2','-A','Mozilla/5.0',url,'-o',str(p)],check=True,capture_output=True)
 return p
for name,num in [('screen','36207'),('balance','37604')]:
 p=fetch(f'https://cdn.3dassets.dev/assets/{num}/v1/model.glb',f'{name}/{name}.glb')
 manifest.append({'asset':name,'url':f'https://cdn.3dassets.dev/assets/{num}/v1/model.glb','path':str(p),'license':'CC0-1.0','sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
name='bronze_shark_statue'
meta=json.loads(fetch('https://api.polyhaven.com/files/'+name,name+'/files.json').read_text());(root/name).mkdir(exist_ok=True)
(root/name/'files.json').write_text(json.dumps(meta,indent=2))
jobs=[]
for kind,node in [('model',meta['fbx']['2k']['fbx']),('diffuse',meta['Diffuse']['2k']['jpg']),('normal',meta['nor_dx']['2k']['png']),('arm',meta['arm']['2k']['jpg'])]:
 jobs.append((kind,node,pathlib.Path(name)/node['url'].split('/')[-1]))
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
 futs=[(kind,node,ex.submit(fetch,node['url'],p)) for kind,node,p in jobs]
 for kind,node,f in futs:
  p=f.result();assert hashlib.md5(p.read_bytes()).hexdigest()==node['md5']
  manifest.append({'asset':name,'kind':kind,'url':node['url'],'path':str(p),'license':'CC0-1.0','md5':node['md5']})
(root/'downloads_followup.json').write_text(json.dumps(manifest,indent=2))
print('FOLLOWUP_DOWNLOADED',[(r['asset'],r.get('kind'),pathlib.Path(r['path']).stat().st_size) for r in manifest])

