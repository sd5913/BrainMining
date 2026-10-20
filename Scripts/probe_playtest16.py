from pathlib import Path
root=Path(unreal.Paths.project_dir()) if 'unreal' in globals() else Path(__file__).resolve().parents[1]
source=(root/'Scripts/capture_feedback11_main.py').read_text(encoding='utf8')
head=source[:source.index('def run():')].replace("Saved/Validation/Feedback11/Main","Saved/Validation/Playtest16/Probe")
run='''def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 r['particles']=[dict(label=a.get_actor_label(),cls=a.get_class().get_name(),pos=str(a.get_actor_location()),components=[dict(name=c.get_name(),asset=str(c.get_asset())) for c in a.get_components_by_class(unreal.NiagaraComponent)]) for a in aa if -13000<a.get_actor_location().x<-3000 and (isinstance(a,unreal.MiningCavernMotes) or a.get_components_by_class(unreal.NiagaraComponent))]
 r['pear_bounds']=str(unreal.load_asset('/Game/Countryside_Interior/Meshes/SM_Food_SM_Pear').get_bounds())
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();pc=unreal.GameplayStatics.get_player_controller(w,0);s['pc']=pc
 yield 8
 H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.get_editor_property('bFinished'):yield .2
 pose(unreal.Vector(-34430,70,11255),unreal.Vector(-34550,292,11200));yield 3
 H.capture_preview(pc,str(OUT/'cabinet.png'))
 pose(unreal.Vector(-34480,-30,11255),unreal.Vector(-34612,-123,11209));yield 2
 H.capture_preview(pc,str(OUT/'plant.png'))
 H.prepare_collection_preview_area(pc,2,False);yield 2
 pose(unreal.Vector(-16600,0,6550),unreal.Vector(-15600,0,6545));yield 2
 H.capture_preview(pc,str(OUT/'office_entry.png'))
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0]
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGame03')
 H.prepare_collection_preview_area(pc,index,False);hall.set_actor_tick_enabled(False);yield 2
 pose(unreal.Vector(33600,11750,20080),unreal.Vector(34500,11750,19800));yield 2
 H.capture_preview(pc,str(OUT/'third_start.png'))
 r['third_rays']=[]
 for z in [19850,19950,20050,20150]:
  h=unreal.SystemLibrary.line_trace_single(w,unreal.Vector(33450,11750,z),unreal.Vector(35100,11750,z-400),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[pc.get_controlled_pawn()],unreal.DrawDebugTrace.NONE,True).to_tuple()
  r['third_rays'].append(dict(hit=str(h),actor=h[9].get_path_name() if h[9] else None))
 r['passed']=True;finish()
'''
exec(compile(head+run+source[source.index('def finish('):],'probe_playtest16_runtime','exec'))
