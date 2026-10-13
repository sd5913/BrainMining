// World-space PBR rock, independent of mesh UVs and stable on newly cut faces.
struct RockPattern {
    float hash(float3 x) { return frac(sin(dot(x,float3(127.1,311.7,74.7)))*43758.5453); }
    float noise(float3 x) {
        float3 i=floor(x), f=frac(x); f=f*f*(3-2*f);
        return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),
                    lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);
    }
};
RockPattern pattern;
float3 q=P*PatternScale;
float coarse=pattern.noise(q*.45);
float medium=pattern.noise(q*1.9+float3(8,2,5));
float fine=pattern.noise(P*DetailScale+float3(1,7,3));
float band=.5+.5*sin(q.z*.85+sin(q.x*.65)*1.8+q.y*.35);
float rough=Roughness;
float3 color=Tint;
Height=0;
Emission=float3(0,0,0);
if(Theme<.5) {
    float cloud=smoothstep(.18,.82,coarse);
    color=lerp(Tint,MineralLight,.20+cloud*.42);
    color=lerp(color,Accent,smoothstep(.66,.88,medium)*.06);
    rough=Roughness+RoughnessVariation*(coarse-.5);
    Height=BumpDepth*(coarse*.90+medium*.10);
} else if(Theme<1.5) {
    float phase=q.x*.7+q.y*.9+sin(q.z*.75+q.x*.3)*2.6;
    float vein=pow(saturate(1-abs(sin(phase))),14)*smoothstep(.25,.6,medium);
    color=lerp(Tint,MineralLight,.20+coarse*.45+band*.10);
    color=lerp(color,Accent,vein*VeinStrength*.35);
    rough=Roughness+RoughnessVariation*(coarse-.5);
    Height=BumpDepth*(coarse*.90+medium*.10);
} else if(Theme<2.5) {
    float phase=q.x*.45+q.y*.60+q.z*.36+medium*1.8;
    float seam=pow(saturate(1-abs(sin(phase))),9);
    color=lerp(Tint,MineralLight,.30+coarse*.45);
    color=lerp(color,Accent,seam*VeinStrength*.40);
    rough=Roughness+RoughnessVariation*(coarse-.5);
    Height=BumpDepth*(coarse*.88+medium*.12);
} else if(Theme<3.5) {
    float fold=.5+.5*sin(q.x*2.8+sin(q.y*1.35+q.z*.85)*2.4);
    float ridge=pow(saturate(fold),2.3);
    float vessel=pow(saturate(1-abs(sin(q.y*3.1+sin(q.z*1.8+q.x*.6)*2))),16)*smoothstep(.23,.57,medium);
    color=lerp(Tint,MineralLight,saturate(coarse*.55+ridge*.45));
    color=lerp(color,Accent,ridge*.31)*(1-vessel*VeinStrength*.72);
    rough=Roughness+RoughnessVariation*(fine-.5)+vessel*.10-ridge*.06;
    Height=BumpDepth*(ridge*.58+coarse*.30+fine*.12-vessel*.13);
} else {
    // Coarse stone relief remains visible on any newly exposed excavation face.
    float chips=pattern.noise(q*3.4+float3(6,5,1));
    float cleft=pow(saturate(1-abs(sin(q.x*.95+q.y*.67+q.z*.72+medium*3))),10);
    color=lerp(Tint,MineralLight,.16+coarse*.48+chips*.20);
    color=lerp(color,Accent,smoothstep(.66,.88,chips)*.12);
    color*=1-cleft*VeinStrength*.42;
    rough=Roughness+RoughnessVariation*(medium-.5);
    Height=BumpDepth*(coarse*.50+medium*.32+chips*.18-cleft*.08);
}
return float4(max(color,0.001),clamp(rough,.10,.98));
