// Localized world-space clay chips. Evaluated on the terrain itself, including
// freshly excavated faces; no floating decoration survives terrain removal.
struct DirtPattern {
    float hash(float3 p) { p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z); }
    float influence(float3 p,float4 a,float4 b,float reach) {
        float3 axis=b.xyz-a.xyz;float len=max(length(axis),.01);axis/=len;
        float s=dot(p-a.xyz,axis);
        float radial=length(p-a.xyz-axis*s);
        float radius=lerp(a.w,b.w,saturate(s/len));
        float outside=max(max(-s,s-len),0);
        return (1-smoothstep(0,reach,outside))*(1-smoothstep(radius+40,radius+200,radial));
    }
    float4 chip(float3 p) {
        float3 cell=floor(p),f=frac(p);float best=10,seed=0;
        [unroll] for(int z=-1;z<=1;z++) [unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
            float3 n=float3(x,y,z),i=cell+n;
            float h=hash(i);
            float3 center=n+float3(hash(i+3.1),hash(i+7.7),hash(i+11.3));
            float3 d=(center-f)*float3(1,.85+h*.35,1.1);
            // Faceted clods, rather than smooth circular spots.
            float shape=max(max(abs(d.x)*.90,abs(d.y)*.86),max(abs(d.z)*.93,(abs(d.x)+abs(d.y)+abs(d.z))*.53));
            if(shape<best){best=shape;seed=h;}
        }
        return float4(best,seed,hash(cell+17.1),0);
    }
};
DirtPattern dirt;
float proximity=0;
// __VOLUME_EVALUATION__
proximity=max(proximity,MudCore);
float3 dirtP=P/max(MudCellSize,1);
float4 chip=dirt.chip(dirtP);
float radius=lerp(.06,.59,saturate(proximity))*(.38+chip.y*.85);
float mask=(1-smoothstep(radius-.018,radius+.018,chip.x))*smoothstep(.02,.14,proximity);
mask*=smoothstep(.10,.24,chip.y+proximity*.25);
mask=lerp(mask,1,saturate(MudCore));
mask=max(mask,saturate(ClodColor.r-ClodColor.g));
// Every clay surface uses the core's color function; proximity changes only
// coverage, never the clay's tint or grain.
float colorRadius=.59*(.38+chip.y*.85);
float chipHeight=saturate(1-chip.x/max(colorRadius,.05));
float fleck=pattern.noise(P*.095+float3(7,2,9));
float3 clay=lerp(MudDark.rgb,MudLight.rgb,.17+chip.y*.58+chipHeight*.15);
clay*=.88+.12*fleck;
color=lerp(color,clay,mask);
rough=lerp(rough,.94,mask);
SurfaceSpecular=lerp(BaseSpecular,MudSpecular,mask);
// Keep the relief continuous inside the clay. Nearest-cell changes affect
// color, while smooth grain avoids unstable screen derivatives at cell seams.
float clayHeight=MudDepth*(.30+pattern.noise(P*.035)*.52+fleck*.10);
Height=lerp(Height,clayHeight,mask);
Emission*=1-mask;
