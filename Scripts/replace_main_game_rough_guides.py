"""Import four validated guide FBXs; rebuild only the 64 guide rocks via the external tool."""
import unreal,json,time,traceback,runpy,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ART=ROOT/'Art/MainGame/BlockersRockV2';OUT=ROOT/'Saved/Validation/main_game_rough_guides.json'
recipe=json.loads((ROOT/'Config/MainGameRockThemes.json').read_text());lib=unreal.MaterialEditingLibrary
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
state={'passed':False,'phase':'starting'}
def checkpoint(**kw):state.update(kw);OUT.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('ROUGH_GUIDES '+json.dumps(kw,ensure_ascii=False))
helper=(ROOT/'Scripts/create_main_game.py').read_text().split('\ntry:\n')[0];ns={'__name__':'guide_import_helpers'};exec(compile(helper,'guide_import_helpers','exec'),ns)
mi_helper=(ROOT/'Scripts/update_main_game_rock_themes.py').read_text().split('\ntry:\n')[0];mn={'__name__':'guide_material_helpers'};exec(compile(mi_helper,'guide_material_helpers','exec'),mn)
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']
def fingerprint(region):
    rock=region.generated_rock
    return dict(actor=rock.get_path_name(),triangles=rock.triangle_count,chunks=len(rock.chunks),cuts=rock.successful_cuts,
                shell=region.generated_shell.get_path_name(),material=rock.rock_material.get_path_name())
try:
    regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion));before={r.get_path_name():fingerprint(r) for r in regions}
    parent=unreal.load_asset(recipe['folder']+'/'+recipe['master']);prepared=[]
    for i,stage in enumerate(STAGES):
        validation=json.loads((ART/stage/'validation.json').read_text());assert validation['passed'] and validation['count']==16
        theme=recipe['themes'][i];r=next(r for r in regions if theme['tag'] in [str(t) for t in r.tags])
        assert len(r.guide_layers)==16 and len(r.generated_guide_layers)==16
        ns['ART']=ART/stage;meshes=ns['import_fbx']('Tunnel_Blockers.fbx','/Game/MainGame/Models/RoughGuides/'+stage,16)
        names={m.get_name():m for m in meshes};layers=[]
        plan=json.loads((ROOT/'Art/MainGame'/stage/'placement_plan.json').read_text())['objects']
        for index,record in enumerate(plan):
            mesh=next(m for m in meshes if m.get_name().endswith(record['name']))
            old=r.guide_layers[index];layer=unreal.MiningExternalGuideLayer()
            layer.set_editor_property('mesh',mesh);layer.set_editor_property('local_transform',old.local_transform);layer.set_editor_property('enabled',old.enabled);layers.append(layer)
        scalars=dict(Theme=4,Roughness=.80,RoughnessVariation=.10,Specular=.22,PatternScale=.010,DetailScale=.035,BumpDepth=6.0,VeinStrength=.58,GlowStrength=0,Metallic=0)
        palettes=[[[.17,.15,.12],[.36,.33,.27],[.52,.49,.42]],[[.12,.145,.16],[.28,.31,.33],[.42,.46,.46]],[[.20,.18,.14],[.39,.37,.31],[.57,.55,.46]],[[.16,.075,.065],[.32,.17,.14],[.46,.30,.24]]]
        mi=mn['instance'](dict(name=f'MI_Guide_{i+1:02}_RoughStone',scalars=scalars,vectors=dict(zip(['Tint','MineralLight','Accent'],palettes[i]))),parent)
        guide_material=unreal.load_asset(recipe['guide_material']) if recipe.get('guide_material') else mi
        assert guide_material
        prepared.append((r,layers,guide_material,validation))
        checkpoint(phase='importing',prepared=i+1)
    # Validate all source assets before replacing any existing guide actors.
    changed=[]
    for r,layers,mi,validation in prepared:
        r.set_editor_property('guide_layers',layers,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        r.set_editor_property('guide_material',mi,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        assert r.generate_guide_layers(),r.guide_build_status
        assert len(r.generated_guide_layers)==16
        assert all(g.rock_material==mi and g.triangle_count>0 for g in r.generated_guide_layers)
        assert fingerprint(r)==before[r.get_path_name()],'Ordinary terrain or shell changed'
        changed.append(dict(region=r.get_actor_label(),count=16,material=mi.get_path_name(),fill_fraction=validation['fill_fraction'],
            source_triangles=validation['total_triangles'],generated_triangles=sum(g.triangle_count for g in r.generated_guide_layers),
            chunks=sum(len(g.chunks) for g in r.generated_guide_layers),status=r.guide_build_status))
        checkpoint(phase='replacing_guides',changed=changed)
    assert all(fingerprint(r)==before[r.get_path_name()] for r in regions)
    assert levels.save_current_level();assert unreal.EditorAssetLibrary.save_directory('/Game/MainGame/Models/RoughGuides',only_if_is_dirty=True,recursive=True)
    assert unreal.EditorAssetLibrary.save_directory('/Game/MainGame/Materials',only_if_is_dirty=True,recursive=True)
    checkpoint(passed=True,phase='complete',changed=changed,count=64,ordinary_terrain_and_shells_preserved=True,saved=True)
except Exception:checkpoint(phase='failed',error=traceback.format_exc());raise
