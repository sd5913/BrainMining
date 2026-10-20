"""Enclose the heaven branch well outside its mineable dome, with a soil buffer."""
import unreal,json,shutil
from pathlib import Path

root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/TraversalFeedback';out.mkdir(parents=True,exist_ok=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert L.set_current_level_by_name('Lvl_MainGame')
file=root/'Content/MainGame/Maps/Lvl_MainGame.umap';backup=out/'Before/Content/MainGame/Maps/Lvl_MainGame.umap'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(file,backup)
aa=list(A.get_all_level_actors());shell=next(a for a in aa if a.get_actor_label()=='COL_Heaven_DiggableDomeShell')
origin=shell.get_actor_location();I=unreal.Transform();opt=unreal.GeometryScriptPrimitiveOptions()
def V(x):return unreal.Vector(*x)
def box(center,size):
    m=unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_box(m,opt,unreal.Transform(location=V(center)),dimension_x=size[0],dimension_y=size[1],dimension_z=size[2],origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    return m
def subtract(a,b):
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(a,I,b,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(a)
def actor(label,cls):
    a=next((a for a in aa if a.get_actor_label()==label),None) or A.spawn_actor_from_class(cls,origin)
    a.set_actor_label(label);a.set_folder_path('MainGame/Collections/HeavenBoundary')
    a.set_editor_property('tags',['BM_Campaign_MainGame03','BM_MainGame_Collections','BM_HeavenOuterBoundary'])
    return a

# The existing dome's box is 4240 wide and rises to +4380. The new outer
# bedrock begins at +/-2900, leaving 7.8 metres beyond the sides and roof.
soil=box((0,-575,2325),(5840,7290,5790))
subtract(soil,box((0,0,2130),(4220,4220,4480)))
# Preserve the complete authored approach and seam collar, including dirt layers.
branch=box((-231,-3200,250),(1100,3300,1200))
subtract(soil,branch)
unreal.GeometryScript_Normals.set_per_face_normals(soil)
buffer=actor('COL_Heaven_OuterDiggableBuffer',unreal.GeneratedMiningRock)
buffer.set_editor_property('rock_material',shell.rock_material,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
buffer.set_editor_property('hardness_level',1)
assert unreal.MiningDesignerLibrary.set_campaign_rock_source(buffer,soil)

# Closed box shell encloses the buffer and branch. Its only opening connects
# to the existing entrance; it does not track the hemisphere surface.
bedrock=box((0,-575,2325),(6160,7610,6110))
subtract(bedrock,box((0,-575,2325),(5800,7250,5750)))
subtract(bedrock,box((-231,-4300,350),(1150,650,1400)))
unreal.GeometryScript_Normals.set_per_face_normals(bedrock)
boundary=actor('COL_Heaven_OuterUnmineableBoundary',unreal.DynamicMeshActor)
c=boundary.get_component_by_class(unreal.DynamicMeshComponent)
c.get_dynamic_mesh().reset();unreal.GeometryScript_MeshEdits.append_mesh(c.get_dynamic_mesh(),bedrock,I)
c.set_material(0,unreal.load_asset('/Game/CollectionPrototype/Materials/M_CollectionBedrock'))
c.set_collision_profile_name('BlockAll');c.set_complex_as_simple_collision_enabled(True,True);c.update_collision(False)
# Old campaign bedrock used to hug the dome. Move that protection out to the
# new boundary too, so side/roof routes through the buffer remain excavatable.
opened=[]
clearance=box((0,-575,2325),(5800,7250,5750))
for a in aa:
    if not isinstance(a,unreal.DynamicMeshActor) or not a.actor_has_tag('BM_Campaign_MainGame03') or a==boundary:continue
    center,extent=a.get_actor_bounds(False);q=center-origin
    if abs(q.x)>extent.x+2900 or abs(q.y+575)>extent.y+3625 or abs(q.z-2325)>extent.z+2875:continue
    source=unreal.MiningDesignerLibrary.get_campaign_rock_source(a)
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(source,a.get_actor_transform(),clearance,unreal.Transform(location=origin),unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(source)
    component=a.get_component_by_class(unreal.DynamicMeshComponent)
    component.get_dynamic_mesh().reset();unreal.GeometryScript_MeshEdits.append_mesh(component.get_dynamic_mesh(),source,I);component.update_collision(False)
    opened.append(a.get_actor_label())
assert L.save_current_level()
report={'passed':True,'map':'/Game/MainGame/Maps/Lvl_MainGame','dome_unchanged':True,'buffer':buffer.get_path_name(),'boundary':boundary.get_path_name(),'origin':list(origin.to_tuple()),'side_clearance_cm':780,'roof_clearance_cm':820,'entrance_opening_width_cm':1150,'soil_triangles':soil.get_triangle_count(),'bedrock_triangles':bedrock.get_triangle_count(),'old_bedrock_opened':opened}
(out/'heaven.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('HEAVEN_OUTER_BOUNDARY_COMPLETE')
