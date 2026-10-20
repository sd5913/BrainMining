"""Reuse the real clot/flight regression, with this revision's visual checks."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/verify_feedback13_ending.py').read_text(encoding='utf8')
code=code.replace("'Saved/Validation/Feedback13/Ending'","'Saved/Validation/Feedback11/Ending'")
old="check(abs(shape.y/shape.x-1.2)<.001 and abs(shape.z/shape.x-3300/3500)<.001,'Cytoplasm and cell membrane have matching ellipsoid axes');shot('07_ellipsoid_cytoplasm')"
new="""check(abs(shape.x-1)<.001 and abs(shape.y-1)<.001 and abs(shape.z-1)<.001,'Cytoplasm retains original spherical scale');shot('07_round_cytoplasm')
 galaxies=next(c for c in d.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name().startswith('DenseCellGalaxies'))
 check(galaxies.get_instance_count()==666 and galaxies.is_visible(),'All 666 surrounding cells retain visible nebula instances')
 check(galaxies.get_material(0).get_scalar_parameter_value('Reveal')>.99,'Surrounding nebula material is fully revealed')
 for t in [0,5,12,17]:
  H.write_preview_state(d,'ShotTime',str(t));yield .15;shot('07_cells_'+str(t))
 H.write_preview_state(d,'Shot','1');H.write_preview_state(d,'ShotTime','14.5');yield .15;shot('08_extended_city')
 """
assert old in code;code=code.replace(old,new)
old="yield 6;H.resize_preview_viewport(pc,1280,720);yield 1;shot('01_cell_room')"
new="""yield 6;H.resize_preview_viewport(pc,1280,720);yield 1;shot('01_cell_room')
 seals=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'EndingCellRearSeal');check(len(seals)==1,'Rear cell wall opening is sealed')
 pose(unreal.Vector(150,0,492),unreal.Vector(-470,1,492));yield .4;shot('01_rear_wall_sealed')
 hit=unreal.SystemLibrary.line_trace_single(s['w'],unreal.Vector(150,1,492),unreal.Vector(-600,1,492),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[pawn],unreal.DrawDebugTrace.NONE,True).to_tuple()
 check(bool(hit[0]),'Rear opening also blocks collision')
 pose(unreal.Vector(600,0,300),unreal.Vector(1390,0,275));yield .3
 """
assert old in code;code=code.replace(old,new)
code=code.replace("400<lamp.intensity<2500","140<lamp.intensity<160")
exec(compile(code,str(root/'Scripts/verify_feedback13_ending.py'),'exec'))
