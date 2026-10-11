"""Import the separate blocker FBX without recentering any object; configure the independent tool."""
from pathlib import Path
import unreal

DEST='/Game/ExternalMining/GuideLayers/WideV2'
SOURCE=Path(unreal.Paths.project_dir())/'Art/Mining/TunnelWideV2/Tunnel_Wide_Blockers_20.fbx'

def models(reimport=False):
    assets=[unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST,recursive=False)]
    meshes=[a for a in assets if isinstance(a,unreal.StaticMesh)]
    if not meshes or reimport:
        options=unreal.FbxImportUI()
        options.set_editor_property('import_mesh',True)
        options.set_editor_property('import_as_skeletal',False)
        options.set_editor_property('import_animations',False)
        options.set_editor_property('import_materials',True)
        options.set_editor_property('import_textures',False)
        options.set_editor_property('automated_import_should_detect_type',False)
        options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
        data=options.static_mesh_import_data
        data.set_editor_property('combine_meshes',False)
        data.set_editor_property('convert_scene',True)
        data.set_editor_property('convert_scene_unit',True)
        data.set_editor_property('transform_vertex_to_absolute',True)
        data.set_editor_property('bake_pivot_in_vertex',False)
        data.set_editor_property('auto_generate_collision',False)
        # Boolean-fitted caps contain small valid triangles: dropping these opens the solid.
        data.set_editor_property('remove_degenerates',False)
        data.set_editor_property('build_nanite',False)
        task=unreal.AssetImportTask()
        for name,value in {'filename':str(SOURCE),'destination_path':DEST,'automated':True,'save':True,'replace_existing':reimport,'options':options,'factory':unreal.FbxFactory()}.items():
            task.set_editor_property(name,value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assets=[unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(DEST,recursive=False)]
        meshes=[a for a in assets if isinstance(a,unreal.StaticMesh)]
    meshes.sort(key=lambda a:a.get_name())
    assert len(meshes)==20,[(a.get_name(),a.get_class().get_name()) for a in assets]
    assert all(f'{i:02}' in mesh.get_name() for i,mesh in enumerate(meshes,1)),[m.get_name() for m in meshes]
    return meshes

def configure(region,meshes):
    layers=[]
    for mesh in meshes:
        layer=unreal.MiningExternalGuideLayer()
        layer.set_editor_property('mesh',mesh)
        layers.append(layer)
    region.set_editor_property('guide_layers',layers)
    region.set_editor_property('guide_follow_cutter_transform',True)
    region.set_editor_property('guide_cutter_index',0)

def region_in_current_level():
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion) and a.get_name()=='MiningExternalRegion_1')
