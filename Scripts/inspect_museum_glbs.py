import bpy,json,pathlib
root=pathlib.Path('H:/PolyU/Spotlight/BrainMining/SourceAssets/MuseumRefresh')
report={}
for name in ['screen','balance']:
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.gltf(filepath=str(root/name/(name+'.glb')))
 bpy.context.scene.frame_set(0)
 report[name]=[{'name':o.name,'type':o.type,'loc':list(o.location),'dim':list(o.dimensions),'parent':o.parent.name if o.parent else None,'verts':len(o.data.vertices) if o.type=='MESH' else 0,'materials':[{'name':m.name,'color':list(m.diffuse_color)} for m in o.data.materials if m] if o.type=='MESH' else []} for o in bpy.context.scene.objects]
(root/'glb_structure.json').write_text(json.dumps(report,indent=2))
