"""Bake playtest fixes into source maps, retaining backup and repeat-run guards."""
import unreal,sys,json,traceback,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
from gameplay_feedback_authoring import *
from main_game_room_entrances_geometry import cross,assert_closed
OUT=ROOT/'Saved/Validation/GameplayFeedback'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report_path=OUT/'authoring.json'
report=json.loads(report_path.read_text(encoding='utf-8')) if report_path.exists() else {'platforms':[],'walls':[]}
report['passed']=False;report.pop('error',None)
try:
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    assert levels.set_current_level_by_name('Lvl_MainGame')
    aa=list(actors.get_all_level_actors());hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall))
    if not hall.actor_has_tag('BM_GameplayFeedback_20261018'):
        report['platforms']=[]
        gate=next(a for a in aa if a.get_name()=='GeneratedMiningRock_6')
        before=bounds(gate);cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'))
        spec=cfg['generated_connections'][1];cx=-13760.;verts=[];faces=[];n=12
        # Bake the existing runtime relocation and fit the plug to the collar there.
        # Its old translated shape left a gap above the sloping floor.
        for side in [-1,1]:
            ring=cross(spec,cx+side*45);cy=sum(p[1] for p in ring)/n;cz=sum(p[2] for p in ring)/n
            verts.extend([(p[0],cy+(p[1]-cy)*1.08,cz+(p[2]-cz)*1.08) for p in ring])
        for side in range(2):verts.append(tuple(sum(p[i] for p in verts[side*n:(side+1)*n])/n for i in range(3)))
        for j in range(n):
            k=(j+1)%n;faces.extend([(j,k,n+k),(j,n+k,n+j),(2*n,k,j),(2*n+1,n+j,n+k)])
        verts,faces=assert_closed(verts,faces)
        verts=[unreal.MathLibrary.inverse_transform_location(gate.get_actor_transform(),V(p)).to_tuple() for p in verts]
        mesh=mesh_from_buffers(verts,faces);assert lib.set_campaign_rock_source(gate,mesh)
        material(gate,unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_BlastOnlyBasalt'))
        lo,hi=bounds(gate);setp(gate,'rock_size',hi-lo)
        setp(gate,'tags',list(gate.tags)+['BM_Feedback_ThinBlastLayer'])
        report['bomb_layer']={'old_bounds':[list(p.to_tuple()) for p in before],'bounds':[list(lo.to_tuple()),list(hi.to_tuple())],'thickness':hi.x-lo.x}
        mat=unreal.load_asset('/Game/MainGame/Materials/RoutesV5/M_ParkourBasalt')
        features=json.loads((ROOT/'Art/MainGame/RoutesV6/TunnelCavernV2/features.json').read_text(encoding='utf-8'))['platforms']
        for i,f in enumerate(features):
            a=next(a for a in aa if a.actor_has_tag('BM_ParkourPlatform') and a.get_actor_label().endswith(f['name']))
            p=f['top_m'];top=V([-7700+p[0]*100,-p[1]*100,400+p[2]*100]);base=V([top.x,top.y,5110 if i<5 else 7510])
            report['platforms'].append(platform(a,base,top,175,631+i,mat,'SM_RockMushroom_%02d'%(i+1)))
        for i,rockname in enumerate(['GeneratedMiningRock_25','GeneratedMiningRock_26']):
            cap=next(a for a in aa if a.get_name()==rockname);lo,hi=bounds(cap)
            a=next(a for a in aa if a.get_actor_label()=='MAIN_GAME_Parkour_DigPlatform_%d'%(i+1))
            top=V([(lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z-450])
            if i==1:top.z-=50
            base=V([top.x,top.y,5110 if i==0 else 7510])
            report['platforms'].append(platform(a,base,top,140,751+i,mat,'SM_RockMushroom_Dig_%d'%(i+1)))
        soil=next(a for a in aa if a.actor_has_tag('BM_MuseumDownwardSoil'));lo,hi=bounds(soil);factor=900/(hi.z-lo.z)
        warp=lambda p:V([p.x,p.y,lo.z+(p.z-lo.z)*factor])
        deform(soil,warp);setp(soil,'rock_size',V([hi.x-lo.x,hi.y-lo.y,900]))
        field=next(a for a in aa if isinstance(a,unreal.MiningTreasureField));placements=list(field.get_editor_property('authored_placements'));count=0
        for p in placements:
            if soil not in p.hosts:continue
            for name in ['transform','surface']:
                t=getattr(p,name);t.translation=warp(t.translation);p.set_editor_property(name,t)
            fragments=list(p.fragments)
            for t in fragments:t.translation=warp(t.translation)
            p.set_editor_property('fragments',fragments);p.set_editor_property('fragment_supports',[warp(v) for v in p.fragment_supports]);count+=1
        setp(field,'authored_placements',placements)
        report['museum_soil']={'bottom':lo.z,'old_depth':hi.z-lo.z,'new_depth':900,'deposits_moved':count}
        setp(hall,'tags',list(hall.tags)+['BM_MainGame_HardPlugAdjusted','BM_MainGame_ParkourAdjusted','BM_GameplayFeedback_20261018'])
        assert levels.save_current_level()
    else:report['main_already_applied']=True
    assert levels.load_level('/Game/PopupClose/Maps/Lvl_PopupClose1')
    aa=list(actors.get_all_level_actors());meshes={}
    for a in aa:
        if not isinstance(a,unreal.StaticMeshActor):continue
        c=a.static_mesh_component;model=c.static_mesh
        if not model or not ('/Office/Meshes/Modular/SM_Wall' in model.get_path_name() or '/PopupClose/Collision/SM_Wall' in model.get_path_name()):continue
        if a.actor_has_tag('BM_Feedback_WallCollision'):continue
        source=model.get_path_name().split('.')[0]
        if source not in meshes:
            path='/Game/PopupClose/Collision/'+model.get_name()
            collision=unreal.load_asset(path) if assets.does_asset_exist(path) else assets.duplicate_asset(source,path)
            body=collision.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);body.set_editor_property('double_sided_geometry',True)
            assert assets.save_loaded_asset(collision);meshes[source]=collision
        c.set_static_mesh(meshes[source]);c.set_collision_profile_name('BlockAll');a.set_actor_enable_collision(True)
        center,extent=a.get_actor_bounds(False);lo=center-extent;hi=center+extent
        # The two west panels and east middle panel previously overlapped the
        # real doorways with no collision. Preserve a 220 x 290 cm clear opening.
        trim=None
        if abs(abs(center.x)-1005)<40 and lo.y<110 and hi.y>-110:
            low=lo.to_tuple();high=list(hi.to_tuple());low=list(low)
            if center.y<-110:high[1]=-110
            elif center.y>110:low[1]=110
            else:low[2]=290
            desiredlo=V(low);desiredhi=V(high);newextent=desiredhi-desiredlo;oldextent=hi-lo
            # These walls are aligned with world X/Y (yaw +/-90 degrees).
            scale=a.get_actor_scale3d();scale.y*=newextent.y/oldextent.y;scale.z*=newextent.z/oldextent.z;a.set_actor_scale3d(scale)
            current,ext=a.get_actor_bounds(False);a.set_actor_location(a.get_actor_location()+(desiredlo+desiredhi)*.5-current,False,True)
            trim=[low,high]
        setp(a,'tags',list(a.tags)+['BM_Feedback_WallCollision'])
        report['walls'].append({'actor':a.get_name(),'label':a.get_actor_label(),'door_trim':trim})
    assert levels.save_current_level();report['passed']=True
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('GAMEPLAY_FEEDBACK_AUTHORING '+json.dumps(report,ensure_ascii=False));unreal.SystemLibrary.quit_editor()
