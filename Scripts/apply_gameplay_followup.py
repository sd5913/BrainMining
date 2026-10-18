"""Targeted 18 October follow-up. Preserve existing terrain and mineral placement."""
import unreal,json,traceback,shutil,math,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/GameplayFollowup';OUT.mkdir(parents=True,exist_ok=True)
BACK=ROOT/'Saved/Backups/GameplayFollowup20261018/Assets';BACK.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ML=unreal.MaterialEditingLibrary;EA=unreal.EditorAssetLibrary
report={'passed':False,'placements':[],'rulers':[]}
def backup(rel):
 dst=BACK/rel
 if not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/rel,dst)
def setp(a,k,v):a.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def xyz(v):return list(v.to_tuple())
def trace(a,b):
 h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
 return h.to_tuple() if h and h.to_tuple()[0] else None
try:
 for rel in ['Content/MainGame/Maps/Lvl_MainGame.umap','Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2.umap','Content/Rooms/CardDuel/Materials/M_EscapeTVInterior.uasset'] :backup(rel)
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(api.get_all_level_actors());field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 minerals=next(a for a in aa if isinstance(a,unreal.MiningTreasureField) and a.get_editor_property('authored_placements'))
 gems=[p.transform.translation for p in minerals.get_editor_property('authored_placements')]
 gem_surfaces=[p.surface.translation for p in minerals.get_editor_property('authored_placements')]
 placements=list(field.authored_placements)
 already=field.actor_has_tag('BM_GameplayFollowup20261018')
 for p in placements:
  if p.item_id in [4,10] and not already:
   start=p.approach_transform.translation;ground=trace(start+unreal.Vector(0,0,100),start-unreal.Vector(0,0,500))
   assert ground and not ground[1]
   start.z=ground[5].z+145
   choices=[]
   for i in range(32):
    d=unreal.Vector(math.cos(i*math.pi/16),math.sin(i*math.pi/16),0)
    h=trace(start,start+d*1400)
    if not h or h[1] or h[9]!=p.host_rock or abs(h[7].z)>.55 or h[5].distance(start)<100:continue
    depth=530 if p.item_id==4 else 480
    pos=h[5]-h[7]*depth
    distance=min(pos.distance(g) for g in gems)
    surface_distance=min(h[5].distance(g) for g in gem_surfaces)
    if distance<700 or surface_distance<380:continue
    choices.append((distance+surface_distance*.3-h[5].distance(start)*.25,h,pos,distance,surface_distance))
   assert choices,'No separate burial location for '+str(p.item_id)
   _,h,pos,distance,surface_distance=max(choices,key=lambda r:r[0])
   t=p.transform;t.translation=pos;t.rotation=unreal.MathLibrary.make_rot_from_x(h[7]*-1).quaternion();p.transform=t
   p.clue_transform=unreal.Transform(location=h[5],rotation=unreal.MathLibrary.make_rot_from_x(h[7]*-1))
   approach=unreal.Vector(start.x,start.y,ground[5].z+100)
   p.approach_transform=unreal.Transform(location=approach,rotation=unreal.MathLibrary.find_look_at_rotation(approach,h[5]))
   report.setdefault('separation',[]).append(dict(id=p.item_id,core_cm=distance,clue_cm=surface_distance))
  if p.item_id==3 and not already:
   front=p.clue_transform.translation;normal=unreal.MathLibrary.transform_direction(p.clue_transform,unreal.Vector(-1,0,0));t=p.transform;t.translation=front-normal*440;t.rotation=p.clue_transform.rotation;p.transform=t
  report['placements'].append(dict(item_id=p.item_id,area_tag=str(p.area_tag),position=xyz(p.transform.translation),clue=xyz(p.clue_transform.translation),normal=xyz(unreal.MathLibrary.transform_direction(p.clue_transform,unreal.Vector(-1,0,0))),approach=xyz(p.approach_transform.translation),host=p.host_rock.get_name() if p.host_rock else None))
 setp(field,'authored_placements',placements);setp(field,'tags',list(field.tags)+([] if already else ['BM_GameplayFollowup20261018']))
 # The original elbow joins the horizontal pipe exactly. Keep only its short
 # outside section visible, at the original height; remove the detached small copy.
 original=next(a for a in aa if a.get_actor_label()=='写实弯管 · 转角1')
 outlet=next(a for a in aa if a.get_actor_label()=='COL_Heaven_SmallFaucet')
 outlet.set_actor_transform(original.get_actor_transform(),False,True)
 original.static_mesh_component.set_visibility(False)
 outlet.static_mesh_component.set_visibility(True);outlet.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 # Hide the long front straight pipe behind the wall by terminating it at the
 # elbow connection. Its buried continuation stays intact for excavation.
 report['faucet']={'location':xyz(outlet.get_actor_location()),'scale':xyz(outlet.get_actor_scale3d())}
 assert unreal.EditorLoadingAndSavingUtils.save_map(ed.get_editor_world(),'/Game/MainGame/Maps/Lvl_MainGame')
 assert levels.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
 for a in api.get_all_level_actors():
  label=a.get_actor_label()
  if isinstance(a,unreal.StaticMeshActor) and label in ['Study V09 / Spiral flight 2','Study V09 / Spiral flight 5','Study V09 / Desk ruler bridge','Study V09 / Desk ruler bridge2']:
   a.set_actor_enable_collision(True);a.static_mesh_component.set_collision_profile_name('BlockAll');report['rulers'].append(label)
 assert 'Study V09 / Spiral flight 2' in report['rulers']
 assert levels.save_current_level()
 # Export the actual viewmodel, so orientation is corrected using the head/handle
 # geometry and the saved artist transform, instead of assuming FBX axes.
 bp=unreal.load_asset('/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter');cdo=unreal.get_default_object(bp.generated_class())
 model=cdo.get_editor_property('pickaxe_model');mesh=model.static_mesh;geo=unreal.DynamicMesh()
 unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,geo,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
 pts=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(geo,False)[1])
 report['pickaxe']={'vertices':[xyz(v) for v in pts],'model_transform':str(model.get_relative_transform()),'mount_transform':str(cdo.get_editor_property('pickaxe_mount').get_relative_transform())}
 # Regenerate only the TV material from the original circuit pattern.
 sys.path.insert(0,str(ROOT/'Scripts'));from card_tv_material import build_tv_material
 build_tv_material()
 report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
