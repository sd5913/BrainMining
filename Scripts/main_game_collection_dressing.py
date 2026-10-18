"""Local dressing for the owned Heaven branch; never edits source assets."""
import unreal

def dress_heaven(world,tag,frame):
    api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    aa=[a for a in api.get_all_level_actors() if a.actor_has_tag(tag)]
    pipe_material=unreal.load_asset('/Game/CollectionPrototype/HeavenToilet/Materials/M_HeavenPipe')
    repaired=0
    for a in aa:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            mesh=c.get_editor_property('static_mesh')
            if not mesh or 'pipe' not in mesh.get_path_name().lower():continue
            for slot,m in enumerate(c.get_materials()):
                if not m or m.get_name() in ['WorldGridMaterial','DefaultMaterial']:
                    c.set_material(slot,pipe_material);repaired+=1
    pp=next((a for a in aa if a.get_actor_label()=='COL_Heaven_LocalExposure'),None)
    if not pp:
        pp=api.spawn_actor_from_class(unreal.PostProcessVolume,unreal.MathLibrary.transform_location(frame,unreal.Vector(2580,0,650)))
        pp.set_actor_label('COL_Heaven_LocalExposure');pp.set_editor_property('tags',[tag,'BM_Campaign_MainGame03']);pp.set_folder_path('MainGame/Collections')
    pp.set_actor_scale3d(unreal.Vector(14,14,8))
    for k,v in [('unbound',False),('enabled',True),('priority',50.),('blend_radius',120.),('blend_weight',1.)]:pp.set_editor_property(k,v)
    settings=pp.get_editor_property('settings')
    for k,v in [('auto_exposure_method',unreal.AutoExposureMethod.AEM_HISTOGRAM),('auto_exposure_min_brightness',4.),('auto_exposure_max_brightness',4.),('auto_exposure_bias',0.),('auto_exposure_apply_physical_camera_exposure',False),('auto_exposure_speed_up',8.),('auto_exposure_speed_down',8.),('bloom_intensity',.25),('reflection_method',unreal.ReflectionMethod.SCREEN_SPACE),('screen_space_reflection_quality',75.)]:
        settings.set_editor_property('override_'+k,True);settings.set_editor_property(k,v)
    pp.set_editor_property('settings',settings)
    center,extent=pp.get_actor_bounds(False)
    assert extent.x>=1300 and extent.y>=1300 and extent.z>=700,'Local exposure volume needs a real bounded brush'
    return {'repaired_pipe_slots':repaired,'exposure_bounds':[list(center.to_tuple()),list(extent.to_tuple())]}
