"""Load an existing 16-entry QA save in the 23-entry build, without user saves."""
import argparse,json,subprocess,time,shutil,traceback,struct
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('exe');p.add_argument('old_save_directory');p.add_argument('output');a=p.parse_args()
out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists()
dest=profile/'Saved/SaveGames';dest.mkdir(parents=True)
for f in Path(a.old_save_directory).glob('*.sav'):shutil.copy2(f,dest/f.name)
assert list(dest.glob('*.sav'))
# The existing QA fixture uses UE's tagged 16-int ArrayProperty (68 payload bytes).
# Read its actual persisted inventory; the last live QA response can be newer.
raw=(dest/'BrainMiningProgress_v1.sav').read_bytes();tag=raw.index(b'Collection\0')
assert b'ArrayProperty\0' in raw[tag:tag+40] and b'IntProperty\0' in raw[tag:tag+60]
payload=raw.index(b'\x44\0\0\0\0\x10\0\0\0',tag,tag+80)+9
expected=list(struct.unpack_from('<16i',raw,payload));assert all(v in (0,1) for v in expected)
c=Client(profile);report={'passed':False};proc=None
def cmd(op,**kw):
 r=c.command(op,timeout=180,**kw);assert r['ok'],(op,kw);return r
def wait(pred):
 end=time.monotonic()+180
 while time.monotonic()<end:
  s=cmd('state')
  if pred(s):return s
  time.sleep(.25)
 raise AssertionError(s)
try:
 startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 proc=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-MiningQA','-UserDir='+str(profile)],startupinfo=startup)
 wait(lambda s:s['opening'] and s['menu_buttons']);cmd('continue');s=wait(lambda s:not s['opening'])
 counts=s['collection_counts'];assert len(counts)==23,counts
 assert counts[:16]==expected,(expected,counts)
 assert counts[16:]==[0]*7,counts
 c.key('C');time.sleep(.5);cmd('shot',name='legacy-progress-of23');c.key('C')
 report.update(passed=True,original=expected,counts=counts,collection=s['collection'])
except Exception:report['error']=traceback.format_exc();raise
finally:
 (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 if proc and proc.poll() is None:
  try:cmd('quit');proc.wait(timeout=60)
  except Exception:pass
