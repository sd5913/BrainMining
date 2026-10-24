"""Read current authoring state for the 26 player feedback fixes."""
import unreal,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'Validation/PlayerFeedback26';out.mkdir(parents=True,exist_ok=True)
r={};api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def v(x):return [x.x,x.y,x.z]
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(api.get_all_level_actors());field=next(a for a in aa if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 r['placements']=[dict(id=p.item_id,pos=v(p.transform.translation),clue=v(p.clue_transform.translation),host=p.host_rock.get_name() if p.host_rock else '',hostlabel=p.host_rock.get_actor_label() if p.host_rock else '',bounds=[v(x) for x in p.host_rock.get_actor_bounds(False)] if p.host_rock else []) for p in field.authored_placements]
 r['actors']=[]
 for a in aa:
  if any(x in a.get_actor_label() for x in ['Heaven','BombStencil','BombOnly','Hopper','Contact','Circuit','PlugSupply','Subtitle','字幕']):
   r['actors'].append(dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),pos=v(a.get_actor_location()),bounds=[v(x) for x in a.get_actor_bounds(False)],tags=[str(x) for x in a.tags],level=a.get_level().get_path_name()))
 r['subtitles']=[]
 for path in unreal.EditorAssetLibrary.list_assets('/Game/Subtitles',True,False):
  if path.rsplit('/',1)[-1].startswith('DA_'):
   a=unreal.load_asset(path)
   if isinstance(a,unreal.SubtitleSegment):r['subtitles'].append(dict(path=path,id=str(a.segment_id),lines=[str(x.text) for x in a.lines]))
except: r['error']=traceback.format_exc()
(out/'authoring-before.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
