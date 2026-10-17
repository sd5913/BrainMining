"""P2-only Countryside art pass. Original gameplay/collision actors are retained in place."""
import unreal, math, json, re, random, hashlib
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
E=unreal.EditorAssetLibrary
ML=unreal.MaterialEditingLibrary
AT=unreal.AssetToolsHelpers.get_asset_tools()
ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/Automation/CountrysideP2'
BASE='/Game/Passages/Echo/ArtP2'
TAG=unreal.Name('CountrysideP2Art')
assert L.get_current_level().get_outer().get_name()=='Lvl_EchoMazeToCards_P2'
actors=list(A.get_all_level_actors())
before=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
beforeby={r['name']:r for r in before}
for a in actors:
 if TAG in a.tags and isinstance(a,unreal.StaticMeshActor):A.destroy_actor(a)
actors=list(A.get_all_level_actors())
hidden=[];added=[];retextured=[]
def xyz(v):return [round(v.x,3),round(v.y,3),round(v.z,3)]
def expr(m,cls):return ML.create_material_expression(m,cls)
def scalar(m,val):
 n=expr(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',val);return n
def rgb(m,c):
 n=expr(m,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*c,1));return n
def material(name,color,rough=.7,texture=None,tile=1,world=False,normal=None,code=None):
 path=BASE+'/Materials/'+name;m=unreal.load_asset(path)
 if not m:m=AT.create_asset(name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(m)
 color_node=rgb(m,color)
 uv=expr(m,unreal.MaterialExpressionTextureCoordinate)
 uv.set_editor_property('u_tiling',tile);uv.set_editor_property('v_tiling',tile)
 if world:
  pos=expr(m,unreal.MaterialExpressionWorldPosition);custom=expr(m,unreal.MaterialExpressionCustom)
  i=unreal.CustomInput();i.set_editor_property('input_name','P');custom.set_editor_property('inputs',[i]);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT2)
  custom.set_editor_property('code',world);ML.connect_material_expressions(pos,'',custom,'P');uv=custom
 if texture:
  sample=expr(m,unreal.MaterialExpressionTextureSample);sample.set_editor_property('texture',unreal.load_asset('/Game/Countryside_Interior/Textures/'+texture))
  ML.connect_material_expressions(uv,'',sample,'Coordinates');mult=expr(m,unreal.MaterialExpressionMultiply)
  ML.connect_material_expressions(sample,'RGB',mult,'A');ML.connect_material_expressions(color_node,'',mult,'B');color_node=mult
 if code:
  custom=expr(m,unreal.MaterialExpressionCustom);ins=[]
  for name_in in ['UV','C']:
   i=unreal.CustomInput();i.set_editor_property('input_name',name_in);ins.append(i)
  custom.set_editor_property('inputs',ins);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);custom.set_editor_property('code',code)
  ML.connect_material_expressions(uv,'',custom,'UV');ML.connect_material_expressions(color_node,'',custom,'C');color_node=custom
 ML.connect_material_property(color_node,'',unreal.MaterialProperty.MP_BASE_COLOR)
 ML.connect_material_property(scalar(m,rough),'',unreal.MaterialProperty.MP_ROUGHNESS)
 if normal:
  sample=expr(m,unreal.MaterialExpressionTextureSample);sample.set_editor_property('texture',unreal.load_asset('/Game/Countryside_Interior/Textures/'+normal));sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
  ML.connect_material_expressions(uv,'',sample,'Coordinates');ML.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_NORMAL)
 ML.recompile_material(m);E.save_loaded_asset(m);return m
# Use the pack's physical fabric/wood/plaster textures, with a restrained toy-room palette.
mats={}
mats['Floor']=material('M_P2HoneyFloor',(1.15,.99,.76),.62,'T_Floor_01_BC',world='return P.xy/2800;',normal='T_Floor_01_N')
mats['Wall']=material('M_P2BluePlaster',(.56,.75,.8),.92,'T_Walls_01_BC',world='return float2(P.x+P.y,P.z)/2600;',normal='T_Walls_01_N')
mats['Wood']=material('M_P2Oak',(.84,.64,.39),.65,'T_Floor_01_BC',world='return float2(P.x+P.y,P.z)/2000;',normal='T_Floor_01_N')
mats['Panel']=material('M_P2TealPanels',(.12,.39,.39),.73,'T_Floor_01_BC',world='return float2(P.x+P.y,P.z)/2400;',normal='T_Floor_01_N')
mats['Ceiling']=material('M_P2Ceiling',(.55,.34,.18),.76,'T_Floor_01_BC',world='return P.xy/3400;',normal='T_Floor_01_N')
furcode='float weave=(sin(UV.x*680)*sin(UV.y*680))*.025; float stripe=pow(saturate(sin(UV.x*25+sin(UV.y*19)*1.4)),9)*smoothstep(.25,.6,UV.y);return C*(1+weave)*(1-stripe*.26);'
mats['Fur']=material('M_PlushFur',(.74,.43,.19),.91,normal='T_Pillow_N',tile=3,code=furcode)
mats['Cream']=material('M_PlushCream',(.83,.7,.49),.92,normal='T_Pillow_N',tile=3,code='return C*(1+.025*sin(UV.x*500)*sin(UV.y*500));')
mats['Ink']=material('M_PlushInk',(.028,.024,.017),.24)
mats['Nose']=material('M_PlushNose',(.31,.06,.055),.63,normal='T_Pillow_N',tile=2)
mats['Bow']=material('M_PlushBow',(.035,.24,.24),.73,normal='T_Towel_N',tile=2)
mats['Seam']=material('M_PlushThread',(.36,.18,.066),.96)
mats['Inner']=material('M_PlushInnerEar',(.64,.24,.19),.9,normal='T_Pillow_N',tile=3)
# Import smooth part meshes. Native head/eye/paw animation continues to drive these components.
catmeshes={}
for file in sorted((ROOT/'SourceAssets/CountrysideP2/Cat').glob('*.fbx')):
 name=file.stem;path=BASE+'/Meshes/'+name
 m=unreal.load_asset(path)
 if not m:
  t=unreal.AssetImportTask();t.filename=str(file);t.destination_path=BASE+'/Meshes';t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
  options=unreal.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.import_materials=False;options.import_textures=False
  options.static_mesh_import_data.set_editor_property('combine_meshes',True);options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',True);options.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
  t.options=options;t.factory=unreal.FbxFactory();AT.import_asset_tasks([t]);m=unreal.load_asset(path)
 assert m,name
 for i,s in enumerate(m.static_materials):
  n=str(s.material_slot_name);mat=mats['Inner'] if 'Inner' in n else mats['Seam'] if 'Seam' in n else mats['Fur']
  m.set_material(i,mat)
 E.save_loaded_asset(m);catmeshes[name]=m
meshcache={}
def mesh(name):
 if name not in meshcache:meshcache[name]=unreal.load_asset('/Game/Countryside_Interior/Meshes/'+name)
 assert meshcache[name],name
 return meshcache[name]
def mark(a,label):
 a.tags=[TAG];a.set_actor_label('P2 Countryside / '+label);a.set_folder_path('GiantRoom/CountrysideP2/'+label.split('/')[0]);added.append(a);return a
def hide(a):
 for c in a.get_components_by_class(unreal.PrimitiveComponent):c.set_visibility(False,False)
 hidden.append(a.get_name())
def fit(name,model,center,size,rot=None,mat=None):
 """Bound-center fit. All visual replacements are nonblocking; source collision is untouched."""
 m=mesh(model);b=m.get_bounds();rot=rot or unreal.Rotator()
 sc=unreal.Vector(*(size[i]/max(.001,[b.box_extent.x,b.box_extent.y,b.box_extent.z][i]*2) for i in range(3)))
 scaled=unreal.Vector(b.origin.x*sc.x,b.origin.y*sc.y,b.origin.z*sc.z)
 p,y,r=map(math.radians,(rot.pitch,rot.yaw,rot.roll));cp,sp,cy,sy,cr,sr=math.cos(p),math.sin(p),math.cos(y),math.sin(y),math.cos(r),math.sin(r)
 offset=unreal.Vector(cp*cy*scaled.x+(sr*sp*cy-cr*sy)*scaled.y-(cr*sp*cy+sr*sy)*scaled.z,cp*sy*scaled.x+(sr*sp*sy+cr*cy)*scaled.y+(sr*cy-cr*sp*sy)*scaled.z,sp*scaled.x-sr*cp*scaled.y+cr*cp*scaled.z)
 a=mark(A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*center)-offset,rot),name)
 a.set_actor_scale3d(sc);a.set_actor_enable_collision(False);c=a.static_mesh_component;c.set_static_mesh(m);c.set_cast_shadow(True)
 if mat:c.set_material(0,mat)
 return a
def sourcefit(a,model,mat=None):
 r=beforeby[a.get_name()];t=a.get_actor_transform();size=[x*100 for x in r['scale']]
 return fit(a.get_actor_label().replace('Study / ','').replace('Study V10 / ',''),model,r['loc'],size,a.get_actor_rotation(),mat)
def texturize(a,mat):
 a.static_mesh_component.set_material(0,mat);retextured.append(a.get_name())
by={}
for a in actors:by.setdefault(a.get_actor_label(),[]).append(a)
def retire(prefixes):
 for a in actors:
  if a.get_actor_label().startswith(tuple(prefixes)) and isinstance(a,(unreal.StaticMeshActor,unreal.TextRenderActor)):hide(a)
def groups(prefix):return [a for a in actors if a.get_actor_label().startswith(prefix)]
def exact(label):return by[label][0]
# Architecture: same floor elevation, wall extents, entry opening and ceiling.
for a in actors:
 if not isinstance(a,unreal.StaticMeshActor):continue
 label=a.get_actor_label()
 if label=='Study / Floor':texturize(a,mats['Floor'])
 elif label=='Study / Floor plank':hide(a)
 elif label in ['Study / Back wall','Study / Side wall','Study / Entry surround','Study / Entry upper wall']:texturize(a,mats['Wall'])
 elif label=='Study / Ceiling':texturize(a,mats['Ceiling'])
 elif label=='Study / Wainscot':texturize(a,mats['Panel'])
 elif label in ['Study / Dado moulding','Study / Wall panel trim']:texturize(a,mats['Wood'])
 elif label=='Study / Ceiling beam':
  hide(a);r=beforeby[a.get_name()];fit('Architecture/Rough hewn beam','SM_Beams_01',r['loc'],(480,480,20000),unreal.Rotator(roll=90))
# The hero desk matches the original 3000cm top exactly.
retire(['Study / Desk solid top','Study / Desk leg','Study / Desk side apron','Study / Desk rear apron','Study / Desk fine wood grain','Study / Desk end grain'])
fit('HeroDesk/Timber table','SM_MainTable',(12600,3600,1500),(3600,7200,3000))
retire(['Study / Chair '])
fit('HeroDesk/Carved opponent chair','SM_WoodenChair06',(16000,3600,1850),(2000,2000,3700),unreal.Rotator(yaw=90))
# Two actual carved bookshelves, fitted to the original shelf volumes.
retire(['Study / Bookcase '])
for y in [-1000,8200]:fit('Library/Carved bookcase','SM_Shelf_03',(22860,y,4200),(4400,1400,8400),unreal.Rotator(yaw=90))
for i,a in enumerate(groups('Study / Shelf book')):
 hide(a)
 r=beforeby[a.get_name()];size=[s*100 for s in r['scale']]
 # Lay the book mesh on its spine: local thickness becomes shelf Y, pages face the room.
 fit('Library/Bound volume %03d'%i,'SM_Book_%02d'%(1+i%6),r['loc'],(size[0],size[2],size[1]),unreal.Rotator(roll=90))
# A carved cabinet replaces the solid low-poly sideboard without touching its blocking envelope.
retire(['Study / Sideboard','Study / Drawer'])
fit('Sideboard/Worn painted dresser','SM_Cabinet_01',(10700,-5280,1800),(6000,1600,3600))
retire(['Study / Candle'])
for x in [9100,9580,10060]:fit('Sideboard/Ceramic candlestick','SM_Candle',(x,-5280,4150),(500,500,1100))
fit('Sideboard/Old radio','SM_Radio',(11700,-5280,4100),(1100,640,1000))
fit('Sideboard/Terracotta vase','SM_ClayVases_03',(13100,-5280,4230),(660,660,1260))
fit('Sideboard/Reading frames','SM_Frames_04',(10600,-5300,4370),(960,180,1440))
# Decorative desk objects retain their original silhouettes/locations, away from card landing.
retire(['Study / Mug','Study / Notebook','Study / Open notebook','Study / Pencil','Study / Binder clip','Study / Clip wire','Study / Standing atlas','Study / Atlas cover','Study / Bookend'])
fit('HeroDesk/Glazed coffee cup','SM_BreakfastProps_Mug',(13700,4360,3200),(400,500,400))
fit('HeroDesk/Coffee','SM_BreakfastProps_Coffee',(13700,4360,3376),(250,250,16))
fit('HeroDesk/Open sketchbook','SM_BookOpen_01',(12300,5840,3030),(1680,1200,60))
fit('HeroDesk/Weathered pencil','SM_Crayon03',(13566,5000,3018),(720,35,36))
fit('HeroDesk/Standing atlas','SM_Book_02',(14260,1320,3600),(1200,880,120),unreal.Rotator(pitch=90))
for i in range(4):
 ps=exact('Study / Desk book %d pages'%i);r=beforeby[ps.get_name()];cover=groups('Study / Desk book %d'%i)
 for a in cover:hide(a)
 fit('HeroDesk/Stacked journal %d'%i,'SM_Book_%02d'%(i+1),r['loc'],(r['scale'][0]*100+20,r['scale'][1]*100+20,r['scale'][2]*100+16))
# Eight seat volumes stay at their existing support heights; turn them into bound books.
for i in range(8):
 a=exact('Study V07 / Seat book pages %d'%i);r=beforeby[a.get_name()]
 for prefix in ['Study V07 / Seat book cover %d'%i,'Study V07 / Seat book pages %d'%i,'Study V07 / Seat book spine %d'%i]:
  for old in by.get(prefix,[]):hide(old)
 fit('HeroDesk/Seat journal %d'%i,'SM_Book_%02d'%(1+i%6),r['loc'],(1200,1600,200))
# Keep walking ramp meshes and collision bit-for-bit. Overlay a bound book with the same top plane.
for a in actors:
 label=a.get_actor_label()
 if not isinstance(a,unreal.StaticMeshActor):continue
 if re.fullmatch(r'Study V09 / Spiral flight [14]',label):
  hide(a);r=beforeby[a.get_name()];sourcefit(a,'SM_Book_04')
 elif label.startswith('Study V09 / Supporting hardback'):
  hide(a);sourcefit(a,'SM_Book_05')
 elif label.startswith('Study V09 / Corner book pages'):
  hide(a);r=beforeby[a.get_name()];fit('Route/Corner journal','SM_Book_03',r['loc'],(440,440,50))
 elif label.startswith('Study V09 / Corner book cover'):hide(a)
 elif label.startswith('Study V09 / Archive carton') or label.startswith('Study V09 / Carton lid'):texturize(a,mats['Wood'])
 elif re.fullmatch(r'Study V09 / Spiral flight [25]',label) or label=='Study V09 / Desk ruler bridge':texturize(a,mats['Wood'])
 elif label.startswith('Study V09 / Spiral flight') and (' pages' in label or ' binding' in label):hide(a)
# Rug and floor props: same footprints, no collision additions.
retire(['Study V10 / Rug '])
fit('Floor/Textile rug','SM_Carpet_03',(8820,3600,26),(3920,3020,10))
retire(['Study V10 / Floor notebook','Study V10 / Floor pencil','Study V10 / Floor plush','Study V10 / Backpack '])
fit('Floor/Closed journal','SM_Book_06',(8750,5530,68),(880,610,112))
fit('Floor/Loose crayon','SM_Crayon06',(7825,5490,35),(680,35,35),unreal.Rotator(yaw=18))
fit('Floor/Woven cushion','SM_Pillow',(9680,9100,550),(1300,1400,1100),unreal.Rotator(yaw=18))
fit('Floor/Curled ginger cat','SM_Cat',(9950,6550,590),(1700,1420,1160),unreal.Rotator(yaw=155))
retire(['Study / Plant '])
fit('Greenery/Terracotta planter','SM_FlowerPot_07',(17700,12200,760),(1280,1280,1520))
fit('Greenery/Indoor plant','SM_Plant_03',(17700,12200,2550),(3000,3000,3600))
# Replace flat window drawings with chunky countryside frames. Keep the original wall and opening.
retire(['Study / Window frame','Study / Window mullion','Study / Window crossbar'])
for y,yaw in [(-6160,0),(13360,180)]:fit('Architecture/Deep wooden window','SM_Windows_01',(12100,y,6800),(240,4880,5680),unreal.Rotator(yaw=90 if y<0 else -90))
for a in groups('Study / Warm window'):texturize(a,mats['Wall'])
# Actual sculpted ceiling fixtures at the old pendant positions.
retire(['Study / Pendant shade','Study / Pendant cord','Study / Pendant bulb'])
for x in [11700,19300]:fit('Lighting/Enamel pendant','SM_KitchenLamp',(x,3600,10300),(1700,1700,3800))
# Keep the articulated lamp and its source in place, but use physically textured enamel/metal.
for a in groups('Study V10 / Lamp '):
 if isinstance(a,unreal.StaticMeshActor) and not any(k in a.get_actor_label() for k in ['diffuser','cord']):
  a.static_mesh_component.set_material(0,unreal.load_asset('/Game/Countryside_Interior/Materials/MI_KitchenLamp'))
for a in groups('Study V11 / Lamp visible shaft'):hide(a)
# Purposeful warm practicals plus cooler window fill; avoid the old uniformly orange darkness.
for a in actors:
 if isinstance(a,unreal.PointLight):
  if a.get_actor_label().startswith('Study / '):
   c=a.light_component;c.set_light_color(unreal.LinearColor(1,.79,.56,1));c.set_intensity(c.get_editor_property('intensity')*1.4);c.set_editor_property('source_radius',180.0)
 if isinstance(a,unreal.SpotLight) and a.get_actor_label().startswith('Study'):
  c=a.light_component;c.set_light_color(unreal.LinearColor(1,.84,.65,1));c.set_editor_property('source_radius',65.0)
  if 'Cat warm key' in a.get_actor_label():c.set_intensity(1400000)
  if 'Cat amber rim' in a.get_actor_label():c.set_light_color(unreal.LinearColor(.52,.73,1,1));c.set_intensity(1100000)
def area(label,p,target,intensity,color,width,height,radius):
 a=mark(A.spawn_actor_from_class(unreal.RectLight,unreal.Vector(*p),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*p),unreal.Vector(*target))),label)
 c=a.light_component;c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE);c.set_intensity(intensity);c.set_light_color(unreal.LinearColor(*color,1));c.set_editor_property('source_width',width);c.set_editor_property('source_height',height);c.set_editor_property('attenuation_radius',radius);c.set_editor_property('cast_shadows',True);return a
area('Lighting/Soft window daylight',(12100,-5850,7400),(15100,3700,1800),5800000,(.61,.77,1),2600,3700,22000)
area('Lighting/Golden window bounce',(12100,12900,7700),(15000,5000,1800),4300000,(1,.79,.5),2400,3400,20000)
area('Lighting/Tabletop soft key',(13000,2400,6600),(14500,3600,3400),900000,(1,.87,.67),1500,1500,7500)
area('Lighting/Entry readability',(6900,6200,2500),(8900,2900,900),430000,(.64,.79,1),1200,1800,6500)
for a in actors:
 if isinstance(a,unreal.PostProcessVolume):
  settings=a.get_editor_property('settings')
  for n,v in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',1.),('auto_exposure_max_brightness',1.),('override_auto_exposure_bias',True),('auto_exposure_bias',.8),('override_bloom_intensity',True),('bloom_intensity',.22),('override_vignette_intensity',True),('vignette_intensity',.16)]:
   settings.set_editor_property(n,v)
  a.set_editor_property('settings',settings)
# Small story clusters are outside route/entrance/card-space envelopes.
for label,model,p,size,yaw in [
 ('Details/Paint tin','SM_PaintCan',(7800,-4200,600),(1100,1100,1200),-15),
 ('Details/Brush','SM_PaintBrush_01',(8250,-4400,1550),(120,150,2100),-12),
 ('Details/Warm reading lamp','SM_LampTall_01',(20200,10400,2700),(2200,2200,5400),0),
 ('Details/Ceramic vase','SM_ClayVases_02',(19800,-4100,670),(900,900,1340),0),
 ('Details/Wooden chest','SM_Chest',(20800,9800,730),(2100,1500,1460),-10),
 ('Details/Trumpet','SM_Trumpet',(20100,9200,1200),(1500,650,850),-20),
 ('Details/Leafy pot','SM_FlowersPlant_02',(20000,-4200,1720),(1700,1700,1900),0)]:fit(label,model,p,size,unreal.Rotator(yaw=yaw))
stage=next(a for a in actors if isinstance(a,unreal.CardRoomStage))
stage.tags=list(stage.tags) if TAG in stage.tags else list(stage.tags)+[TAG]
stage.set_actor_transform(stage.get_actor_transform(),False,True)
# Apply visible editor preview too. These materials are re-applied by the opt-in C++ skin at runtime.
for c in stage.get_components_by_class(unreal.StaticMeshComponent):
 n=c.get_name();mn='SM_PlushBody' if 'Body' in n else 'SM_PlushHead' if 'Head' in n else 'SM_PlushEar' if 'Ear' in n else 'SM_PlushPaw' if 'Mitten' in n else 'SM_PlushMuzzle' if 'Muzzle' in n else 'SM_PlushBow' if 'Bow' in n else 'SM_PlushButton' if 'Eye' in n or n=='Nose' else None
 if mn:
  c.set_static_mesh(catmeshes[mn]);c.set_cast_shadow(True)
  mat=mats['Cream'] if 'Muzzle' in n else mats['Ink'] if 'Eye' in n else mats['Nose'] if n=='Nose' else mats['Bow'] if 'Bow' in n else mats['Fur'];c.set_material(0,mat)
# Compare every original actor's transform before saving; all interaction and route anchors are fixed.
errors=[]
for a in actors:
 r=beforeby.get(a.get_name())
 if not r:continue
 if xyz(a.get_actor_location())!=r['loc'] or xyz(a.get_actor_scale3d())!=r['scale']:errors.append(a.get_actor_label())
assert not errors,errors
assert all(not a.get_actor_enable_collision() for a in added if isinstance(a,unreal.StaticMeshActor))
assert L.save_current_level()
report=dict(level=L.get_current_level().get_outer().get_name(),original_actors=len(actors),unchanged_transforms=len(actors),hidden_sources=len(set(hidden)),new_visual_actors=len(added),retextured=len(retextured),cat_mesh_bounds={n:xyz(m.get_bounds().box_extent) for n,m in catmeshes.items()},collision_policy='All original colliders preserved; all added static meshes NoCollision',source_hidden=hidden)
(OUT/'art_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('COUNTRYSIDE_P2_ART_READY '+str(len(added)))
