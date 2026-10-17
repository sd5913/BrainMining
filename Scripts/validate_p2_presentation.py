"""Review P2's real four-hand sequence and record runtime asset provenance."""
import unreal,json,time,hashlib,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/IntroP2Office'
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=E.get_game_world();assert w
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old=perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'began':time.monotonic(),'frames':[]}
unreal.SystemLibrary.execute_console_command(w,'BrainMining.ReviewP2')
def tick(dt):
 try:
  world=E.get_game_world()
  if not world or time.monotonic()-s['began']>65:
   perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
   p=root/'Content/Passages/Echo/ArtP2/VFX/NS_P2_Poison.uasset'
   s['poison_sha256']=hashlib.sha256(p.read_bytes()).hexdigest()
   s['poison_asset_unchanged']=s['poison_sha256']==(out/'poison_sha256_before.txt').read_text().strip()
   (out/'presentation_runtime.json').write_text(json.dumps(s,indent=2),encoding='utf8')
   unreal.unregister_slate_post_tick_callback(handle);return
  actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CardRoomStage)
  if not actors:return
  a=actors[0]
  fx=[dict(name=c.get_name(),asset=c.get_asset().get_path_name(),active=c.is_active()) for c in a.get_components_by_class(unreal.NiagaraComponent) if c.get_asset()]
  signature=[x['name'] for x in fx if x['active']]
  if not s['frames'] or signature!=s['frames'][-1]['active']:
   s['frames'].append(dict(seconds=round(time.monotonic()-s['began'],2),active=signature,effects=fx))
   s['plush_materials']={c.get_name():c.get_material(0).get_path_name() if c.get_material(0) else None for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() in ['VelvetBody','RoundHead','Nose','BowTie']}
 except Exception:
  s['error']=traceback.format_exc();s['began']=0
handle=unreal.register_slate_post_tick_callback(tick)
