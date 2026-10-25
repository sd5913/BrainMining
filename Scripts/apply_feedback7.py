"""Narrow authoring update for the seven-item player feedback pass."""
import unreal,json,sys,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Feedback7';out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,wire,prop,finish,custom
r={'passed':False}
try:
 for rel in ['MainGame/Maps/Lvl_MainGame.umap','CollectionPrototype/Materials/M_BranchAroma.uasset','Subtitles/DA_LeaveStartRoom.uasset']:
  b=root/'Saved/Backups/Feedback7'/rel;b.parent.mkdir(parents=True,exist_ok=True)
  if not b.exists():shutil.copy2(root/'Content'/rel,b)
 m=mat('/Game/CollectionPrototype/Materials/M_BranchAroma');m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
 uv=node(m,unreal.MaterialExpressionTextureCoordinate);time=node(m,unreal.MaterialExpressionTime)
 alpha=custom(m,'float2 p=(UV-.5)*2; float r=length(p); float wisps=.72+.28*sin(p.x*7+sin(p.y*8+Time*.55)); return (1-smoothstep(.12,1,r))*exp(-r*r*2.4)*wisps*.12;',{'UV':(uv,''),'Time':(time,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
 depth=node(m,unreal.MaterialExpressionDepthFade,fade_distance_default=12.);wire(alpha,depth,'Opacity');prop(depth,unreal.MaterialProperty.MP_OPACITY)
 prop(node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.8,.75,.65,1)),unreal.MaterialProperty.MP_EMISSIVE_COLOR);finish(m)
 asset=unreal.load_asset('/Game/Subtitles/DA_LeaveStartRoom');lines=list(asset.lines);assert len(lines)==1
 lines[0].set_editor_property('text','亮晶晶的地方说明一定有矿石 赶紧挖挖看');asset.set_editor_property('lines',lines);assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 pp=next(a for a in aa if a.get_actor_label()=='COL_Heaven_LocalExposure');tags=list(pp.tags)
 if 'BM_HeavenInteriorExposure' not in [str(t) for t in tags]:tags.append('BM_HeavenInteriorExposure')
 pp.tags=tags
 field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 placements=list(field.authored_placements);p=next(x for x in placements if x.item_id==17)
 direction=unreal.MathLibrary.transform_direction(p.clue_transform,unreal.Vector(1,0,0));p.transform=unreal.Transform(location=p.clue_transform.translation+direction*400,rotation=p.transform.rotation.rotator(),scale=p.transform.scale3d)
 field.set_editor_property('authored_placements',placements,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 specpath=root/'Config/BranchCollectiblePlacements.json';specs=json.loads(specpath.read_text(encoding='utf8'));next(x for x in specs if x['id']==17)['position']=list(p.transform.translation.to_tuple());specpath.write_text(json.dumps(specs,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 assert levels.save_current_level();r.update(passed=True,fishbowl_depth_cm=400,fishbowl_position=list(p.transform.translation.to_tuple()),exposure_tag=str(pp.tags[-1]))
except: r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'author.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
