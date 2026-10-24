"""Recess the hidden bedrock collar behind the soil-lined entrance, preserving collision."""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/Collection23';report={'passed':False,'changed':[]}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');aa=list(api.get_all_level_actors());by={a.get_actor_label():a for a in aa}
 bounds={
 'STARTROOM_Junction_Bedrock_North':((-30250,370,5800),(-30120,2000,13400)),
 'STARTROOM_Junction_Bedrock_South':((-30250,-3000,5800),(-30120,-370,13400)),
 'STARTROOM_Junction_Bedrock_Below':((-30250,-370,5800),(-30120,370,11850)),
 'STARTROOM_Junction_Bedrock_Above':((-30250,-370,12430),(-30120,370,13400)),
 'STARTROOM_Junction_UpperShellSeal':((-30120,-320,12410),(-30040,320,12621)),
 }
 for name,(lo,hi) in bounds.items():
  a=by[name];assert isinstance(a,unreal.StaticMeshActor);p=unreal.Vector(*lo);q=unreal.Vector(*hi)
  report['changed'].append(dict(name=name,before=[list(x.to_tuple()) for x in a.get_actor_bounds(False)],min=lo,max=hi))
  a.set_actor_location((p+q)/2,False,True);a.set_actor_scale3d((q-p)/100)
 # Inspect all axis-aligned static cube surfaces for identical same-facing planes.
 cubes=[]
 for a in aa:
  if not isinstance(a,unreal.StaticMeshActor):continue
  c=a.static_mesh_component
  if not c.static_mesh or c.static_mesh.get_path_name()!='/Engine/BasicShapes/Cube.Cube':continue
  if any(abs(x)>0.01 for x in a.get_actor_rotation().to_tuple()):continue
  mid,ext=a.get_actor_bounds(False);cubes.append((a,list((mid-ext).to_tuple()),list((mid+ext).to_tuple())))
 pairs=[]
 for i,(a,lo,hi) in enumerate(cubes):
  for b,bl,bh in cubes[i+1:]:
   for axis in range(3):
    uv=[j for j in range(3) if j!=axis]
    if any(min(hi[j],bh[j])-max(lo[j],bl[j])<1 for j in uv):continue
    for side in [0,1]:
     if abs((lo if side==0 else hi)[axis]-(bl if side==0 else bh)[axis])<.05:pairs.append(dict(a=a.get_actor_label(),b=b.get_actor_label(),axis=axis,side=side))
 report['static_coplanar_candidates']=pairs
 assert levels.save_current_level();report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:(OUT/'seams.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
