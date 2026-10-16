"""Inspect rendered fragment bounds, not merely the component visibility flag."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
code=(ROOT/'Scripts/verify_routes_v5_fx.py').read_text(encoding='utf-8')
code=code[:code.index('exec(compile')]
scope={}
exec(compile(code,'fx_wrapper','exec'),scope)
code=scope['code'].replace('MainGameRoutesV5/FX','MainGameRoutesV5/DebrisDiagnosis')
code=code.replace("        elif phase=='pickup':\n            key('E',False);assert s['pickups'][s['pickup']].collected;s['pickup']+=1;", "        elif phase=='pickup':\n            key('E',False)\n            if not s['pickups'][s['pickup']].collected:\n                s['pickup_attempt']=s.get('pickup_attempt',0)+1\n                assert s['pickup_attempt']<=4,'Pickup input failed after warmup/retry'\n                s.update(phase='aim_pickup',next=now+2);return\n            s['pickup_attempt']=0;s['pickup']+=1;")
code=code.replace("next=now+.65);key('LeftMouseButton',True)","next=now+.03,hit_started=now);key('LeftMouseButton',True)")
code=code.replace("        elif phase=='observe':\n            record()", "        elif phase=='observe':\n            if s['fx'].debris_burst_count==s['bursts']:\n                assert now-s['hit_started']<8, 'No impact after holding mouse'\n                return\n            if not s.get('capture_at'):\n                s['capture_at']=now+.09;key('LeftMouseButton',False);return\n            if now<s['capture_at']:return\n            s['capture_at']=None\n            record()")
code=code.replace("        bursts.append(dict(","        details=[]\n        for c in visible:\n            pos,ext,radius=c.get_component_bounds()\n            details.append(dict(position=list(c.get_world_location().to_tuple()),bounds=list(pos.to_tuple()),extent=list(ext.to_tuple()),scale=list(c.get_world_scale().to_tuple()),mesh=str(c.static_mesh),mesh_origin=list(c.static_mesh.get_bounds().origin.to_tuple()),velocity=list(c.get_physics_linear_velocity().to_tuple()),hidden=c.get_editor_property('hidden_in_game')))\n        bursts.append(dict(details=details,")
code=code.replace('c.get_component_bounds()', 'unreal.SystemLibrary.get_component_bounds(c)')
code=code.replace("r=dict(case=", "r=dict(camera=list(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location().to_tuple()),case=")
code=code.replace("STRICT and not s.get('upgraded')", "False and not s.get('upgraded')")
exec(compile(code,'debris_diagnosis','exec'),dict(__name__='__main__',STRICT=False,cases=[(0,.23,False),(0,.8,False,'guide')]))
