"""Apply imported Officeasset architecture to Close1, retaining authored gameplay actors.

Run through the current editor's remote Python bridge after compiling the native
architecture switch and dynamic screen fitting. No source asset is edited.
"""
import unreal,math,pathlib,json

def dress_officeasset():
 root=pathlib.Path(unreal.Paths.project_dir());out=root/'Saved/OfficeAsset';out.mkdir(exist_ok=True)
 api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert world.get_path_name().startswith('/Game/PopupClose/Maps/Lvl_PopupClose1.')
 actors=api.get_all_level_actors();office=next(a for a in actors if a.get_class().get_name()=='PopupCloseOffice');stage=next(a for a in actors if a.get_class().get_name()=='PopupCloseStage');devices=[a for a in actors if a.get_class().get_name()=='PopupOfficeDevice'];assert len(devices)==4
 asset=lambda p:unreal.load_asset('/Game/Office/'+p)
 material=lambda n:asset('Materials/'+n)
 created=[]
 def place(label,n,p,yaw=0,scale=(1,1,1),mats=None,collision=False,mesh_asset=None):
  a=next((a for a in api.get_all_level_actors() if a.actor_has_tag('OfficeAssetClose1') and a.get_actor_label()==label),None) or api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p))
  a.set_actor_label(label);a.set_folder_path('PopupOffice/OfficeAsset/Architecture');a.set_editor_property('tags',['OfficeAssetClose1'])
  c=a.static_mesh_component;c.set_static_mesh(mesh_asset or asset('Meshes/'+n));c.set_mobility(unreal.ComponentMobility.STATIC);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
  if mats:
   for i,m in enumerate(mats):c.set_material(i,m)
  a.set_actor_location(unreal.Vector(*p),False,True);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True);a.set_actor_scale3d(unreal.Vector(*scale));created.append(a);return a
 wall=material('Mi_Wall_01a');carpet=material('Mi_Carpet_02');ceiling=material('Mi_Ceiling');trim=material('Mi_WoodCoving')
 # The imported glass references a Megascans roughness map that was not migrated.
 # Duplicate its parent, and neutralize only that absent texture dependency.
 art='/Game/PopupClose/OfficeArt/Architecture';unreal.EditorAssetLibrary.make_directory(art)
 glass_parent=unreal.load_asset(art+'/M_OfficeGlass') or unreal.EditorAssetLibrary.duplicate_asset('/Game/Office/Materials/Master/M_Glass',art+'/M_OfficeGlass')
 for e in unreal.ObjectIterator(unreal.MaterialExpressionTextureSample):
  if e.get_path_name().startswith(glass_parent.get_path_name()+':') and not e.get_editor_property('texture'):
   e.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
 unreal.MaterialEditingLibrary.recompile_material(glass_parent)
 glass=unreal.load_asset(art+'/MI_OfficeGlass') or unreal.EditorAssetLibrary.duplicate_asset('/Game/Office/Materials/Master/Mi_Glass',art+'/MI_OfficeGlass')
 unreal.MaterialEditingLibrary.set_material_instance_parent(glass,glass_parent)
 # Mesh default slots are also cook dependencies, even when actor overrides exist.
 frame=unreal.load_asset(art+'/SM_OfficeWindowFrame') or unreal.EditorAssetLibrary.duplicate_asset('/Game/Office/Meshes/Modular/SM_DoubleWindowFrame_300b',art+'/SM_OfficeWindowFrame')
 slots=list(frame.static_materials);slots[0].material_interface=trim;slots[1].material_interface=glass;frame.set_editor_property('static_materials',slots)
 # Render the live desktop onto the original curved screen's 0..1 UVs.
 screen=unreal.load_asset(art+'/M_CRTScreen')
 if not screen:
  screen=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CRTScreen',art,unreal.Material,unreal.MaterialFactoryNew())
  screen.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 texture=next((e for e in unreal.ObjectIterator(unreal.MaterialExpressionTextureSampleParameter2D) if e.get_outer()==screen),None)
 if not texture:
  texture=unreal.MaterialEditingLibrary.create_material_expression(screen,unreal.MaterialExpressionTextureSampleParameter2D);texture.set_editor_property('parameter_name','DesktopTexture');texture.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
  gain=unreal.MaterialEditingLibrary.create_material_expression(screen,unreal.MaterialExpressionMultiply);strength=unreal.MaterialEditingLibrary.create_material_expression(screen,unreal.MaterialExpressionConstant);strength.set_editor_property('r',1.35);unreal.MaterialEditingLibrary.connect_material_expressions(texture,'RGB',gain,'A');unreal.MaterialEditingLibrary.connect_material_expressions(strength,'',gain,'B');unreal.MaterialEditingLibrary.connect_material_property(gain,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 unreal.MaterialEditingLibrary.recompile_material(screen)
 for e in unreal.ObjectIterator(unreal.MaterialExpressionConstant):
  if e.get_outer()==screen:e.set_editor_property('r',8.)
 unreal.MaterialEditingLibrary.recompile_material(screen)
 office.set_editor_property('use_imported_architecture',True)
 overrides=list(office.get_editor_property('surface_overrides'));overrides[0]=wall;overrides[1]=wall;overrides[7]=trim
 office.set_editor_property('surface_overrides',overrides);office.set_editor_property('carpet_material',carpet);office.set_editor_property('ceiling_tile_material',ceiling);office.refresh_office_art()
 for prop,mat in [('floor_art_material',carpet),('wall_art_material',wall),('ceiling_art_material',ceiling)]:stage.set_editor_property(prop,mat)
 stage.refresh_office_art()
 # The original structural slab and walls retain collision; modular meshes are visual skins.
 for c in stage.get_components_by_class(unreal.StaticMeshComponent):
  if c.get_name() in ['WallPositiveY','WallNegativeY']:c.set_visibility(False)
 # Eight broad window bays with varied partially lowered blinds, plus infill walls.
 centers=[-691.666667,-233.333333,233.333333,691.666667];width=350.;edges=[-990.]
 for cy in [-735.,735.]:
  side='南' if cy<0 else '北';yaw=90 if cy<0 else -90;segments=[];cursor=-990.
  for i,cx in enumerate(centers):
   lo=cx-width/2;hi=cx+width/2
   if lo>cursor:segments.append((cursor,lo))
   anchor=cx-width/2 if cy<0 else cx+width/2
   p=(anchor,cy,0);s=(1,width/300,1.2)
   place(f'{side}窗_{i+1:02d} · 墙体','Modular/SM_DoubleWindow_300b',p,yaw,s,[wall,wall])
   place(f'{side}窗_{i+1:02d} · 木窗框与玻璃','Modular/SM_DoubleWindowFrame_300b',p,yaw,s,[trim,glass],mesh_asset=frame)
   # Two correctly sized blinds per broad double window; keep the view intentionally occluded.
   for j in range(2):
    bw=175.;bx=cx-width/2+j*bw if cy<0 else cx+width/2-j*bw
    blind_name='SM_Blinds_01b' if (i+j+(cy>0))%3==0 else 'SM_Blinds_01a'
    place(f'{side}窗_{i+1:02d} · 百叶_{j+1}',blind_name,(bx,cy+(8 if cy<0 else -8),0),yaw,(1,bw/160,1.2),[material('Mi_Blinds')])
   cursor=hi
  if cursor<990:segments.append((cursor,990.))
  for i,(lo,hi) in enumerate(segments):place(f'{side}墙 · 窗间墙_{i+1:02d}','Modular/SM_Wall_300a',(lo if cy<0 else hi,cy,0),yaw,(1,(hi-lo)/300,1.2),[wall])
 # Doorways are kept open at both ends; no solid wall spans the existing exit.
 for sx in [-990.,990.]:
  yaw=0 if sx<0 else 180
  for band,(lo,hi) in enumerate([(-735.,-110.),(110.,735.)]):
   for i in range(3):
    start=lo+(hi-lo)*i/3;end=lo+(hi-lo)*(i+1)/3
    place(f'端墙_{int(sx)}_{band}_{i}','Modular/SM_Wall_300a',(sx,end if sx<0 else start,0),yaw,(1,(end-start)/300,1.2),[wall])
 # Preserve the player's floor at Z=0 and align one texture scale across real modular tiles.
 cols=7;rows=5;dx=1980/cols;dy=1470/rows
 for x in range(cols):
  for y in range(rows):
   lo=-990+x*dx;top=-735+(y+1)*dy
   place(f'地毯_{x:02d}_{y:02d}','Modular/SM_Floor_300a',(lo,top,.65),0,(dx/300,dy/300,1),[carpet])
   place(f'吊顶_{x:02d}_{y:02d}','Modular/SM_Ceiling_300a',(lo+dx,top,356),0,(dx/300,dy/300,1),[ceiling])
 for x in [-625.,0.,625.]:
  for y in [-416.666667,0.,416.666667]:place(f'荧光灯_{int(x)}_{int(y)}','SM_CeilingLight_01a',(x,y,355.8),90,(.72,.74,1),[material('Mi_CeilingLight_01a')])
 for i,(x,y) in enumerate([(-820,20),(830,-330),(-150,640)]):
  v=place(f'吊顶通风口_{i+1}','SM_CeilingVent_01a',(x,y,352.6),0,(2,2,1),[material('Wall_Ceiling_Items')]);v.set_actor_rotation(unreal.Rotator(pitch=180,yaw=0,roll=0),True)
 # Pillars already placed by the user keep their locations and silhouettes.
 pillars=[]
 for a in actors:
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if c.static_mesh and c.static_mesh.get_name().startswith('SM_Pillar'):
    c.set_material(0,wall);pillars.append(a.get_actor_label())
 # The authored wall clock also used the missing original glass as a mesh default.
 clock_source=asset('Meshes/SM_Clock_01a');clock=unreal.load_asset(art+'/SM_OfficeClock') or unreal.EditorAssetLibrary.duplicate_asset('/Game/Office/Meshes/SM_Clock_01a',art+'/SM_OfficeClock')
 slots=list(clock.static_materials)
 for slot in slots:
  if slot.material_interface and slot.material_interface.get_name()=='M_Glass':slot.material_interface=glass_parent
 clock.set_editor_property('static_materials',slots)
 for a in actors:
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   if c.static_mesh==clock_source:
    old=[c.get_material(i) for i in range(c.get_num_materials())];c.set_static_mesh(clock)
    for i,m in enumerate(old):c.set_material(i,glass_parent if m and m.get_name()=='M_Glass' else m)
 # Transfer the user's central CRT placement into the real device, eliminating the duplicate.
 crt=asset('Meshes/SM_Computer_02a_CRT_CustomScreen')
 central=next(d for d in devices if d.get_editor_property('device_index')==0)
 sample=next((a for a in actors if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh==crt),None)
 saved_path=out/'central_crt_transform.json'
 if sample:
  p=sample.get_actor_location();r=sample.get_actor_rotation();s=sample.get_actor_scale3d();authored={'position':[p.x,p.y,p.z],'yaw':r.yaw,'scale':s.x};saved_path.write_text(json.dumps(authored,indent=2),encoding='utf8')
 else:
  assert central.get_editor_property('replacement_model')==crt,'Place the desired CRT or configure the central CRT first.'
  t=central.get_editor_property('replacement_transform');p=t.translation;r=t.rotation.rotator();s=t.scale3d;authored={'position':[p.x,p.y,p.z],'yaw':r.yaw,'scale':s.x}
 for d in devices:
  if d.get_editor_property('device_index')>=2:continue
  # Both working monitors face their existing player approach; use the user's monitor scale.
  s=authored['scale'];loc=unreal.Vector(*authored['position']);rotation=unreal.Rotator(pitch=0,yaw=authored['yaw'],roll=0);angle=math.radians(authored['yaw'])
  d.set_editor_property('replacement_model',crt);d.set_editor_property('replacement_transform',unreal.Transform(location=loc,rotation=rotation,scale=unreal.Vector(s,s,s)))
  d.set_editor_property('screen_projection_material',screen)
  surface=next(c for c in d.get_components_by_class(unreal.WidgetComponent) if c.get_name()=='DesktopSurface')
  surface.set_relative_location(unreal.Vector(loc.x+18.3*s*math.sin(angle),loc.y-18.3*s*math.cos(angle),loc.z+27.786*s),False,False)
  surface.set_relative_rotation(unreal.Rotator(pitch=0,yaw=authored['yaw']-90,roll=0),False,False)
  surface.set_relative_scale3d(unreal.Vector(.03,35.5*s/1280,26.5*s/800));surface.set_tint_color_and_opacity(unreal.LinearColor(3,3,3,1));d.refresh_office_art()
  shell=next(c for c in d.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OfficeArtShell');shell.set_material(0,material('Mi_Computer_01a'));shell.set_material(1,screen)
 if sample:api.destroy_actor(sample)
 # Replace six decorative displays, retaining their tags and their eruption source identities.
 computers=[]
 for a in actors:
  if not a.actor_has_tag('OfficePopupEmitter'):continue
  c=a.static_mesh_component;p=a.get_actor_location();c.set_static_mesh(asset('Meshes/SM_Computer_02a_CRT'));c.set_material(0,material('Mi_Computer_01a'))
  a.set_actor_scale3d(unreal.Vector(1,1,1));a.set_editor_property('tags',list(a.tags)+(['OfficeCRTPopupEmitter'] if not a.actor_has_tag('OfficeCRTPopupEmitter') else []));computers.append(a.get_actor_label())
 # Desk peripherals share the reference kit, without changing the actual work surfaces.
 for a in actors:
  if not isinstance(a,unreal.StaticMeshActor):continue
  c=a.static_mesh_component;m=c.static_mesh
  if not m:continue
  if m.get_name()=='SM_Keyboard':c.set_static_mesh(asset('Meshes/SM_Computer_02a_Keyboard'));c.set_material(0,material('Mi_Computer_01a'))
  elif m.get_name()=='SM_Mouse':c.set_static_mesh(asset('Meshes/SM_Computer_02a_Mouse'));c.set_material(0,material('Mi_Computer_01a'))
 unreal.EditorAssetLibrary.save_loaded_assets([glass_parent,glass,screen,frame,clock])
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/PopupClose/Maps/Lvl_PopupClose1')
 report={'architecture_actors':len(created),'working_computers':2,'decorative_computers':computers,'pillars':pillars,'authored_crt':authored,'unchanged_printers':2}
 (out/'dressing.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('OFFICEASSET_DRESSED',json.dumps(report,ensure_ascii=False))

dress_officeasset()
