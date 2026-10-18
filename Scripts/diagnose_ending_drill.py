import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/PackagePlaytest'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary;s={'phase':0,'start':time.monotonic(),'next':0};report={}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def shot(name):assert helper.capture_preview(s['pc'],str(out/(name+'.png')))
def finish(error=None):
    s['done']=True;report['error']=error
    (out/'ending-drill-runtime.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
    cleanup={'at':time.monotonic()+3}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
    if s.get('done') or s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<240,'Diagnosis timeout'
        if now<s['next']:return
        if s['phase']==0:
            assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
            helper.start_floating_preview();s.update(phase=1,next=now+8);return
        w=ed.get_game_world()
        if not w:return
        if s['phase']==1:
            ds=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.EndingEmergence)
            if not ds or unreal.GameplayStatics.get_time_seconds(w)<5:return
            s['d']=ds[0];s['pc']=unreal.GameplayStatics.get_player_controller(w,0)
            s['d'].set_editor_property('film_speed',4);s['d'].preview_film();s['phase']=2;return
        if s['phase']==2:
            d=s['d']
            if d.shot!=9 or d.drill_distance<800:return
            d.set_actor_tick_enabled(False);s['pc'].get_controlled_pawn().get_component_by_class(unreal.MiningComponent).set_component_tick_enabled(False)
            s['camera']=s['pc'].get_view_target();cf=s['camera'].get_actor_transform()
            s['meshes']=[c for c in d.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and 'Drill' in c.static_mesh.get_name()]
            report['parts']=[dict(name=c.get_name(),mesh=c.static_mesh.get_path_name(),visible=c.is_visible(),hidden=c.get_editor_property('hidden_in_game'),transform=str(c.get_world_transform()),camera_local=str(unreal.MathLibrary.inverse_transform_location(cf,c.get_world_location())),relative=str(c.get_relative_transform()),parent=str(c.get_attach_parent().get_relative_transform())) for c in s['meshes']]
            report['camera']=str(cf);shot('drill-diagnose-original')
            for c in s['meshes']:
                c.set_hidden_in_game(False);c.set_visibility(True)
                for i in range(c.get_num_materials()):c.set_material(i,unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
            s.update(phase=3,next=now+2);return
        if s['phase']==3:
            shot('drill-diagnose-basic');finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
