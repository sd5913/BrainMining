import unreal, json
paths = [
    '/Game/BloodCirculation/Mesh/BloodVessel/SM_Vein',
    '/Game/BloodCirculation/Mesh/BloodVessel/SM_Aorta',
    '/Game/BloodCirculation/Mesh/BloodVessel/SM_SuperiorVenaCava',
    '/Game/BloodCirculation/Mesh/BloodVessel/SM_PulmonaryArtery',
    '/Game/BloodCirculation/Mesh/Cell/SM_RedBloodCell',
]
rows=[]
for p in paths:
    m=unreal.load_asset(p)
    if not m:
        rows.append({'path':p,'missing':True}); continue
    b=m.get_bounds()
    rows.append({'path':p,'size':[b.box_extent.x*2,b.box_extent.y*2,b.box_extent.z*2],
                 'materials':m.get_num_materials(),'triangles':m.get_num_triangles(0)})
print('BLOOD_ASSETS '+json.dumps(rows))
out=unreal.Paths.project_dir()+'/Saved/Validation/BloodAssets.json'
unreal.FileHelper.save_string_to_file(json.dumps(rows,indent=2),out)
