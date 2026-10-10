"""Export two aligned FBX files, render front/style/placement previews, then reimport-check."""
import bpy,json,runpy
from pathlib import Path
from mathutils import Vector

OUTPUT=Path(__file__).parent
ns=runpy.run_path(str(OUTPUT/'build_wide.py'),run_name='wide_helpers')
enum_value=ns['enum_value']
tunnel=bpy.data.objects[ns['NAME']]
main_scene=bpy.data.scenes[ns['SCENE']]
block_scene=bpy.data.scenes['Tunnel_Wide_Blockers_V2']
blocks=[bpy.data.objects[f'SM_WideTunnelBlocker_{i:02}'] for i in range(1,21)]
fbx_main=OUTPUT/'SM_DescendingTunnel_Wide_Lowpoly.fbx'
fbx_blocks=OUTPUT/'Tunnel_Wide_Blockers_20.fbx'

def export(scene,objects,path):
    bpy.context.window.scene=scene;bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    for obj in objects:
        assert tuple(obj.location)==(0,0,0) and tuple(obj.scale)==(1,1,1)
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},global_scale=1,
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        use_mesh_modifiers=True,mesh_smooth_type='FACE',use_custom_props=True,bake_anim=False)

export(main_scene,[tunnel],fbx_main);export(block_scene,blocks,fbx_blocks)
paths=json.loads((OUTPUT/'centerlines.json').read_text())

def render(scene,objects,path,eye,width=1600,height=1200,bounds_object=None,labels=False):
    bpy.context.window.scene=scene
    camera_data=bpy.data.cameras.new('Temporary_Preview_Camera');camera=bpy.data.objects.new(camera_data.name,camera_data)
    scene.collection.objects.link(camera);enum_value(camera_data,'type','ORTHO');camera_data.clip_end=2000
    bounds=[(bounds_object or objects[0]).matrix_world@Vector(c) for c in (bounds_object or objects[0]).bound_box]
    center=sum(bounds,Vector())/8;eye=Vector(eye).normalized();rotation=eye.to_track_quat('Z','Y')
    local=[rotation.inverted()@(p-center) for p in bounds]
    horizontal=max(p.x for p in local)-min(p.x for p in local);vertical=max(p.y for p in local)-min(p.y for p in local)
    camera_data.ortho_scale=max(horizontal,vertical*width/height)*1.14
    camera.location=center+eye*600;camera.rotation_euler=rotation.to_euler()
    old_camera,old_engine,old_world=scene.camera,scene.render.engine,scene.world
    world=bpy.data.worlds.new('Temporary_Preview_World');world.color=(.026,.035,.050)
    text_objects=[]
    try:
        scene.camera=camera;scene.world=world
        try:scene.render.engine='BLENDER_WORKBENCH'
        except TypeError as error:raise RuntimeError(str(error))
        scene.render.resolution_x=width;scene.render.resolution_y=height;scene.render.resolution_percentage=100
        scene.render.film_transparent=False;enum_value(scene.render.image_settings,'file_format','PNG')
        shading=scene.display.shading
        for prop,value in [('light','STUDIO'),('color_type','MATERIAL'),('cavity_type','BOTH'),('background_type','WORLD')]:enum_value(shading,prop,value)
        shading.show_shadows=True;shading.show_cavity=True;shading.curvature_ridge_factor=.75;shading.curvature_valley_factor=.75
        if labels:
            for label,p,offset in [('START',paths[0]['points'][0],Vector((0,0,13))),('END',paths[0]['points'][-1],Vector((0,0,-15)))]:
                data=bpy.data.curves.new('Temporary_'+label,'FONT');data.body=label;data.size=6;enum_value(data,'align_x','CENTER')
                ob=bpy.data.objects.new(data.name,data);scene.collection.objects.link(ob);ob.location=Vector(p)+offset+eye*15
                ob.rotation_euler=rotation.to_euler();text_objects.append(ob)
        scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)
    finally:
        scene.camera,scene.render.engine,scene.world=old_camera,old_engine,old_world
        for ob in text_objects:
            data=ob.data;bpy.data.objects.remove(ob,do_unlink=True);bpy.data.curves.remove(data)
        bpy.data.objects.remove(camera,do_unlink=True);bpy.data.cameras.remove(camera_data);bpy.data.worlds.remove(world)
    return center,rotation,max(horizontal,vertical*width/height)*1.14

front=render(main_scene,[tunnel],OUTPUT/'Tunnel_Wide_Front.png',(0,-1,0),labels=True)
render(main_scene,[tunnel],OUTPUT/'Tunnel_Wide_Perspective.png',(.8,-1,.6))
guide=bpy.data.objects['REFERENCE_Wide_Tunnel_Fit_Only']
wire=bpy.data.objects.new('Temporary_Placement_Wire',guide.data.copy());wire.matrix_world=guide.matrix_world.copy();block_scene.collection.objects.link(wire)
wire.data.materials.clear();material=bpy.data.materials.new('Temporary_Wire_Material');material.diffuse_color=(.12,.28,.36,1);wire.data.materials.append(material)
for poly in wire.data.polygons:poly.material_index=0
dec=wire.modifiers.new('Wire_Display_Facets','DECIMATE');dec.ratio=.18
wf=wire.modifiers.new('Wire_Display','WIREFRAME');wf.thickness=.065;wf.use_replace=True
try:render(block_scene,blocks,OUTPUT/'Tunnel_Wide_Blockers_Placement.png',(.8,-1,.6),bounds_object=guide)
finally:
    mesh=wire.data;bpy.data.objects.remove(wire,do_unlink=True);bpy.data.meshes.remove(mesh);bpy.data.materials.remove(material)
chosen=blocks[2]
for ob in blocks:ob.hide_render=ob!=chosen
try:render(block_scene,[chosen],OUTPUT/'Tunnel_Wide_Blocker_Detail.png',(.8,-1,.6),1200,1000)
finally:
    for ob in blocks:ob.hide_render=False

# The source file opens in the strict Blender front view with just the cutter visible.
bpy.context.window.scene=main_scene;bpy.ops.object.select_all(action='DESELECT');tunnel.select_set(True);bpy.context.view_layer.objects.active=tunnel
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        space=area.spaces.active;enum_value(space.shading,'type','SOLID');enum_value(space.shading,'color_type','MATERIAL')
        space.shading.show_cavity=True;space.overlay.show_floor=False;space.overlay.show_axis_x=False;space.overlay.show_axis_y=False
        space.region_3d.view_location=front[0];space.region_3d.view_rotation=front[1];space.region_3d.view_distance=front[2]*.9
        enum_value(space.region_3d,'view_perspective','ORTHO')
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT/'Tunnel_Wide_V2.blend'))

validation=runpy.run_path(str(OUTPUT.parent/'TunnelCutter/validate_tunnel.py'),run_name='validation_helpers')
report={'files':[]}
for file,expected in [(fbx_main,[tunnel]),(fbx_blocks,blocks)]:
    temp=bpy.data.scenes.new('Temporary_Import_Verification');enum_value(temp.unit_settings,'system','METRIC');temp.unit_settings.scale_length=1
    bpy.context.window.scene=temp;bpy.ops.import_scene.fbx(filepath=str(file),use_custom_props=True)
    imported=[ob for ob in temp.objects if ob.type=='MESH'];assert len(imported)==len(expected),(file,len(imported))
    entries=[]
    for src in expected:
        dst=next(ob for ob in imported if ob.name.startswith(src.name))
        a=[tuple(src.matrix_world@Vector(c)) for c in src.bound_box];b=[tuple(dst.matrix_world@Vector(c)) for c in dst.bound_box]
        # Compare axis extrema: float noise can reorder equivalent box corners.
        error=max(abs(fn(p[i] for p in a)-fn(p[i] for p in b)) for i in range(3) for fn in (min,max))
        assert error<.001,(src.name,error)
        topology=validation['topology'](dst.data);assert topology['boundary_edges']==0 and topology['nonmanifold_edges']==0
        entries.append({'name':src.name,'max_bounds_error_m':error,'faces':len(dst.data.polygons)})
    report['files'].append({'path':str(file),'mesh_count':len(imported),'passed':True,'objects':entries})
    for ob in list(temp.objects):
        data=ob.data;bpy.data.objects.remove(ob,do_unlink=True)
        if data and data.users==0:bpy.data.meshes.remove(data)
    bpy.context.window.scene=main_scene;bpy.data.scenes.remove(temp)
report['passed']=True;(OUTPUT/'export_verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report),flush=True)
