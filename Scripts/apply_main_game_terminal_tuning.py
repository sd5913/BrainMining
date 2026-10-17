"""Bake terminal thickness without nonuniform actor scale."""
import unreal, json, shutil, traceback
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();out=ROOT/'Saved/Validation/MainGameTerminalTuning';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
assert levels.set_current_level_by_name('Lvl_MainGame')
all_actors=list(actors.get_all_level_actors());hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall))
assert not hall.actor_has_tag('BM_MainGame_TerminalFeedback'),'Already applied'
backup=ROOT/'Saved/Backups'/('MainGame_Terminals_'+datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
lib=unreal.MiningDesignerLibrary;NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER
report={'backup':str(backup),'layers':[],'platforms':[],'passed':False}
def setp(o,k,v):o.set_editor_property(k,v,notify_mode=NEVER)
def positions(actor,src):
    pts=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(src,False)[1])
    return [unreal.MathLibrary.transform_location(actor.get_actor_transform(),p) for p in pts]
def warp(p,bottom,factor):return unreal.Vector(p.x,p.y,bottom+(p.z-bottom)*factor)
def deform(actor,bottom,factor):
    src=lib.get_campaign_rock_source(actor);assert src
    pts=positions(actor,src)
    for i,p in enumerate(pts):
        q=unreal.MathLibrary.inverse_transform_location(actor.get_actor_transform(),warp(p,bottom,factor))
        _,ok=unreal.GeometryScript_MeshEdits.set_vertex_position(src,i,q,True);assert ok
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(src)
    unreal.GeometryScript_Normals.recompute_normals(src,unreal.GeometryScriptCalculateNormalsOptions())
    assert lib.set_campaign_rock_source(actor,src),actor.get_name()
    assert all(abs(v-1)<.001 for v in actor.get_actor_scale3d().to_tuple())
try:
    field=next(a for a in all_actors if isinstance(a,unreal.MiningTreasureField))
    placements=list(field.get_editor_property('authored_placements'))
    for number,name,clod in [(1,'GeneratedMiningRock_25','GeneratedMiningRock_46'),(2,'GeneratedMiningRock_26','GeneratedMiningRock_47'),(3,'GeneratedMiningRock_57',None)]:
        actor=next(a for a in all_actors if a.get_name()==name)
        pts=positions(actor,lib.get_campaign_rock_source(actor));bottom=min(p.z for p in pts);top=max(p.z for p in pts)
        thickness=90. if number<3 else 650.;factor=thickness/(top-bottom)
        deform(actor,bottom,factor);partners=[actor]
        if clod:
            extra=next(a for a in all_actors if a.get_name()==clod);deform(extra,bottom,factor);partners.append(extra)
        adjusted=0
        for p in placements:
            if not any(h in partners for h in p.hosts):continue
            t=p.transform;t.translation=warp(t.translation,bottom,factor);p.set_editor_property('transform',t)
            t=p.surface;t.translation=warp(t.translation,bottom,factor);p.set_editor_property('surface',t)
            fragments=list(p.fragments)
            for t in fragments:t.translation=warp(t.translation,bottom,factor)
            p.set_editor_property('fragments',fragments)
            p.set_editor_property('fragment_supports',[warp(v,bottom,factor) for v in p.fragment_supports]);adjusted+=1
        setp(actor,'tags',list(actor.tags)+['BM_MainGame_ThinParkourCap' if number<3 else 'BM_MainGame_ThickMuseumSoil'])
        report['layers'].append({'actor':name,'bottom':bottom,'old_thickness':top-bottom,'thickness':thickness,'deposits_moved':adjusted})
        if number<3:
            cx=(min(p.x for p in pts)+max(p.x for p in pts))/2;cy=(min(p.y for p in pts)+max(p.y for p in pts))/2;loc=unreal.Vector(cx,cy,bottom-255.)
            pad=actors.spawn_actor_from_class(unreal.StaticMeshActor,loc);pad.set_actor_label('MAIN_GAME_Parkour_DigPlatform_%d'%number)
            setp(pad,'tags',['BM_Campaign_MainGame02','BM_MainGame_ParkourTerminalPlatform'])
            pad.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));pad.set_actor_scale3d(unreal.Vector(4.8,4.8,.5))
            pad.static_mesh_component.set_material(0,unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_ParkourBasalt'));pad.static_mesh_component.set_collision_profile_name('BlockAll')
            report['platforms'].append({'name':pad.get_name(),'position':list(loc.to_tuple()),'headroom':230.})
    setp(field,'authored_placements',placements);setp(hall,'tags',list(hall.tags)+['BM_MainGame_TerminalFeedback'])
    assert levels.save_current_level();report['passed']=True
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (out/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MAIN_GAME_TERMINALS '+json.dumps(report,ensure_ascii=False))
