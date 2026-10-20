"""Observe an actual supported feather release, land, fade and disappear in PIE."""
import unreal,time,json,traceback
from pathlib import Path
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out=Path(unreal.Paths.project_dir())/'Saved/Validation/TraversalFeedback'
s={'phase':0,'start':time.monotonic(),'next':0,'landed':False,'faded':False};checks=[]
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=settings.get_editor_property('bThrottleCPUWhenNotForeground');settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
def end(error=None):
    (out/'feather.json').write_text(json.dumps({'passed':not error,'checks':checks,'error':error},indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    if E.get_game_world():L.editor_request_end_play()
    settings.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def step(dt):
    try:
        now=time.monotonic()
        if now<s['next']:return
        assert now-s['start']<80,'Feather test timeout'
        if s['phase']==0:
            assert L.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V03')
            unreal.MiningGUIEditorLibrary.start_floating_preview();s.update(phase=1,next=now+5);return
        w=E.get_game_world()
        if not w:return
        if s['phase']==1:
            feathers=unreal.GameplayStatics.get_all_actors_with_tag(w,'CollectionFeather')
            if len(feathers)<7:return
            feather=feathers[0];p=feather.get_actor_location()
            field=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            bird=next(i for i in field.items if i.item_id==3)
            rock=min(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock),key=lambda r:abs(r.get_actor_location().y-bird.get_actor_location().y))
            assert rock.mine_sphere(p+unreal.Vector(3,0,0),50)
            checks.append('Feather support removed through real mining')
            s.update(phase=2,feather=feather,neighbor=feathers[1],cut_at=now);return
        feather=s['feather']
        if not unreal.SystemLibrary.is_valid(feather):
            assert s['landed'] and s['faded'],'Feather must visibly fade after landing'
            assert unreal.SystemLibrary.is_valid(s['neighbor']),'Supported adjacent clue remains'
            checks.extend(['Released feather lands on floor','Landed feather opacity decreases','Fallen feather disappears; supported neighbor remains'])
            end();return
        if feather.actor_has_tag('FeatherLanded'):
            s['landed']=True
            for part in feather.get_components_by_class(unreal.MeshComponent):
                m=part.get_material(0)
                if isinstance(m,unreal.MaterialInstanceDynamic) and .01<m.get_scalar_parameter_value('Opacity')<.95:s['faded']=True
    except Exception:end(traceback.format_exc())
busy=False
def tick(dt):
    global busy
    if busy:return
    busy=True
    try:step(dt)
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
