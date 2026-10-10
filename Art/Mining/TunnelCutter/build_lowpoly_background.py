"""Use a headless Blender session for the same model-generation workflow."""
import runpy
import bpy
from pathlib import Path

OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')
generator = runpy.run_path(str(OUTPUT / 'stylize_lowpoly.py'),run_name='rock_stylizer')
generator['generate']()
validation = runpy.run_path(str(OUTPUT / 'validate_tunnel.py'),run_name='mesh_validation')
validation['validate']('SM_DescendingTunnel_Cutter_Lowpoly','validation_lowpoly.json')
runpy.run_path(str(OUTPUT / 'save_lowpoly.py'),run_name='__main__')
