"""Local entrance repair, one lowered copper vein, music voice protection and ball contacts."""
import unreal,json,sys,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
import campaign_minerals_api as minerals
out=root/'Saved/Validation/Feedback8';out.mkdir(parents=True,exist_ok=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
r={'passed':False}
def backup(path):
 dst=out/'BeforeAssets'/path
 if not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/path,dst)
try:
 for path in ['Content/MainGame/Maps/Lvl_MainGame.umap','Content/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.umap','Content/Audio/Mix/SC_Music.uasset']:backup(path)
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert L.set_current_level_by_name('Lvl_MainGame')
 aa=list(A.get_all_level_actors());shell=next(a for a in aa if a.get_name()=='DynamicMeshActor_6')
 assert shell.get_actor_label()=='EDIT_MainGame_外部通路矿区_不可挖外壳'
 comp=shell.get_component_by_class(unreal.DynamicMeshComponent);mesh=comp.get_dynamic_mesh();r['shell_before']=mesh.get_triangle_count()
 tool=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=unreal.Vector(-30180,0,12135)),600,760,650,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,shell.get_actor_transform(),tool,unreal.Transform(),unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
 assert mesh.get_triangle_count()>0 and unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh)
 unreal.GeometryScript_Normals.set_per_face_normals(mesh);comp.set_complex_as_simple_collision_enabled(True,True);comp.update_collision(False)
 r['shell_after']=mesh.get_triangle_count()
 hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));field=minerals.get_field(hall);placements=list(field.get_editor_property('authored_placements'));before=minerals._layout(field)
 index=7;old=placements[index];assert str(old.treasure_id)=='2' and abs(old.surface.translation.z-11031.852924)<1
 normal=unreal.MathLibrary.get_forward_vector(old.surface.rotation.rotator());target=old.surface.translation-unreal.Vector(0,0,80)
 hit=unreal.SystemLibrary.line_trace_single(E.get_editor_world(),target+normal*300,target-normal*500,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True).to_tuple()
 assert hit[0] and isinstance(hit[9],unreal.DiggableRock)
 assert 40<old.surface.translation.z-hit[5].z<150
 p=unreal.MiningTreasurePlacement()
 for key in ['treasure_id','value','area_tag','route_progress','guide_layer']:p.set_editor_property(key,old.get_editor_property(key))
 p.surface=unreal.Transform(location=hit[5],rotation=unreal.MathLibrary.make_rot_from_x(hit[7]));p.transform=unreal.Transform(location=hit[5]-hit[7]*120);p.hosts=[hit[9]]
 stage=minerals._stage(hall,A,field);stage.set_editor_property('authored_placements',[p])
 assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage)
 validation=minerals.validate(stage,hall);assert validation['ok'],validation['errors']
 placements[index]=list(stage.get_editor_property('authored_placements'))[0];field.set_editor_property('authored_placements',placements);A.destroy_actor(stage)
 after=minerals._layout(field);assert all(before[i]==after[i] for i in range(len(before)) if i!=index)
 r['copper']={'index':index,'before':before[index]['surface'][0],'after':after[index]['surface'][0],'validation':validation}
 assert L.save_current_level()
 assert L.load_level('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1')
 assert not any(a.get_actor_label().startswith('BallContactSeat') for a in A.get_all_level_actors()),'Contacts already authored'
 cylinder=unreal.load_asset('/Engine/BasicShapes/Cylinder');brass=unreal.load_asset('/Game/WenSi/Museum/Materials/MI_Brass');blue=unreal.load_asset('/Game/WenSi/Museum/Materials/MI_DeepBlue')
 def part(name,pos,size,mat,rotation=unreal.Rotator()):
  a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),rotation);a.set_actor_label(name);a.set_folder_path('MuseumMirror/BalanceAndCircuit');a.tags=['WenSiMechanismRevision2'];c=a.static_mesh_component;c.set_static_mesh(cylinder);c.set_material(0,mat);c.set_collision_profile_name('NoCollision');a.set_actor_scale3d(unreal.Vector(*size)/100);return a
 part('BallContactPedestal',(1900,120,54),(20,20,20),blue);part('BallContactSeat',(1900,120,65.15),(20,20,2.3),brass)
 for side in [-1,1]:
  start=unreal.Vector(1900,120+side*23,75);end=unreal.Vector(1900,120+side*9,69.7);d=end-start
  part('BallContactBridge_'+str(side),((start+end)*.5).to_tuple(),(4,4,d.length()),brass,unreal.MathLibrary.make_rot_from_z(d))
 assert L.save_current_level();r['museum_support_top']=66.3
 cls=unreal.load_asset('/Game/Audio/Mix/SC_Music');props=cls.get_editor_property('properties')
 props.set_editor_property('is_music',True);props.set_editor_property('is_ui_sound',True);props.set_editor_property('always_play',True);cls.set_editor_property('properties',props);assert unreal.EditorAssetLibrary.save_loaded_asset(cls,False)
 for path in ['/Game/Audio/Pilot/M_DreamyFlashback_Loop','/Game/Audio/RoomMusic/M_Room_CardExplore_Loop','/Game/Audio/RoomMusic/M_Room_Museum_Loop']:
  backup('Content/'+path[6:]+'.uasset');sound=unreal.load_asset(path);sound.set_editor_property('priority',100.0);sound.set_editor_property('bypass_volume_scale_for_priority',True)
  sound.set_editor_property('virtualization_mode',unreal.VirtualizationMode.PLAY_WHEN_SILENT);assert unreal.EditorAssetLibrary.save_loaded_asset(sound,False)
 r['passed']=True
except Exception:r['error']=traceback.format_exc();unreal.log_error(r['error'])
finally:
 (out/'assets.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
