import unreal, json, shutil, datetime
from pathlib import Path
root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/IntroP2Office';out.mkdir(parents=True,exist_ok=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not E.get_game_world()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
backup=root/'Saved/Backups'/('IntroP2Office_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
maps=['/Game/PopupClose/Maps/Lvl_PopupClose1','/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1','/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2','/Game/MainGame/Maps/Lvl_MainGame']
result={}
for m in maps:
 f=root/'Content'/(m.removeprefix('/Game/')+'.umap')
 if not f.exists():result[m]={'missing':True};continue
 shutil.copy2(f,backup/f.name)
 assert L.load_level(m)
 rows=[]
 for a in A.get_all_level_actors():
  cls=a.get_class().get_name();label=a.get_actor_label()
  if any(s in cls for s in ['Escape','CardRoomStage','GiantRoomEntrance','PopupOfficeDevice','StartRoom']) or any(s in label for s in ['Bed_','Computer','Monitor','PLAYER_START']):
   r=dict(name=a.get_name(),label=label,cls=cls,path=a.get_path_name(),loc=list(a.get_actor_location().to_tuple()),rot=str(a.get_actor_rotation()),components=[])
   for p in ['card_stage','entrance','b_build_study_layout','build_study_layout','area_managed','water','breach_wall','computer_rocks','replacement_model','replacement_transform','device_index','surface','game_mode_override']:
    try:r[p]=str(a.get_editor_property(p))
    except:pass
   for c in a.get_components_by_class(unreal.PrimitiveComponent):
    r['components'].append(dict(name=c.get_name(),visible=c.is_visible(),loc=list(c.get_world_location().to_tuple()),mesh=str(c.static_mesh) if isinstance(c,unreal.StaticMeshComponent) else ''))
   rows.append(r)
 result[m]=dict(actors=rows,game_mode=str(E.get_editor_world().get_world_settings().get_editor_property('default_game_mode')))
(out/'audit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('INTRO_P2_OFFICE_AUDIT_DONE '+str(backup))
