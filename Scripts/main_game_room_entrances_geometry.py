"""Closed faceted cave collars for the linked MainGame rooms (world cm)."""
import math

PROFILE=[(-1,.25),(-.96,.05),(-.60,0),(.45,0),(.96,.05),(1,.25),(.94,.65),(.72,.88),(.35,1),(-.20,1.03),(-.68,.90),(-.93,.65)]

def setup(original,index,rooms):
    s=dict(original);room=rooms[index//2]
    if index==0:s['b']=[room['origin'][0]-990,0,6400];s['plug_x']=room['origin'][0]-1090
    if index==2:s['b']=[-725,0,900];s['plug_x']=-835
    if index==4:s['b']=[46670,11750,4400];s['plug_x']=46560
    if index==1:s['plug_x']=-14180
    if index==5:s['plug_x']=50400
    room_width=[190,200,390,490,390,390][index]
    room_height=[285,290,285,350,350,350][index]
    s.update(natural=True,room_width=room_width,room_height=room_height,backing_depth=110,
             start_x=s['a'][0]-300,end_x=s['b'][0]+25)
    return s

def floor_at(s,x):
    points=s.get('floor_samples',[])
    if not points:return s['a'][2]
    if x<=points[0][0]:return points[0][1]
    for (a,h),(b,k) in zip(points,points[1:]):
        if x<=b:return h+(k-h)*(x-a)/(b-a)
    return points[-1][1]

def cross(s,x):
    t=max(0,min(1,(x-s['start_x'])/(s['end_x']-s['start_x'])))
    # Ease out of the original cave, then meet the room's existing doorway.
    u=t if s['side']=='exit' else 1-t
    v=u*u*(3-2*u);width=390*(1-v)+s['room_width']/2*v
    height=720*(1-v)+s['room_height']*v
    # Pin the final two metres inside the office's original door vestibule.
    if s['id']=='Mine01_Office' and x>s['end_x']-230:
        width=min(width,94);height=min(height,285)
    if s['id']=='Office_Mine02' and x<s['start_x']+300:
        width=min(width,98);height=min(height,290)
    wobble=math.sin(math.pi*t)**2
    cy=24*math.sin(t*5.4)*wobble
    return [(x,s['a'][1]+cy+y*width*(1+.045*math.sin(j*2.1+t*8)*wobble),floor_at(s,x)+z*height*(1+.04*math.sin(j*1.7+t*7)*wobble)) for j,(y,z) in enumerate(PROFILE)]

def closed_tube(s,hard=False):
    vertices=[];triangles=[];n=len(PROFILE)
    xs=sorted(set([s['start_x']+(s['end_x']-s['start_x'])*i/16 for i in range(17)]+[p[0] for p in s.get('floor_samples',[]) if s['start_x']<=p[0]<=s['end_x']]))
    rings=len(xs)
    for x in xs:
        inner=cross(s,x);floor=floor_at(s,x)
        ceiling=max(p[2] for p in inner)-floor
        expanded=[(p[0],s['a'][1]+(p[1]-s['a'][1])*1.44,p[2]+220*(p[2]-floor)/ceiling-110) for p in inner]
        if hard:
            inner=expanded
            outer=[(x,s['a'][1]+y*1000,floor-340+z*1550) for y,z in PROFILE]
        else:
            outer=expanded
        vertices.extend(inner+outer)
    for i in range(rings-1):
        for j in range(n):
            k=(j+1)%n;a=i*2*n+j;b=(i+1)*2*n+j;c=(i+1)*2*n+k;d=i*2*n+k
            triangles.extend([(a,b,c),(a,c,d)])
            a+=n;b+=n;c+=n;d+=n;triangles.extend([(a,c,b),(a,d,c)])
    for i,reverse in [(0,False),(rings-1,True)]:
        for j in range(n):
            k=(j+1)%n;a=i*2*n+j;b=i*2*n+k;c=b+n;d=a+n
            for tri in [(a,b,c),(a,c,d)]:triangles.append(tuple(reversed(tri)) if reverse else tri)
    return vertices,triangles

def soil(s):
    points=cross(s,s['plug_x']);n=len(points);vertices=[];triangles=[];floor=s['a'][2]
    for side in [-1,1]:
        for j,p in enumerate(points):
            vertices.append((s['plug_x']+side*(80+12*math.sin(j*2.3)),s['a'][1]+(p[1]-s['a'][1])*1.09,floor-12+(p[2]-floor)*1.07))
    vertices.extend([(s['plug_x']-116,s['a'][1]-9,floor+s['room_height']*.49),(s['plug_x']+105,s['a'][1]+14,floor+s['room_height']*.50)])
    for j in range(n):
        k=(j+1)%n;triangles.extend([(j,k,n+k),(j,n+k,n+j),(2*n,k,j),(2*n+1,n+j,n+k)])
    return vertices,triangles

def assert_closed(vertices,triangles):
    from collections import Counter
    edges=Counter(tuple(sorted((a,b))) for t in triangles for a,b in zip(t,(t[1],t[2],t[0])))
    assert all(v==2 for v in edges.values()),'Open generated solid'
    # Surface orientation matters for collision and terrain booleans.
    volume=sum(vertices[a][0]*(vertices[b][1]*vertices[c][2]-vertices[b][2]*vertices[c][1])+vertices[a][1]*(vertices[b][2]*vertices[c][0]-vertices[b][0]*vertices[c][2])+vertices[a][2]*(vertices[b][0]*vertices[c][1]-vertices[b][1]*vertices[c][0]) for a,b,c in triangles)/6
    # UE's terrain generators use clockwise outward faces. Match their signed
    # volume, otherwise the inward cave surface is culled and booleans invert.
    if volume>0:triangles=[(a,c,b) for a,b,c in triangles]
    return vertices,triangles

def write_obj(path,vertices,triangles):
    vertices,triangles=assert_closed(vertices,triangles)
    lines=['v %.9f %.9f %.9f'%(x/100,-y/100,z/100) for x,y,z in vertices]
    lines+=['f %d %d %d'%(a+1,c+1,b+1) for a,b,c in triangles]
    path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
