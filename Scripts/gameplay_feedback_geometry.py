"""Deterministic watertight rock platforms with a level, generous landing core."""
import math,random

def mushroom(height,radius,seed):
    rng=random.Random(seed);n=11
    angles=[math.tau*(i+rng.uniform(-.19,.19))/n for i in range(n)]
    outline=[rng.uniform(.82,1.12) for _ in range(n)]
    cap=min(150,height*.38);stem=radius*.30
    rings=[(0,stem*1.3),((height-cap)*.27,stem*.94),((height-cap)*.58,stem*1.06),
           (height-cap,stem*.88),(height-cap*.65,radius*.72),(height-35,radius*1.03),
           (height-9,radius*.95),(height,radius*.64)]
    vertices=[];faces=[]
    for j,(z,r) in enumerate(rings):
        shift=(math.sin(j*1.7+seed)*radius*.10,math.cos(j*1.1+seed)*radius*.08) if j<4 else (0,0)
        twist=rng.uniform(-.12,.12) if j<6 else 0
        for i,a in enumerate(angles):
            radial=outline[i]*(1+rng.uniform(-.07,.07)) if j<6 else outline[i]
            # The landing core stays planar; chips live on the cap's outer lip.
            dz=rng.uniform(-8,2) if j in (4,5,6) else 0
            vertices.append((shift[0]+r*radial*math.cos(a+twist),shift[1]+r*radial*math.sin(a+twist),z+dz))
    bottom=len(vertices);vertices.append((0,0,0))
    top=len(vertices);vertices.append((0,0,height))
    for i in range(n):
        k=(i+1)%n;faces.append((bottom,k,i));faces.append((top,7*n+i,7*n+k))
    for j in range(7):
        for i in range(n):
            k=(i+1)%n;a=j*n+i;b=j*n+k;c=(j+1)*n+k;d=(j+1)*n+i
            faces.extend(((a,b,d),(b,c,d)) if (i+j)%2 else ((a,b,c),(a,c,d)))
    edges={}
    for f in faces:
        for a,b in zip(f,(f[1],f[2],f[0])):
            key=tuple(sorted((a,b)));edges[key]=edges.get(key,0)+1
    assert all(v==2 for v in edges.values())
    # GeometryScript uses Unreal's clockwise outward triangle convention.
    return vertices,[(a,c,b) for a,b,c in faces]
