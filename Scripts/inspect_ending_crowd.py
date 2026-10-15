"""Read-only inspection of the two acquired crowd packs."""
import unreal
import json
from pathlib import Path

registry = unreal.AssetRegistryHelpers.get_asset_registry()
rows = []
for root in ['/Game/CitySampleCrowd', '/Game/Walk_cycle_AnimsSet']:
    for a in registry.get_assets_by_path(root, recursive=True):
        cls = str(a.asset_class_path.asset_name)
        if cls in ['SkeletalMesh', 'Skeleton', 'AnimSequence', 'StaticMesh', 'GroomAsset', 'IKRigDefinition', 'IKRetargeter']:
            rows.append(dict(path=str(a.package_name), cls=cls))
out = {'assets': rows, 'samples': [], 'api': {}}
names = ['m_tal_nrw_base', 'm_tal_nrw_body', 'm_tal_nrw_crewneck', 'm_tal_nrw_jeans', 'm_tal_nrw_oxfords', 'Anim_FemaleWalk', 'MTN_N_WalkQuickly_F']
for name in names:
    row = next((r for r in rows if r['path'].split('/')[-1] == name), None)
    if not row:
        continue
    a = unreal.load_asset(row['path'])
    sample = dict(row)
    if row['cls'] == 'SkeletalMesh':
        sample['materials'] = [str(m.material_interface.get_path_name()) if m.material_interface else None for m in a.get_editor_property('materials')]
        c = unreal.new_object(unreal.SkeletalMeshComponent)
        c.set_skeletal_mesh_asset(a)
        sample['bones'] = [str(c.get_bone_name(i)) for i in range(c.get_num_bones())]
        sample['lod_count'] = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(a)
    sample['skeleton'] = a.get_editor_property('skeleton').get_path_name()
    out['samples'].append(sample)
for name in ['IKRigController', 'IKRetargeterController', 'IKRetargetBatchOperation', 'AnimationLibrary', 'AnimationBlueprintLibrary']:
    cls = getattr(unreal, name, None)
    out['api'][name] = [x for x in dir(cls) if not x.startswith('_')] if cls else []
path = Path(unreal.Paths.project_saved_dir()) / 'Validation' / 'EndingCrowdInspect.json'
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(out, indent=2), encoding='utf-8')
unreal.log('ENDING_CROWD_INSPECTED ' + str(path))
