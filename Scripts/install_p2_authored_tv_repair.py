import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Saved/Validation/P2Repair'
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not e.get_game_world()
a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert e.get_editor_world().get_name()=='Lvl_EchoMazeToCards_P2'
actors=a.get_all_level_actors()
tv=next(x for x in actors if x.get_name()=='StaticMeshActor_348')
escape=next(x for x in actors if isinstance(x,unreal.MiningRoomEscape))
stage=next(x for x in actors if isinstance(x,unreal.CardRoomStage))
entrance=next(x for x in actors if isinstance(x,unreal.MiningGiantRoomEntrance))
glass_asset=unreal.load_asset('/Game/Passages/Echo/ArtP2/Television/SM_P2_TV_Glass')
cabinet=unreal.load_asset('/Game/Passages/Echo/ArtP2/Television/SM_P2_TV_Cabinet')
d=unreal.DynamicMesh()
unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(glass_asset,d,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
unreal.GeometryScript_MeshRepair.weld_mesh_edges(d,unreal.GeometryScriptWeldEdgesOptions(tolerance=.001,only_unique_pairs=True))
v=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(d,False)[1])
lo=unreal.Vector(min(p.x for p in v),min(p.y for p in v),min(p.z for p in v));hi=unreal.Vector(max(p.x for p in v),max(p.y for p in v),max(p.z for p in v));local_center=(lo+hi)*.5
# ActorTools returns the authored actor's world transform, while the Python
# Transform helper is inverse-oriented in this UE build. Apply its known
# authored 90-degree yaw and uniform scale explicitly.
loc=tv.get_actor_location(); center=loc+unreal.Vector(-local_center.y*26.0,local_center.x*26.0,local_center.z*26.0)
assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(d)
screen=next((x for x in actors if x.get_actor_label()=='P2 / Authored diggable television glass'),None)
if screen is None:screen=a.spawn_actor_from_class(unreal.DiggableRock,center)
screen.set_actor_label('P2 / Authored diggable television glass');screen.set_folder_path('Gameplay/TelevisionEscape')
screen.set_actor_transform(tv.get_actor_transform(),False,True)
screen.set_editor_property('rock_material',unreal.load_asset('/Game/Countryside_Interior/Materials/MI_TVSet'))
screen.set_editor_property('rock_size',hi-lo)
screen.bake_authored_mesh(d)
assert screen.get_editor_property('authored_rest_mesh')
# Make the authored screen the only collision target for the mining trace.
# Rebuild its complex-as-simple collision after baking so the editor preview
# and PIE use the same diggable surface.
for component in screen.get_components_by_class(unreal.DynamicMeshComponent):
 component.set_collision_profile_name('BlockAll')
 component.set_complex_as_simple_collision_enabled(True,True)
 component.update_collision(False)
screen.reset_rock()
tv.static_mesh_component.set_static_mesh(cabinet)
# The cabinet is a visual shell.  The authored DiggableRock screen owns the
# collision and excavation interaction; blocking the shell would prevent the
# player trace from reaching the screen.
tv.static_mesh_component.set_collision_profile_name('NoCollision')
tv.set_actor_label('SM_TVSet / Authored escape cabinet')
entry=center+unreal.Vector(308,0,0)
delta=entry-unreal.Vector(23520,11750,6700)
for key,value in {'authored_television_screen':screen,'preview_television':False,'auto_claim_victory_reward':True,'card_stage':stage,'entrance':entrance,'computer_entry':entry,'computer_checkpoint_position':unreal.Vector(tv.get_actor_location().x-660,entry.y,5945),'tunnel_end':unreal.Vector(30140,11750,10230)+delta,'exit':unreal.Vector(31300,11750,9996)+delta}.items():
 escape.set_editor_property(key,value)
# Property edits reconstruct and clear the old procedural preview components.
assert not [c for c in escape.get_components_by_class(unreal.ActorComponent) if c.component_has_tag('TelevisionEditorPreview')]
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
(out/'installed.json').write_text(json.dumps({'screen_center':str(center),'screen_size':str(hi-lo),'entry':str(entry),'checkpoint':str(escape.get_editor_property('computer_checkpoint_position')),'triangles':screen.get_editor_property('triangle_count'),'styled':str(stage.get_editor_property('styled_effects')),'systems':str(stage.get_editor_property('styled_systems'))},indent=2))
