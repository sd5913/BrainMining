"""Shared continuous vertical deformation, in region-2 Blender metres."""
def smooth(a,b,x):
    t=max(0.,min(1.,(x-a)/(b-a)))
    return t*t*(3.-2.*t)

def weights(x):
    return smooth(-49,-38,x)*(1-smooth(-16,-7,x)),smooth(-1,4,x)*(1-smooth(28,37,x))

def point(p):
    x,y,z=p;a,b=weights(x)
    return [x,y,z+.70*(a*(z-76)+b*(z-100))]

def world(p):
    x,y,z=p
    q=point([x/100+77,-y/100,z/100-4])
    return [(q[0]-77)*100,-q[1]*100,(q[2]+4)*100]
