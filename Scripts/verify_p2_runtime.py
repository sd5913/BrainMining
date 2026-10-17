"""Launch P2, exercise real card flow, and leave the finished art map open."""
import unreal,time,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Automation/CountrysideP2'
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ED=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
finalpath='/Game/Passages/Echo/ArtP2/VFX/NS_P2_Finale'
if not (ROOT/'Content/Passages/Echo/ArtP2/VFX/NS_P2_Finale.uasset').exists():
 final=unreal.EditorAssetLibrary.duplicate_asset('/Game/Realistic_Starter_VFX_Pack_Niagara/Niagara/Explosion/NS_Explosion_Big_4',finalpath)
 assert final
 assert unreal.EditorAssetLibrary.save_loaded_asset(final,only_if_is_dirty=False)
 assert (ROOT/'Content/Passages/Echo/ArtP2/VFX/NS_P2_Finale.uasset').exists()
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
f=str(ROOT/'Scripts/finish_countryside_floor_cat.py');exec(compile(Path(f).read_text(encoding='utf-8'),f,'exec'),{'__file__':f})
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
csv=OUT/'runtime-vfx.csv'
if csv.exists():csv.rename(OUT/('runtime-vfx-before-'+str(int(time.time()))+'.csv'))
s={'phase':0,'time':time.monotonic(),'began':time.monotonic(),'fired':set()}
def done(ok,reason):
 (OUT/'runtime_validation.json').write_text(json.dumps(dict(passed=ok,reason=reason,elapsed=time.monotonic()-s['began']),indent=2),encoding='utf-8')
 L.editor_request_end_play();s['phase']=9;s['time']=time.monotonic();unreal.log('P2_RUNTIME_VALIDATION '+str(ok)+' '+reason)
def tick(dt):
 try:
  now=time.monotonic()
  if s['phase']==9:
   if now-s['time']>3:
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(11300,6900,4300),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(11300,6900,4300),unreal.Vector(15400,3600,4900)))
    unreal.unregister_slate_post_tick_callback(handle)
    f=str(ROOT/'Scripts/capture_countryside_p2.py');exec(compile(Path(f).read_text(encoding='utf-8'),f,'exec'),{'__file__':f})
   return
  if now-s['began']>140:done(False,'Timed out waiting for real encounter');return
  if s['phase']==0 and now-s['time']>2:L.editor_request_begin_play();s['phase']=1;s['time']=now;return
  w=ED.get_game_world()
  if not w:return
  pawn=unreal.GameplayStatics.get_player_character(w,0);pc=unreal.GameplayStatics.get_player_controller(w,0)
  if not pawn or not pc:return
  if s['phase']==1:
   pawn.set_actor_location(unreal.Vector(12060,3600,3120),False,True);pc.set_control_rotation(unreal.Rotator(pitch=20,yaw=0));unreal.GameplayStatics.set_global_time_dilation(w,.35);s['phase']=2;s['time']=now;return
  if s['phase']==2 and now-s['time']>2:
   unreal.SystemLibrary.execute_console_command(w,'CardRoom.P2AutoPlay');s['phase']=3;s['time']=now;return
  if s['phase']==3:
   for stage in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage):
    for c in stage.get_components_by_class(unreal.NiagaraComponent):
     if c.component_has_tag('P2Fired'):s['fired'].add(c.get_name())
  if s['phase']==3 and csv.exists():
   text=csv.read_text(encoding='utf-8-sig');rows=text.splitlines();rounds=[x for x in rows if x and x[0].isdigit()]
   expected=[{'P2Fx_ArcHit'},{'P2Fx_PoisonHit'},{'P2Fx_ComboFire','P2Fx_ComboSparks'},{'P2Fx_FinaleImpact','P2Fx_FinaleSparks','P2Fx_FinaleConfetti'}]
   actual=[set(x.split(',')[4].split('|')) if len(x.split(','))>4 else set() for x in rounds]
   # One-shot systems may finish before the screenshot. Check that they fired as well as active-set isolation.
   semantics=len(actual)==4 and actual[:3]==expected[:3] and actual[3].issubset(expected[3]) and {'P2Fx_FinaleImpact','P2Fx_FinaleSparks','P2Fx_FinaleConfetti'}.issubset(s['fired'])
   (OUT/'skill_vfx_validation.json').write_text(json.dumps(dict(passed=semantics,expected=[sorted(x) for x in expected],actual=[sorted(x) for x in actual],fired=sorted(s['fired'])),indent=2),encoding='utf-8')
   good=len(rounds)==4 and all('SM_PlushBody' in x for x in rounds) and 'victory,player_hp=1,rival_hp=0,rounds=4' in text and semantics
   (OUT/'runtime_meshes.json').write_text(json.dumps([dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,material=c.get_material(0).get_path_name() if c.get_material(0) else None) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage) for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() in ['VelvetBody','RoundHead','LeftEar']],indent=2),encoding='utf-8')
   done(good,'Four rounds completed through real staging/confirm APIs; skill-specific VFX sets '+str(semantics)+'; player HP 1, rival HP 0; plush mesh survived BeginPlay');return
 except Exception:done(False,traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
