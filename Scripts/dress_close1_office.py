"""Dress Close1 only. Preserve actors with gameplay and hand-placed user props."""
import unreal,math,json,pathlib
root=pathlib.Path(unreal.Paths.project_dir());folder='/Game/PopupClose/OfficeArt';actor_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=editor.get_editor_world();assert world.get_path_name().startswith('/Game/PopupClose/Maps/Lvl_PopupClose1.')
actors=actor_api.get_all_level_actors();stage=next(a for a in actors if a.get_class().get_name()=='PopupCloseStage');office=next(a for a in actors if a.get_class().get_name()=='PopupCloseOffice');devices=[a for a in actors if a.get_class().get_name()=='PopupOfficeDevice'];assert len(devices)==4
mat=lambda n:unreal.load_asset(folder+'/Materials/M_BR_'+n)
mesh=lambda n:unreal.load_asset(folder+'/Meshes/SM_'+n)
office.set_editor_property('replace_perimeter_workstations',True);office.set_editor_property('carpet_material',mat('Carpet'));office.set_editor_property('ceiling_tile_material',mat('Ceiling'))
office.set_editor_property('surface_overrides',[mat('Wall'),mat('Trim'),mat('Wood'),mat('Trim'),mat('Window'),mat('Paper'),mat('Trim'),mat('Blind'),mat('Trim'),mat('Blind'),mat('Trim'),mat('Trim'),mat('WarmPanel')])
stage.set_editor_property('interior_lighting',True);stage.set_editor_property('interior_fill_intensity',120.)
stage.set_editor_property('replace_central_desk',True)
for prop,name in [('floor_art_material','Carpet'),('wall_art_material','Wall'),('ceiling_art_material','Ceiling'),('desktop_art_material','Wood')]:stage.set_editor_property(prop,mat(name))
for c in office.get_components_by_class(unreal.RectLightComponent):
 c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
 name=c.get_name();c.set_intensity(1500 if name not in ['OfficeLight02','OfficeLight20'] else 800);c.set_light_color(unreal.LinearColor(1,1,1,1));c.set_editor_property('use_temperature',True);c.set_editor_property('temperature',4700.);c.set_cast_shadows(True)
for c in stage.get_components_by_class(unreal.PointLightComponent):c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
for d in devices:
 d.set_editor_property('model_material_overrides',[mat('Trim'),mat('Blind'),mat('Wood'),mat('Window'),mat('Paper')])
 if d.get_editor_property('device_index')>=2:
  d.set_editor_property('replacement_model',mesh('Copier'))
  d.set_editor_property('replacement_transform',unreal.Transform(location=unreal.Vector(0,0,0),rotation=unreal.Rotator(pitch=0,yaw=90,roll=0),scale=unreal.Vector(1.12,1.12,1.12)))
  # Measured from printer1's top side-tray horizontal surface at 0.948 m, scaled by 1.12.
  d.set_editor_property('paper_output_position',unreal.Vector(6,-36,106.45))
  d.set_editor_property('paper_output_direction',unreal.Vector(0,-1,0));d.set_editor_property('paper_output_yaw',90.)
 else:
  d.set_editor_property('replacement_model',mesh('DeskMonitor'))
  d.set_editor_property('replacement_transform',unreal.Transform(location=unreal.Vector(50,0,75),rotation=unreal.Rotator(pitch=0,yaw=-90,roll=14),scale=unreal.Vector(1.25,1.25,1.25)))
  surface=next(c for c in d.get_components_by_class(unreal.WidgetComponent) if c.get_name()=='DesktopSurface');surface.set_relative_location(unreal.Vector(39.5,0,99.65),False,False)
  surface.set_tint_color_and_opacity(unreal.LinearColor(7,7,7,1))
office.refresh_office_art();stage.refresh_office_art()
for d in devices:d.refresh_office_art()
created=[]
def place(n,label,p,yaw=0,scale=1,collision=False,asset=None):
 existing=next((a for a in actor_api.get_all_level_actors() if a.actor_has_tag('OfficeArtClose1') and a.get_actor_label()==label),None)
 a=existing or actor_api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p));c=a.static_mesh_component;c.set_static_mesh(asset or mesh(n));c.set_mobility(unreal.ComponentMobility.STATIC)
 a.set_actor_label(label);a.set_folder_path('PopupOffice/OfficeArt/'+('Furniture' if collision else 'DesktopProps'));a.set_editor_property('tags',['OfficeArtClose1']);a.set_actor_location(unreal.Vector(*p),False,True);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True);a.set_actor_scale3d(unreal.Vector(scale,scale,scale));c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');created.append(a);return a
def local(p,yaw,off):
 rad=math.radians(yaw);return [p[0]+math.cos(rad)*off[0]-math.sin(rad)*off[1],p[1]+math.sin(rad)*off[0]+math.cos(rad)*off[1],p[2]+off[2]]
# Six modest perimeter workstations, rather than filling all eight original positions.
for i,(x,y) in enumerate([(-640,-550),(0,-570),(640,-550),(-640,550),(0,570),(640,550)]):
 desk_yaw=0 if y<0 else 180
 # Blender export flips Y, so the front of these desks faces +Y in UE.
 cubicle=i not in [1,4];s=.92 if cubicle else .83;p=[x,y,0]
 place('CubicleDesk' if cubicle else 'OfficeDesk',f'工位_{i+1:02d} · 旧办公桌',p,desk_yaw,s,True)
 height=78*s if cubicle else 91.24*s
 front=1 if y<0 else -1
 place('DeskChair',f'工位_{i+1:02d} · 空办公椅',[x+(-10 if i%2 else 12),y+front*120,0],desk_yaw+(6 if i%2 else -9),.84,True)
 def prop(n,off,yaw=0):return place(n,f'工位_{i+1:02d} · {n}',local(p,desk_yaw,[*off[:2],height+off[2]]),desk_yaw+yaw)
 monitor=prop('DeskMonitor',[0,-24,.4],180);monitor.set_editor_property('tags',['OfficeArtClose1','OfficePopupEmitter'])
 prop('Keyboard',[0,15,.4],180);prop('Mouse',[25,12,.4],180)
 prop('Paper',[-25,10,.3],-7 if i%2 else 12)
 if i in [0,3,5]:prop('PaperTray',[35,-20,.4],0)
 elif i in [1,4]:prop('Folder',[33,-15,.4],-4)
 else:prop('Notebook',[34,-10,.4],6)
 if i in [0,2,4]:prop('CoffeeCup',[-44,-17,.4])
 if i in [0,3]:prop('PenCup',[-39,-27,.4])
 if i in [1,5]:prop('Calculator',[-28,-18,.4],9)
 # Use selected existing assets for a clipboard, without moving the user's original notebook.
 if i==2:place('',f'工位_{i+1:02d} · 剪贴板',local(p,desk_yaw,[-28,-23,height+.5]),desk_yaw-8,1,False,unreal.load_asset('/Game/Fab/Document_Clipboard_with_Pen/document_clipboard_with_pen/StaticMeshes/document_clipboard_with_pen'))
# A small file cabinet near the room service area, leaving device approaches and exits open.
place('FilingCabinet','档案柜 · 侧墙',[880,570,0],90,1.4,True)
# Replace the visible blockout furniture with the supplied detailed meshes.
place('OfficeDesk','中央工位 · 实木办公桌',[0,0,0],0,.81,True)
place('Keyboard','中央工位 · 键盘',[-13,0,74.5],90,1)
place('Mouse','中央工位 · 鼠标',[-13,27,74.5],90,1)
place('CoffeeCup','中央工位 · 杯子',[11,21,74.5],0,1)
place('PenCup','中央工位 · 笔筒',[-37,25,74.5],0,1)
place('OfficeDesk','西北交互工位 · 办公桌',[-450,300,0],180,.81,True)
place('Keyboard','西北交互工位 · 键盘',[-437,300,74.5],-90,1)
place('DeskChair','西北交互工位 · 办公椅',[-335,340,0],155,.84,True)
user_chair=next((a for a in actor_api.get_all_level_actors() if a.get_actor_label()=='office_chair'),None)
if user_chair:
 for a in actor_api.get_all_level_actors():
  if a.actor_has_tag('OfficeArtClose1') and a.get_actor_label()=='中央工位 · 办公椅':actor_api.destroy_actor(a)
 user_chair.set_actor_scale3d(unreal.Vector(.35,.35,.35));user_chair.set_actor_location(unreal.Vector(-112,98,0),False,True);user_chair.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-25,roll=0),True);user_chair.set_folder_path('PopupOffice/OfficeArt/Furniture')
else:place('DeskChair','中央工位 · 办公椅',[-112,98,0],-25,.84,True)
place('OfficeDesk','会议区 · 办公桌',[700,-170,0],0,.82,True)
for i,(x,y) in enumerate([(650,-267),(755,-267),(650,-73)]):place('DeskChair',f'会议区 · 办公椅_{i+1}',[x,y,0],0 if y<-170 else 180,.8,True)
place('PaperStack','会议区 · 文件',[650,-170,75.3],15)
place('CoffeeCup','会议区 · 杯子',[728,-168,75.3])
for i,x in enumerate([815,918]):place('FilingCabinet',f'档案区 · 抽屉柜_{i+1}',[x,-600,0],0,1.55,True)
place('Folder','档案区 · 文件夹',[815,-600,95.4],12)
# Keep the exit and center aisle visibly empty. Added meshes never absorb the E interaction trace.
pp=next((a for a in actor_api.get_all_level_actors() if a.get_actor_label()=='OfficeArt · Exposure'),None) or actor_api.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector(0,0,0))
pp.set_actor_label('OfficeArt · Exposure');pp.set_folder_path('PopupOffice/OfficeArt/Lighting');pp.set_editor_property('tags',['OfficeArtClose1']);pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
for k,v in [('override_auto_exposure_method',True),('auto_exposure_method',unreal.AutoExposureMethod.AEM_HISTOGRAM),('override_auto_exposure_min_brightness',True),('auto_exposure_min_brightness',4.),('override_auto_exposure_max_brightness',True),('auto_exposure_max_brightness',4.),('override_auto_exposure_bias',True),('auto_exposure_bias',0.),('override_vignette_intensity',True),('vignette_intensity',.22),('override_bloom_intensity',True),('bloom_intensity',.18)]:settings.set_editor_property(k,v)
pp.set_editor_property('settings',settings)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-850,-180,165),unreal.Rotator(pitch=-3,yaw=9,roll=0))
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/PopupClose/Maps/Lvl_PopupClose1')
(root/'Saved/OfficeArt/dressing.json').write_text(json.dumps({'actors':[a.get_actor_label() for a in created],'devices':[d.get_path_name() for d in devices]},ensure_ascii=False,indent=2),encoding='utf8');print('DRESSED_CLOSE1',len(created),'editable actors; four devices retained')
