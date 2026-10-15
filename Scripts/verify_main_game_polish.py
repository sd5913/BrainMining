"""Saved editor acceptance for reduced plugs, matte surfaces and wall minerals."""
import unreal,json,sys,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
OUT=ROOT/'Saved/Validation/MainGamePolish';report={'passed':False,'checks':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
try:
    ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world,hall,actors=api._context();all_actors=list(actors.get_all_level_actors());cfg=json.loads((ROOT/'Config/MainGameTuning.json').read_text())
    journal=json.loads((OUT/'authoring.json').read_text());regions=[a for a in all_actors if isinstance(a,unreal.MiningExternalRegion)]
    for r in regions:
        original=journal['geometry_before'][r.get_path_name()]
        check(r.generated_rock.get_path_name()==original['rock'] and r.generated_rock.triangle_count==original['triangles'],r.get_actor_label()+' ordinary geometry unchanged after reload')
        expected='/Game/MainGame/Materials/DirtTransition/'
        check(r.rock_material.get_path_name().startswith(expected) and r.shell_material.get_path_name().startswith(expected),r.get_actor_label()+' rock and bedrock use matte material family')
        if not r.actor_has_tag('BM_MainGame_RoutesV2'):continue
        check([i for i,l in enumerate(r.guide_layers) if l.enabled]==cfg['active_blocker_indices'] and len(r.generated_guide_layers)==8,r.get_actor_label()+' exactly eight alternating plugs survive reload')
        check(all(g.rock_material.get_name()=='MI_ClayBlockage' for g in r.generated_guide_layers),r.get_actor_label()+' remaining plugs use dark moss soil')
    entrance=[a for a in all_actors if a.get_actor_label()=='STARTROOM_Junction_DiggableBlockage']
    check(len(entrance)==1,'Entrance teaching plug retained')
    lights=[a for a in all_actors if a.get_components_by_class(unreal.LightComponent)]
    check(not [a for a in lights if a.get_level()==hall.get_level()],'All persistent corridor fixed lights removed')
    check(len([a for a in lights if 'Lvl_StartRoom' in a.get_level().get_path_name()])==5,'Five inherited StartRoom lights retained')
    lib=unreal.MaterialEditingLibrary;master=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/M_DirtTransition')
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,1),(unreal.MaterialProperty.MP_SPECULAR,0),(unreal.MaterialProperty.MP_METALLIC,0),(unreal.MaterialProperty.MP_EMISSIVE_COLOR,0)]:
        node=lib.get_material_property_input_node(master,prop)
        check(isinstance(node,unreal.MaterialExpressionConstant) and node.get_editor_property('r')==value,str(prop)+' hard wired to '+str(value))
    check(master.get_editor_property('shading_model')==unreal.MaterialShadingModel.MSM_DEFAULT_LIT,'Excavation material remains normally lit by the headlamp')
    for p in unreal.EditorAssetLibrary.list_assets('/Game/MainGame/Materials/DirtTransition'):
        m=unreal.load_asset(p)
        if not isinstance(m,unreal.MaterialInstanceConstant):continue
        s={str(v.parameter_info.name):v.parameter_value for v in m.get_editor_property('scalar_parameter_values')}
        check(all(abs(s[k]-v)<1e-5 for k,v in {'Roughness':1,'Specular':0,'Metallic':0,'GlowStrength':0,'MudSpecular':0,'MossAmount':.34}.items()),m.get_name()+' matte settings and moss coverage saved')
    field=api.get_field(hall);valid=api.validate(field,hall)
    check(valid['ok'] and valid['deposits']==cfg['expected_deposits'],'All 165 saved mineral placements pass burial and fragment validation')
    placements=list(field.get_editor_property('authored_placements'))
    check(sum(not p.guide_layer for p in placements)==145,'145 ordinary layer deposits saved')
    check(sum(p.surface.rotation.rotator().pitch<-30 for p in placements)==44,'44 ceiling deposits retained')
    check(all(not p.guide_layer and p.surface.rotation.rotator().pitch>=-30 for p in placements[125:]),'All 40 added deposits are ordinary tunnel wall sites')
    for p,digest in journal['protected_hashes'].items():check(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest,'Protected asset unchanged: '+p)
    report.update(passed=True,saved=True,minerals_sha256=valid['layout_sha256'],removed_lights=journal['removed_lights'],backup=journal['backup'])
except Exception:report['error']=traceback.format_exc();raise
finally:
    (OUT/'saved_state.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('POLISH_SAVED_CHECK '+json.dumps(report,ensure_ascii=False))
