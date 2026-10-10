"""Reuse the validated exact-intersection method with wider, twice-as-long plugs."""
import bpy,json
from pathlib import Path

OUTPUT=Path(__file__).parent
previous=OUTPUT.parent/'TunnelBlockers'/'generate_blockers.py'
code=previous.read_text(encoding='utf-8')
# Explicit, checked substitutions keep the first version untouched.
changes={
    "OUTPUT = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelBlockers')":"OUTPUT = Path(__file__).parent",
    "SOURCE = Path('H:/PolyU/Spotlight/BrainMining/Art/Mining/TunnelCutter')":"SOURCE = OUTPUT",
    "SEED = 20261010":"SEED = 20261011",
    "TUNNEL = 'SM_DescendingTunnel_Cutter_Lowpoly'":"TUNNEL = 'SM_DescendingTunnel_Wide_Lowpoly'",
    "SCENE = 'Tunnel_Blockers_20'":"SCENE = 'Tunnel_Wide_Blockers_V2'",
    "REFERENCE = 'REFERENCE_Tunnel_Fit_Only'":"REFERENCE = 'REFERENCE_Wide_Tunnel_Fit_Only'",
    "COUNTS = [10, 1, 2, 1, 2, 1, 2, 1]":"COUNTS = [10, 2, 1, 2, 2, 1, 1, 1]",
    "rng.uniform(9, path['arc'][-1]-7)":"((rng.uniform(22,23) if not selected else rng.uniform(path['arc'][-1]-13,path['arc'][-1]-12)) if count == 2 else rng.uniform(22,path['arc'][-1]-12))",
    "abs(position-root)<10":"abs(position-root)<22",
    "abs(position-old)<10":"abs(position-old)<20",
    ".length<8.8":".length<20",
    "rng.uniform(1.05,1.7)":"rng.uniform(2.4,3.4)",
    "SM_TunnelBlocker_":"SM_WideTunnelBlocker_",
    "grid=12":"grid=24",
    "span=7":"span=16",
    "relief=.42*noise.noise(p/1.6)+.16*noise.noise(p/.7)":"relief=.65*noise.noise(p/3.2)+.22*noise.noise(p/1.4)",
    "max(-.55,min(.55,relief))":"max(-.8,min(.8,relief))",
    "SOURCE/'validate_tunnel.py'":"SOURCE.parent/'TunnelCutter'/'validate_tunnel.py'",
    "ray,7)":"ray,18)",
    "center-tangent*5,tangent,10":"center-tangent*8,tangent,16",
    "OUTPUT/'validation.json'":"OUTPUT/'blockers_validation.json'",
}
for old,new in changes.items():
    assert old in code,old
    code=code.replace(old,new)
ns={'__file__':str(__file__),'__name__':'wide_blocker_helpers'}
exec(compile(code,str(previous),'exec'),ns)

if __name__=='__main__':
    ns['initialize']()
    ns['generate_range'](0,20)
    ns['validate']()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT/'Tunnel_Wide_V2.blend'))
