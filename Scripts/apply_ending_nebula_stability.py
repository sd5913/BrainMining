"""Save stable mip/filter and translucent AA settings for the existing space art."""
import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/TraversalFeedback'
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
report={'saved':[]}
def save(a):
    p=a.get_path_name().split('.')[0];f=root/'Content'/(p[6:]+'.uasset');b=out/'Before'/f.relative_to(root)
    if f.exists() and not b.exists():b.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,b)
    assert EA.save_loaded_asset(a,False);report['saved'].append(p)
for name in ['T_ParallaxNebula','T_WestbeamStars']:
    t=unreal.load_asset('/Game/EndingEmergence/Space/'+name);assert isinstance(t,unreal.Texture2D)
    t.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    t.set_editor_property('filter',unreal.TextureFilter.TF_TRILINEAR)
    t.set_editor_property('never_stream',True);save(t)
for name in ['NebulaField','SpaceField','CellNebula']:
    m=unreal.load_asset('/Game/EndingEmergence/Materials/M_EE_'+name);assert isinstance(m,unreal.Material)
    m.set_editor_property('enable_responsive_aa',True)
    m.set_editor_property('used_with_instanced_static_meshes',True)
    for n in ML.get_material_expressions(m):
        if not isinstance(n,unreal.MaterialExpressionCustom):continue
        code=n.get_editor_property('code')
        if '*Reveal;' in code and 'nearPlaneFade' not in code:
            # The retreat passes through huge translucent sheets. Fade before
            # the near plane cuts one away, instead of flashing a full sheet.
            code='float nearPlaneFade=smoothstep(180,1800,abs(dot(P-Cam,normalize(N))));\n'+code.replace('*Reveal;','*Reveal*nearPlaneFade;')
            n.set_editor_property('code',code)
    ML.recompile_material(m);save(m)
report['passed']=True
(out/'nebula_assets.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ENDING_NEBULA_STABILITY_SAVED')
