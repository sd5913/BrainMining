"""V08 overlay: shorter study, mixed clutter and editable presentation anchors.
Prerequisite: V06 then V07. Re-running V08 alone updates its props without shifting twice.
"""
import unreal, math, json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1')
assert not any(unreal.Name('StudyV09LayoutApplied') in a.tags for a in A.get_all_level_actors()), 'V09 active: rerun V09 only, or rebuild V06 -> V07 -> V08 -> V09.'
TAG=unreal.Name('StudyPresentationV08');SHIFT=unreal.Name('StudyV08LayoutApplied')
for a in list(A.get_all_level_actors()):
    if TAG in a.tags:A.destroy_actor(a)
actors=list(A.get_all_level_actors());stage=next(a for a in actors if isinstance(a,unreal.CardRoomStage));entrance=next(a for a in actors if isinstance(a,unreal.MiningGiantRoomEntrance))
base='/Game/Rooms/CardDuel/Materials/'
mats={k:unreal.load_asset(base+'M_'+k) for k in ['Oak','OakLight','Trim','Cream','Gold','Green','BookRed','BookBlue','BookGreen','Dark','GiantDeskPaper','GiantDeskPencil','GiantDeskInk','GiantDeskGlow']}
def material(name,color):
    path=base+'M_'+name;m=unreal.load_asset(path)
    if not m:
        m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_'+name,base.rstrip('/'),unreal.Material,unreal.MaterialFactoryNew())
        lib=unreal.MaterialEditingLibrary
        c=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(*color,1));lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
        r=lib.create_material_expression(m,unreal.MaterialExpressionConstant);r.set_editor_property('r',.87);lib.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
        lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
    mats[name]=m
material('StudyCardboard',(.63,.43,.24));material('StudyPackingTape',(.80,.64,.37))
def mark(a,label):
    a.tags=[TAG];a.set_actor_label('Study V08 / '+label);a.set_folder_path('GiantRoom/StudyPresentationV08');return a
def box(label,p,size,mat,rotation=None,collision=True):
    a=mark(A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),rotation or unreal.Rotator()),label)
    c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,mats[mat]);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if not collision:a.set_actor_enable_collision(False);c.set_cast_shadow(False)
    return a
def text(label,value,p,size=55,rotation=None):
    a=mark(A.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(*p),rotation or unreal.Rotator(yaw=180)),label)
    c=a.get_component_by_class(unreal.TextRenderComponent);c.set_text(value);c.set_world_size(size);c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER);c.set_text_render_color(unreal.Color(r=40,g=25,b=12,a=255));a.set_actor_enable_collision(False)
    return a
# Keep the excavatable wall and passage fixed; draw the entire furnished room 27m closer.
if SHIFT not in entrance.tags:
    for a in actors:
        tags={str(t) for t in a.tags};label=a.get_actor_label();p=a.get_actor_location();sc=a.get_actor_scale3d()
        if a==stage:a.set_actor_location(p+unreal.Vector(-2700,0,0),False,False);continue
        if 'CoherentStudyV06' in tags:
            if label in ['Study / Entry surround','Study / Entry upper wall']:continue
            if label in ['Study / Floor','Study / Side wall','Study / Wainscot','Study / Dado moulding','Study / Ceiling']:
                p.x-=1350;sc.x-=27;a.set_actor_location(p,False,False);a.set_actor_scale3d(sc);continue
            p.x-=2700
            if label=='Study / Floor plank':
                lo=p.x-sc.x*50;hi=p.x+sc.x*50
                if hi<=6400:A.destroy_actor(a);continue
                if lo<6400:sc.x=(hi-6400)/100;p.x=(hi+6400)/2;a.set_actor_scale3d(sc)
            if label=='Study / Wall panel trim' and p.x<6500:A.destroy_actor(a);continue
            a.set_actor_location(p,False,False)
        elif 'StudyAccessV07' in tags:
            stair=any(label.startswith('Study V07 / '+s) for s in ['Notebook','Ramp pages','Landing','File box','Box lid','Box label','Route paper','Entry notebook','Clutter route'])
            a.set_actor_location(p+unreal.Vector(-2700,2000 if stair else 0,0),False,False)
    entrance.tags=list(entrance.tags)+[SHIFT]
    bounds=entrance.get_component_by_class(unreal.BoxComponent);bounds.set_relative_location(unreal.Vector(8650,0,5810),False,False);bounds.set_box_extent(unreal.Vector(8650,10000,6000),False)
actors=list(A.get_all_level_actors())
by={a.get_actor_label():a for a in actors}
# V07 route geometry remains continuous; replace alternating bridge surfaces with real ruler / cardboard cues.
layout=json.loads((Path(unreal.Paths.project_saved_dir())/'Automation/StudyV07/layout.json').read_text(encoding='utf-8'))
for r in layout['ramps']:
    for key in ['start','end']:r[key][0]-=2700;r[key][1]+=2000
for p in layout['route']:p[0]-=2700;p[1]+=2000
# The V07 route finished at its old central seat; after the first tabletop point, head to the unchanged seat Y.
layout['route'][-2]=[11300,4200,3000];layout['route'][-1]=[12060,3600,3000]
for index in range(1,7):
    ramp=by['Study V07 / Notebook ramp %d'%index];r=layout['ramps'][index-1]
    p=unreal.Vector(*r['start']);q=unreal.Vector(*r['end']);u=(q-p).normal();rot=ramp.get_actor_rotation();length=(q-p).length()
    n=unreal.Vector(0,-(1 if q.y>p.y else -1)*498/length,1000/length)
    def at(t,side=0,h=0):
        v=p+u*t+unreal.Vector(side,0,0)+n*h;return (v.x,v.y,v.z)
    if index in [2,5]:
        ramp.static_mesh_component.set_material(0,mats['GiantDeskPencil'])
        for key in ['Ramp pages %d'%index,'Notebook binding %d'%index]:by['Study V07 / '+key].set_actor_hidden_in_game(True);by['Study V07 / '+key].set_is_temporarily_hidden_in_editor(True)
        for j in range(1,29):
            major=j%5==0;box('Ruler %d tick %d'%(index,j),at(j*38,-155 if major else -180,2),(8,105 if major else 55,3),'GiantDeskInk',rot,False)
        box('Ruler centre stripe %d'%index,at(length/2,100,1),(length-80,10,2),'Cream',rot,False)
    elif index in [3,6]:
        ramp.static_mesh_component.set_material(0,mats['StudyCardboard'])
        by['Study V07 / Ramp pages %d'%index].static_mesh_component.set_material(0,mats['StudyCardboard'])
        by['Study V07 / Notebook binding %d'%index].static_mesh_component.set_material(0,mats['StudyPackingTape'])
        box('Flattened carton seam %d'%index,at(length/2,0,2),(length-20,90,3),'StudyPackingTape',rot,False)
        for j in [1,2]:box('Cardboard fold %d.%d'%(index,j),at(length*j/3,0,2),(5,430,2),'Trim',rot,False)
# Warm kraft cardboard, taped seams and printed labels make the supports visibly paper boxes, not drawers.
for a in actors:
    label=a.get_actor_label()
    if label.startswith('Study V07 / File box ') or label.startswith('Study V07 / Box lid '):
        a.static_mesh_component.set_material(0,mats['StudyCardboard'])
    if label.startswith('Study V07 / File box '):
        p=a.get_actor_location();sc=a.get_actor_scale3d();suffix=label.split('File box ')[1]
        box('Packing tape '+suffix,(p.x,p.y-217,p.z),(110,4,sc.z*100),'StudyPackingTape',collision=False)
    if label.startswith('Study V07 / Box label '):
        p=a.get_actor_location();text('Printed box label', 'ARCHIVE' if '1.' in label else 'PAPER',(p.x-4,p.y,p.z+12),32)
# One open carton on the floor beside the route, with spread flaps outside the walkable lane.
box('Open carton base',(7700,5500,25),(760,620,30),'StudyCardboard')
for x in [7330,8070]:box('Open carton wall',(x,5500,240),(20,620,440),'StudyCardboard')
for y in [5200,5800]:box('Open carton wall',(7700,y,240),(760,20,440),'StudyCardboard')
box('Open carton left flap',(7220,5500,475),(250,620,14),'StudyCardboard',unreal.Rotator(pitch=-12))
box('Open carton right flap',(8180,5500,475),(250,620,14),'StudyCardboard',unreal.Rotator(pitch=12))
box('Loose notebook in carton',(7700,5500,155),(490,530,250),'BookBlue')
# Fixed battle camera and original tabletop: no open book or camera tracking.
stage.set_editor_property('card_landing_local',unreal.Vector(80,0,116));stage.set_editor_property('card_landing_pitch',0.0)
stage.set_editor_property('card_throw_height',190.0);stage.set_editor_property('card_visual_scale',2.2)
stage.set_editor_property('spell_presentation_scale',2.8);stage.set_editor_property('reward_local',unreal.Vector(220,0,395))
stage.set_actor_transform(stage.get_actor_transform(),False,True)
origin=stage.get_actor_location()
layout.update({'stage_world':[origin.x,origin.y,origin.z],'room_depth':17300,'entry_to_stair_distance':math.hypot(7200-6555,3580-3600),'card_landing_local':[80,0,116],'reward_local':[220,0,395],'version':'V08'})
assert layout['entry_to_stair_distance']<700
assert abs(origin.x-12600)<1
out=Path(unreal.Paths.project_saved_dir())/'Automation/StudyV08';out.mkdir(parents=True,exist_ok=True);(out/'layout.json').write_text(json.dumps(layout,indent=2),encoding='utf-8')
assert L.save_current_level()
unreal.log('STUDY_PRESENTATION_V08_READY')
