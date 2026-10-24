"""Append seven stable collectible IDs, retain existing terrain/save identities."""
import unreal,json,traceback,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/Collection23';OUT.mkdir(parents=True,exist_ok=True)
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary;DL=unreal.MiningDesignerLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False,'placements':[]}
def setp(a,k,v):a.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def xyz(p):return list(p.to_tuple())
def trace(a,b):
 h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
 return h.to_tuple() if h and h.to_tuple()[0] else None
try:
 tasks=[]
 for path in list((ROOT/'SourceAssets/Audio/BranchRewards').glob('*.wav'))+[ROOT/'SourceAssets/Collections/GPTFlowerReference.png']:
  t=unreal.AssetImportTask();t.filename=str(path);t.destination_path='/Game/Audio/Collectibles' if path.suffix=='.wav' else '/Game/CollectionPrototype/Textures';t.destination_name=path.stem;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
 assert len(tasks)==8
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
 for t in tasks[:7]:
  a=unreal.load_asset(t.destination_path+'/'+t.destination_name);assert a
  a.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.FORCE_INLINE);a.set_editor_property('sound_class_object',unreal.load_asset('/Game/Audio/Mix/SC_Effects'));EA.save_loaded_asset(a,False)
 name='M_CollectionFlowerLogo';folder='/Game/CollectionPrototype/Materials'
 m=unreal.load_asset(folder+'/'+name) if EA.does_asset_exist(folder+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(m);m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
 tx=ML.create_material_expression(m,unreal.MaterialExpressionTextureSample);tx.set_editor_property('texture',unreal.load_asset('/Game/CollectionPrototype/Textures/GPTFlowerReference'))
 inv=ML.create_material_expression(m,unreal.MaterialExpressionOneMinus);assert ML.connect_material_expressions(tx,'R',inv,'');ML.connect_material_property(inv,'',unreal.MaterialProperty.MP_OPACITY_MASK)
 tint=ML.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);tint.set_editor_property('constant',unreal.LinearColor(.008,.038,.018,1));ML.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);ML.recompile_material(m);EA.save_loaded_asset(m,False)
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(api.get_all_level_actors());field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 backup=ROOT/'Saved/Backups'/('Collection23_'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap');backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup);report['backup']=str(backup)
 placements=list(field.authored_placements)
 for Id,Region,Branch in [(16,1,1),(17,1,3),(18,2,1),(19,2,4),(20,3,1),(21,3,2),(22,3,3)]:
  assert not any(p.item_id==Id for p in placements),'Already placed '+str(Id)
  area=f'BM_Campaign_MainGame{Region:02}';region=next(a for a in aa if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(area))
  stage=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3'][Region-1];path=json.loads((ROOT/'Art/MainGame/RoutesV6'/stage/'centerlines.json').read_text())[Branch]
  origin=region.get_actor_location()+region.cutters[0].local_transform.translation;raw=lambda q:origin+unreal.Vector(q[0]*100,-q[1]*100,q[2]*100)
  candidates=[]
  for k in range(len(path['points'])-3,max(1,len(path['points'])-24),-2):
   base=raw(path['points'][k]);floor=trace(base,base-unreal.Vector(0,0,1500))
   if not floor or floor[1]:continue
   start=unreal.Vector(base.x,base.y,floor[5].z+140)
   heading=(raw(path['points'][-1])-raw(path['points'][-8])).normal()
   for direction in [heading,unreal.Vector(0,1,0),unreal.Vector(0,-1,0)]:
    h=trace(start,start+direction*1300)
    if h and not h[1] and h[9]==region.generated_rock and abs(h[7].z)<.65 and h[5].distance(start)>80:candidates.append((h,start,floor[5]));break
   if candidates:break
  assert candidates,'No safe site '+str(Id)
  h,start,ground=candidates[0];front=h[5];n=h[7];depth=180 if Id==21 else 125
  p=unreal.MiningCollectionPlacement();p.item_id=Id;p.area_tag=area;p.host_rock=h[9]
  rot=unreal.MathLibrary.make_rot_from_x(n*-1)
  p.transform=unreal.Transform(location=front-n*depth,rotation=rot,scale=unreal.Vector(1.25,1.25,1.25) if Id==21 else unreal.Vector(1,1,1))
  p.clue_transform=unreal.Transform(location=front,rotation=rot)
  approach=unreal.Vector(start.x,start.y,ground.z+100);p.approach_transform=unreal.Transform(location=approach,rotation=unreal.MathLibrary.find_look_at_rotation(approach,front));placements.append(p)
  report['placements'].append(dict(id=Id,region=Region,branch=Branch,position=xyz(p.transform.translation),clue=xyz(front),normal=xyz(n),approach=xyz(approach),host=h[9].get_name()))
 setp(field,'authored_placements',placements)
 bird=next(p for p in placements if p.item_id==3);host=bird.host_rock;assert host and host.actor_has_tag('BM_BombOnly')
 report['pigeon_before']=dict(position=xyz(bird.transform.translation),host=host.get_name(),hardness=host.hardness_level)
 setp(host,'tags',[t for t in host.tags if str(t)!='BM_BombOnly']+['BM_Collection23_PigeonSoil']);setp(host,'hardness_level',0)
 clay=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage');setp(host,'rock_material',clay)
 for c in host.get_components_by_class(unreal.MeshComponent):
  for i in range(c.get_num_materials()):c.set_material(i,clay)
 host.set_actor_label('MAIN_GAME_PigeonSoil_2_7')
 removed=[]
 for a in aa:
  if a.get_attach_parent_actor()==host and (a.actor_has_tag('BM_Polish32_BombSign') or 'Bomb' in a.get_actor_label()):removed.append(a.get_actor_label());api.destroy_actor(a)
 report['removed_bomb_signs']=removed
 # Record all start-room seams before the targeted geometry repair.
 report['startroom']=[dict(name=a.get_actor_label(),path=a.get_path_name(),bounds=[xyz(v) for v in a.get_actor_bounds(False)],materials=[c.get_material(0).get_path_name() if c.get_material(0) else '' for c in a.get_components_by_class(unreal.MeshComponent)]) for a in aa if a.get_actor_label().startswith('STARTROOM_') and isinstance(a,(unreal.StaticMeshActor,unreal.DiggableRock))]
 assert levels.save_current_level();report['total_placements']=len(placements);report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:(OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
