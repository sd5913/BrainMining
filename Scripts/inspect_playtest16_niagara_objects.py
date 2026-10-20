import unreal,json
from pathlib import Path
s=unreal.load_asset('/Game/Variant_Horror/Blueprints/Light/Assets/NS_DustMote')
r=[]
for o in unreal.ObjectIterator(unreal.Object):
 if unreal.SystemLibrary.get_path_name(o).startswith(s.get_path_name()+':'):
  row={'path':unreal.SystemLibrary.get_path_name(o),'class':o.get_class().get_name(),'values':{}}
  for p in ['modules','spawn_infos','renderer_properties','fixed_bounds','emitter_state','shape_primitive','box_size','sphere_radius','shape_scale','transform_offset','initial_position_distribution','sprite_size_distribution','lifetime_distribution','color_distribution','material','b_module_enabled','b_enabled']:
   try:row['values'][p]=str(o.get_editor_property(p))
   except:pass
  r.append(row)
(Path(unreal.Paths.project_dir())/'Saved/Validation/Playtest16/niagara_objects.json').write_text(json.dumps(r,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()

