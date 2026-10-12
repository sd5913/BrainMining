"""Keep the UV-zero appearance of the cave's generated white rock on detached chips."""
import unreal
assets=unreal.AssetToolsHelpers.get_asset_tools()
for size in ['Large','Small']:
    for i in range(1,5):
        name=f'SM_Debris_Concrete_{size}_{i}'
        source=unreal.load_asset('/Game/Debris/Meshes/'+name);assert source
        path='/Game/Bomb/FX/'+name+'_CaveUV'
        mesh=unreal.load_asset(path) or assets.duplicate_asset(name+'_CaveUV','/Game/Bomb/FX',source)
        dynamic=unreal.DynamicMesh()
        _,status=unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(source,dynamic,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
        assert status==unreal.GeometryScriptOutcomePins.SUCCESS
        unreal.GeometryScript_UVs.scale_mesh_u_vs(dynamic,0,unreal.Vector2D(0.,0.),unreal.Vector2D(0.,0.),unreal.GeometryScriptMeshSelection())
        options=unreal.GeometryScriptCopyMeshToAssetOptions();options.set_editor_property('enable_recompute_normals',False);options.set_editor_property('enable_recompute_tangents',False)
        _,status=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dynamic,mesh,options,unreal.GeometryScriptMeshWriteLOD())
        assert status==unreal.GeometryScriptOutcomePins.SUCCESS
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
unreal.log('BOMB_CAVE_UV_READY: separate chip meshes, original drill/pickaxe assets preserved')
