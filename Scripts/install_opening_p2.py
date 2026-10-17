import unreal,json
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not E.get_game_world()
EA=unreal.EditorAssetLibrary
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
stage=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.CardRoomStage))
stage.set_editor_property('styled_effects',True)
base='/Game/Passages/Echo/ArtP2/StyledVFX/'
systems=[unreal.load_asset(base+'NS_Styled_'+n) for n in ['ArcImpact','Ignition','Finale','Muzzle']]
systems.append(unreal.load_asset('/Game/Passages/Echo/ArtP2/VFX/NS_P2_Poison'))
assert all(systems)
stage.set_editor_property('styled_systems',systems)
stage.set_editor_property('styled_energy_material',unreal.load_asset(base+'M_SoftEnergy'))
stage.set_editor_property('styled_ring_material',unreal.load_asset(base+'M_SoftImpactRing'))
toy_meshes={};toy_materials={}
for c in stage.get_components_by_class(unreal.StaticMeshComponent):
 n=c.get_name()
 if n in ['VelvetBody','RoundHead','LeftEar','RightEar','LeftMitten','RightMitten','LeftMuzzle','RightMuzzle','LeftEye','RightEye','Nose','BowTie','Mouth','LeftBrow','RightBrow','SmileLeft','SmileRight','NoseStem']:
  kind='Cream' if 'Muzzle' in n else 'Ink' if any(s in n for s in ['Eye','Mouth','Brow','Smile','Stem']) else 'Nose' if n=='Nose' else 'Bow' if n=='BowTie' else 'Fur'
  mat=unreal.load_asset('/Game/Passages/Echo/ArtP2/Materials/M_Plush'+kind);assert mat
  toy_materials[unreal.Name(n)]=mat;c.set_material(0,mat)
  mn='SM_PlushBody' if 'Body' in n else 'SM_PlushHead' if 'Head' in n else 'SM_PlushEar' if 'Ear' in n else 'SM_PlushPaw' if 'Mitten' in n else 'SM_PlushMuzzle' if 'Muzzle' in n else 'SM_PlushBow' if 'Bow' in n else 'SM_PlushButton' if 'Eye' in n or n=='Nose' else None
  if mn:
   mesh=unreal.load_asset('/Game/Passages/Echo/ArtP2/Meshes/'+mn);assert mesh
   toy_meshes[unreal.Name(n)]=mesh;c.set_static_mesh(mesh)
stage.set_editor_property('styled_toy_meshes',toy_meshes)
stage.set_editor_property('styled_toy_materials',toy_materials)
assert L.save_current_level()
stage_name=stage.get_name();system_paths=[x.get_path_name() for x in systems]
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
assert L.set_current_level_by_name('Lvl_MainGame')
allactors=A.get_all_level_actors();bed=next(a for a in allactors if a.get_actor_label()=='Bed_Mattress')
origin=bed.get_actor_location()-unreal.Vector(-260,-225,52)
opening=next((a for a in allactors if isinstance(a,unreal.MiningOpening)),None) or A.spawn_actor_from_class(unreal.MiningOpening,origin)
opening.set_actor_label('OPENING / Bed to doorway');opening.set_folder_path('MainGame/Opening')
hall=next(a for a in allactors if isinstance(a,unreal.MiningExhibitHall))
opening.set_editor_property('tags',[hall.get_editor_property('areas')[0].get_editor_property('tag')])
opening.set_editor_property('title_texture',unreal.load_asset('/Game/StartRoom/UI/T_OpeningTitle'))
opening.set_editor_property('seat_eye',origin+unreal.Vector(-260,-160,127))
opening.set_editor_property('standing_location',origin+unreal.Vector(-260,-70,98))
opening.set_editor_property('facing',unreal.Rotator(yaw=13))
opening.set_editor_property('door_title_location',origin+unreal.Vector(466,0,168))
opening.set_editor_property('door_title_rotation',unreal.Rotator(yaw=180))
assert L.save_current_level()
Path(unreal.Paths.project_dir(),'Saved/Validation/IntroP2Office/installed.json').write_text(json.dumps(dict(origin=list(origin.to_tuple()),opening=opening.get_path_name(),p2_stage=stage_name,systems=system_paths),indent=2),encoding='utf8')
unreal.log('OPENING_P2_INSTALLED')
