"""Dress the pictured floor plush as well as the animated card opponent."""
import unreal,json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.get_current_level().get_outer().get_name()=='Lvl_EchoMazeToCards_P2'
root=Path(unreal.Paths.project_dir()).resolve();rows=json.loads((root/'Saved/Automation/CountrysideP2/before.json').read_text(encoding='utf-8'))
base='/Game/Passages/Echo/ArtP2';tag=unreal.Name('CountrysideP2Art')
for a in list(A.get_all_level_actors()):
 if isinstance(a,unreal.MiningGiantRoomEntrance):
  for c in a.get_components_by_class(unreal.PostProcessComponent):
   settings=c.get_editor_property('settings');settings.set_editor_property('override_auto_exposure_bias',True);settings.set_editor_property('auto_exposure_bias',-5.0);c.set_editor_property('settings',settings)
 if a.get_actor_label().startswith('P2 Countryside / Floor/Curled ginger cat') or a.get_actor_label().startswith('P2 Countryside / Floor/Handmade plush '):A.destroy_actor(a)
for r in rows:
 if not r['label'].startswith('Study V10 / Floor plush '):continue
 n=r['label'].split('Floor plush ')[1]
 mn='SM_PlushBody' if 'Body' in n else 'SM_PlushHead' if 'Head' in n else 'SM_PlushEar' if 'Ear' in n else 'SM_PlushPaw' if 'Mitten' in n else 'SM_PlushMuzzle' if 'Muzzle' in n else 'SM_PlushBow' if 'Bow' in n else 'SM_PlushButton'
 a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*r['loc']),unreal.Rotator(pitch=r['rotation'][0],yaw=r['rotation'][1],roll=r['rotation'][2]));a.set_actor_scale3d(unreal.Vector(*r['scale']));a.set_actor_enable_collision(False)
 a.set_actor_label('P2 Countryside / Floor/Handmade plush '+n);a.set_folder_path('GiantRoom/CountrysideP2/FloorPlush');a.tags=[tag]
 c=a.static_mesh_component
 if any(k in n for k in ['Brow','Smile','NoseStem','Mouth']):c.set_static_mesh(unreal.load_asset(r['components'][0]['mesh']))
 else:c.set_static_mesh(unreal.load_asset(base+'/Meshes/'+mn))
 mat='M_PlushCream' if 'Muzzle' in n else 'M_PlushNose' if n=='Nose' else 'M_PlushBow' if 'Bow' in n else 'M_PlushInk' if any(k in n for k in ['Eye','Brow','Smile','NoseStem','Mouth']) else 'M_PlushFur'
 c.set_material(0,unreal.load_asset(base+'/Materials/'+mat));c.set_cast_shadow(True)
# Seat the new decorative library books on the carved asset's real shelves.
oldbooks=[r for r in rows if r['label']=='Study / Shelf book']
for a in A.get_all_level_actors():
 if 'Library/Bound volume ' not in a.get_actor_label():continue
 index=int(a.get_actor_label().rsplit(' ',1)[1]);r=oldbooks[index];height=r['scale'][2]*100
 row=max(0,min(3,round((r['loc'][2]-height*.5-120)/2060)))
 factor=.54 if row==3 else .65;bounds=a.static_mesh_component.static_mesh.get_bounds();sc=a.get_actor_scale3d();sc.y=height*factor/(bounds.box_extent.y*2);a.set_actor_scale3d(sc)
 p=a.get_actor_location();p.z=[2980,4600,6140,7580][row]+height*factor*.5+bounds.origin.y*sc.y;a.set_actor_location(p,False,False)
assert L.save_current_level()
unreal.log('P2_PICTURED_FLOOR_PLUSH_REFINED')
