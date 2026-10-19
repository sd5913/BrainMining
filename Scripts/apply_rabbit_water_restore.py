import pathlib, runpy
root=pathlib.Path(__file__).resolve().parent
runpy.run_path(str(root/'restore_p2_rabbit_presentation.py'))
runpy.run_path(str(root/'restore_heaven_water_material.py'))
print('RABBIT_AND_WATER_RESTORE_COMPLETE')
