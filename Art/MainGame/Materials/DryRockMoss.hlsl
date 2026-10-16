// Matte brown-grey stone and dark, moss-flecked soil. World-space coordinates
// keep the same appearance on original walls and newly excavated faces.
struct TerrainPattern {
    float hash(float3 p) { return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453); }
    float noise(float3 p) {
        float3 i=floor(p),f=frac(p);f=f*f*(3-2*f);
        return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),
                    lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);
    }
    float3 turn(float3 p) {
        // Rotate each octave away from the world grid; no axis-aligned cells.
        return float3(dot(p,float3(.36,.48,-.8)),dot(p,float3(-.8,.6,0)),dot(p,float3(.48,.64,.6)));
    }
    float organic(float3 p) {
        return noise(p)*.57+noise(turn(p)*2.03+float3(7.1,3.4,11.7))*.29
             +noise(turn(p+float3(5.2,9.7,2.1))*4.17)*.14;
    }
    float influence(float3 p,float4 a,float4 b,float reach) {
        float3 axis=b.xyz-a.xyz;float len=max(length(axis),.01);axis/=len;
        float s=dot(p-a.xyz,axis),radial=length(p-a.xyz-axis*s);
        float radius=lerp(a.w,b.w,saturate(s/len));
        return (1-smoothstep(0,reach,max(max(-s,s-len),0)))*(1-smoothstep(radius+40,radius+200,radial));
    }
    float2 chips(float3 p) {
        float3 cell=floor(p),f=frac(p);float best=10,seed=0;
        [unroll] for(int z=-1;z<=1;z++) [unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
            float3 n=float3(x,y,z),i=cell+n;
            float h=hash(i),dither=hash(i+3.1);
            float3 d=(n+float3(dither,hash(i+7.7),hash(i+11.3))-f)*float3(1,.85+h*.35,1.1);
            float shape=max(max(abs(d.x)*.90,abs(d.y)*.86),max(abs(d.z)*.93,(abs(d.x)+abs(d.y)+abs(d.z))*.53));
            if(shape<best){best=shape;seed=h;}
        }
        return float2(best,seed);
    }
};
TerrainPattern dirt;
float coarse=dirt.noise(P*PatternScale*.55);
float grain=dirt.noise(P*DetailScale+float3(3,8,1));
float strata=dirt.noise(P*float3(.009,.009,.026)+float3(2,5,7));
float3 stone=lerp(Tint.rgb,MineralLight.rgb,saturate(.14+coarse*.52+strata*.12));
stone*=.91+.09*grain;
float stoneHeight=BumpDepth*(coarse*.65+grain*.23+strata*.12);
float proximity=0;
// __VOLUME_EVALUATION__
proximity=max(proximity,MudCore);
float2 chip=dirt.chips(P/max(MudCellSize,1));
float radius=lerp(.06,.59,saturate(proximity))*(.38+chip.y*.85);
float mask=(1-smoothstep(radius-.018,radius+.018,chip.x))*smoothstep(.02,.14,proximity);
mask*=smoothstep(.10,.24,chip.y+proximity*.25);
mask=lerp(mask,1,saturate(MudCore));
mask=max(mask,saturate(ClodColor.r-ClodColor.g));
// Soil coloration is continuous across the chip cells and real clod faces.
// The cell pattern above only determines the dirt/rock boundary.
float3 mossP=dirt.turn(P*max(MossScale,.0001));
float3 warp=float3(dirt.noise(mossP*.63+float3(13,5,7)),
                   dirt.noise(mossP*.63+float3(2,19,11)),
                   dirt.noise(mossP*.63+float3(17,3,23)))-.5;
float3 grown=mossP+warp*2.4;
float soilTone=dirt.noise(grown*.73+float3(9,2,6))*.72+grain*.28;
float3 soil=lerp(MudDark.rgb,MudLight.rgb,.22+soilTone*.53)*(.93+.07*grain);
// Warped multi-scale growth gives connected, irregular patches with a fine,
// feathered fringe rather than a clipped green pixel in each noise cell.
float mossField=dirt.organic(grown+float3(13,5,7));
float mossDetail=dirt.noise(dirt.turn(grown)*5.3+float3(1,17,4));
float fibers=dirt.noise(dirt.turn(P*.17)+warp*.7+float3(4,11,3))*.65
            +dirt.noise(dirt.turn(P*.39)+warp*1.4+float3(8,2,19))*.35;
float growth=mossField*.78+mossDetail*.14+fibers*.08;
float threshold=lerp(.78,.35,saturate(MossAmount));
float moss=smoothstep(threshold-.11,threshold+.09,growth);
moss*=lerp(.70,1,smoothstep(.18,.78,mossDetail));
moss*=lerp(.62,1,smoothstep(.25,.72,fibers));
moss*=saturate(MossAmount*8);
float3 mossColor=lerp(MossDark.rgb,MossLight.rgb,.20+mossDetail*.45+fibers*.15);
mossColor*=.78+fibers*.44;
soil=lerp(soil,mossColor,moss*.78);
Height=lerp(stoneHeight,MudDepth*(.30+grain*.50+mossDetail*.10+moss*fibers*.07),mask);
Emission=float3(0,0,0);
SurfaceSpecular=0;
return float4(max(lerp(stone,soil,mask),.001),1);
