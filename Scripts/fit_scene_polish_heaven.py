"""Fit campaign terrain to the latest source room, retaining its diggable entrance."""
import unreal,json,traceback,shutil,math
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/ScenePolish'
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);lib=unreal.MiningDesignerLibrary
report={'passed':False,'terrain':[],'minerals':[]}
I=unreal.Transform();opt=unreal.GeometryScriptPrimitiveOptions()
def box(center,size):
 m=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_box(m,opt,unreal.Transform(location=center),dimension_x=size.x,dimension_y=size.y,dimension_z=size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER);return m
def sub(m,t,tool,tt=I):
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(m,t,tool,tt,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
try:
 backup=ROOT/'Saved/Backups'/('HeavenResize_'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
 shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');aa=list(A.get_all_level_actors())
 water=next(a for a in aa if isinstance(a,unreal.HeavenToiletWater));frame=water.spill_coordinate_frame
 # The larger dome would intersect Branch_01 and its malachite at the old center.
 # Move the chamber 11 m farther along its own entrance, keeping the exposed clue fixed.
 shifted='BM_HeavenRoomClearance1100'
 if not water.actor_has_tag(shifted):
  delta=unreal.MathLibrary.transform_direction(frame,unreal.Vector(1100,0,0))
  for a in aa:
   if a.actor_has_tag('BM_ScenePolishHeaven') and a.get_actor_label() not in ['COL_Heaven_ExposedClueDrip','COL_Heaven_BuriedPipeConnection']:
    a.set_actor_location(a.get_actor_location()+delta,False,True)
  frame.translation=frame.translation+delta;water.spill_coordinate_frame=frame;water.outer_x=-1400.;water.tags=list(water.tags)+[shifted];water.rebuild_water()
  collection=next(a for a in aa if isinstance(a,unreal.MiningCollectionField));pp=list(collection.authored_placements)
  p=next(p for p in pp if p.item_id==0);t=p.transform;t.translation=t.translation+delta;p.transform=t;collection.authored_placements=pp
  report['chamber_offset_cm']=1100
 original_frame=unreal.Transform(location=frame.translation-unreal.MathLibrary.transform_direction(frame,unreal.Vector(1100,0,0)),rotation=frame.rotation.rotator())
 center=unreal.MathLibrary.transform_location(frame,unreal.Vector(2641,-231,0))
 shell=next(a for a in aa if a.get_actor_label()=='COL_Heaven_DiggableDomeShell')
 # Source HDRI bounds: radius 1920.7 cm and height 4120.6 cm. Add clearance.
 radius=1980.;height=4300.
 cavity=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_cylinder(cavity,opt,unreal.Transform(location=center+unreal.Vector(0,0,(height-60)/2)),radius=radius,height=height+60,radial_steps=64,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 for a in aa:
  if not isinstance(a,unreal.GeneratedMiningRock) or a==shell or not a.actor_has_tag('BM_Campaign_MainGame03'):continue
  c,e=a.get_actor_bounds(False)
  if abs(c.x-center.x)>e.x+radius or abs(c.y-center.y)>e.y+radius or c.z+e.z<center.z or c.z-e.z>center.z+height:continue
  mesh=lib.get_campaign_rock_source(a);assert mesh
  before=mesh.get_triangle_count();sub(mesh,a.get_actor_transform(),cavity)
  assert lib.set_campaign_rock_source(a,mesh),a.get_name()
  report['terrain'].append({'actor':a.get_name(),'before':before,'after':mesh.get_triangle_count()})
 shell.set_actor_location(center,False,True);shell.set_actor_rotation(unreal.Rotator(),False)
 mesh=box(unreal.Vector(0,0,2130),unreal.Vector(4240,4240,4500));sub(mesh,shell.get_actor_transform(),cavity)
 mouth=box(unreal.MathLibrary.transform_location(frame,unreal.Vector(600,0,230)),unreal.Vector(660,2200,460));sub(mesh,shell.get_actor_transform(),mouth)
 assert lib.set_campaign_rock_source(shell,mesh)
 # Extend the existing narrow entrance to meet the relocated chamber mouth.
 for a in aa:
  if a.get_actor_label().startswith(('COL_Heaven_BranchRoof','COL_Heaven_BranchWall','COL_Heaven_BranchFloor')):
   p=unreal.MathLibrary.inverse_transform_location(original_frame,a.get_actor_location());p.x=690
   a.set_actor_location(unreal.MathLibrary.transform_location(original_frame,p),False,True)
   size=a.rock_size;size.y=2220;a.rock_size=size;a.reset_rock()
 connector=next(a for a in aa if a.get_actor_label()=='COL_Heaven_BuriedPipeConnection')
 start=unreal.MathLibrary.transform_location(original_frame,unreal.Vector(-269,125,220));end=unreal.MathLibrary.transform_location(frame,unreal.Vector(181,29,220));bounds=connector.static_mesh_component.static_mesh.get_bounding_box()
 connector.set_actor_location((start+end)/2,False,True);connector.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(start,end),False);connector.set_actor_scale3d(unreal.Vector(start.distance(end)/(bounds.max.x-bounds.min.x),.181716,.181716))
 # Extend the existing floor underneath the enlarged skyline, with its original material.
 floor=next(a for a in aa if a.get_actor_label().startswith('Heaven circular floor'))
 floor.set_actor_location(center-unreal.Vector(0,0,21),False,True);floor.set_actor_scale3d(unreal.Vector(39.6,39.6,.42))
 pp=next(a for a in aa if a.get_actor_label()=='COL_Heaven_LocalExposure')
 pp.set_actor_location(center+unreal.Vector(0,0,2100),False,True);pp.set_actor_scale3d(unreal.Vector(21,21,23))
 # Keep the pre-existing outer-wall mineral buried in rock after the room grows.
 field=next(a for a in aa if isinstance(a,unreal.MiningTreasureField));placements=list(field.get_editor_property('authored_placements'))
 for index,p in enumerate(placements):
  q=p.transform.translation-center
  if str(p.area_tag)=='BM_Campaign_MainGame03' and q.z>450 and q.z<1500 and math.hypot(q.x,q.y)<radius:
   target=center+unreal.Vector(2050,0,q.z);delta=target-p.transform.translation
   for k in ['transform','surface']:
    t=p.get_editor_property(k);t.translation=t.translation+delta;p.set_editor_property(k,t)
   fragments=list(p.fragments)
   for t in fragments:t.translation=t.translation+delta
   p.fragments=fragments;p.fragment_supports=[v+delta for v in p.fragment_supports];p.hosts=[shell];report['minerals'].append(index)
 field.set_editor_property('authored_placements',placements)
 assert L.save_current_level();report.update(passed=True,center=list(center.to_tuple()),radius=radius,height=height)
except Exception:report['error']=traceback.format_exc()
(OUT/'heaven_fit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
