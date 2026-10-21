import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());r={}
for name,path in [('main','/Game/MainGame/Blueprints/BP_MainGameCharacter'),('shared','/Game/Showcase/Blueprints/BP_SharedMiningCharacter')]:
 c=unreal.load_class(None,path+'.'+path.rsplit('/',1)[1]+'_C');o=unreal.get_default_object(c)
 r[name]=dict(meshes=[str(x) for x in o.get_editor_property('PickaxeLevelMeshes')])
for suffix in ['', '_gold','_max']:
 m=unreal.load_asset('/Game/Pickaxe/SM_Pickaxe'+suffix)
 r['mesh'+suffix]=dict(bounds=str(m.get_bounds()),materials=[str(x.material_interface) for x in m.static_materials])
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
r['toilets']=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.HeavenToilet):r['toilets'].append(dict(name=a.get_actor_label(),scale=str(a.get_actor_scale3d()),distance=a.get_editor_property('OpenDistance'),location=str(a.get_actor_location())))
out=root/'Saved/Validation/Playtest18';out.mkdir(parents=True,exist_ok=True)
(out/'assets.json').write_text(json.dumps(r,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
