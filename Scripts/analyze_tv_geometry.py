import json,collections
from pathlib import Path
j=json.loads(Path('Saved/Validation/P2Repair/tv_geometry.json').read_text());v=j['v'];t=j['t']
adj=collections.defaultdict(set)
# Welded-position connectivity catches UV seams.
canon={}; idx=[]
for p in v:
 k=tuple(round(c,3) for c in p);idx.append(canon.setdefault(k,len(canon)))
for a,b,c in t:
 a,b,c=idx[a],idx[b],idx[c];adj[a].update((b,c));adj[b].update((a,c));adj[c].update((a,b))
seen=set();groups=[]
for i in adj:
 if i in seen:continue
 q=[i];seen.add(i);g=set(q)
 while q:
  for n in adj[q.pop()]:
   if n not in seen:seen.add(n);g.add(n);q.append(n)
 vv=[p for k,p in enumerate(v) if idx[k] in g]
 tt=[ti for ti,tr in enumerate(t) if idx[tr[0]] in g]
 groups.append({'vertices':len(vv),'triangles':len(tt),'min':[min(p[d] for p in vv) for d in range(3)],'max':[max(p[d] for p in vv) for d in range(3)],'ids':tt})
Path('Saved/Validation/P2Repair/tv_groups.json').write_text(json.dumps(groups))
for i,g in enumerate(groups):print(i,{k:x for k,x in g.items() if k!='ids'})
