"""Give the red doorway sole ownership of the approach, retaining downstream cave cuts."""
import unreal
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
actors=api.get_all_level_actors()
rock=next(a for a in actors if a.get_actor_label()=='LINK_Journey_Mine03_NaturalRock')
# Trim authored geometry, not just rendering, so its collision cannot protrude either.
component=rock.get_component_by_class(unreal.DynamicMeshComponent)
mesh=component.get_dynamic_mesh()
center,extent=rock.get_actor_bounds(False)
lo=center-extent-unreal.Vector(100,100,100)
hi=unreal.Vector(33360,center.y+extent.y+100,center.z+extent.z+100)
tool=unreal.DynamicMesh();size=hi-lo
unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=(hi+lo)*.5),size.x,size.y,size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,rock.get_actor_transform(),tool,unreal.Transform(),unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
unreal.GeometryScript_Normals.set_per_face_normals(mesh)
# Generated rock keeps an authored source; use its setter so BeginPlay cannot regenerate the old geometry.
assert unreal.MiningDesignerLibrary.set_campaign_rock_source(rock,mesh)
component.update_collision(False)
for a in actors:
    if a.get_actor_label()=='LINK_Stage_RedRoofSeal':
        a.set_actor_hidden_in_game(True);a.set_actor_enable_collision(False)
assert levels.save_current_level()
unreal.SystemLibrary.quit_editor()
