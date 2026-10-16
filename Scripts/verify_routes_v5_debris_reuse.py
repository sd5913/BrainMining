"""Real mouse strikes must produce rendered fragments at the CURRENT impact."""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
source=(ROOT/'Scripts/diagnose_routes_v5_debris.py').read_text(encoding='utf-8')
scope={};exec(compile(source[:source.index("exec(compile(code,'debris_diagnosis'")],'diagnostic_helpers','exec'),scope)
code=scope['code'].replace('MainGameRoutesV5/DebrisDiagnosis','MainGameRoutesV5/DebrisFixed')
code=code.replace("STRICT and not s.get('upgraded')", "False and not s.get('upgraded')")
code=code.replace("    report['samples'].append(r)", """    active=[b for b in bursts if b['active'] and not b['hidden']]
    check(bool(active), 'Current strike has active fragments')
    near=[d for b in active for d in b['details'] if unreal.Vector(*d['bounds']).distance(s['point'])<350]
    check(len(near)>=10, 'At least ten physical fragments remain beside this hit, including reused bursts')
    check(all(b['material']==str(s['rock'].rock_material) for b in active), 'Fragments retain the struck material')
    check(any(unreal.Vector(*d['velocity']).length()>10 for d in near), 'Fragments are flying rather than static decoration')
    r['near_current_hit']=len(near)
    report['samples'].append(r)""")
# Repeated strikes and remote reuse with the starting pickaxe, plus all cave colours.
code=code.replace("passed=STRICT and error is None", "passed=error is None")
code=code.replace("            h=unreal.SystemLibrary.line_trace_single", "            report['trace_context']=dict(start=list(p.to_tuple()),end=list(end.to_tuple()),guides=[dict(name=g.get_actor_label(),position=list(g.get_actor_location().to_tuple()),bounds=[list(v.to_tuple()) for v in g.get_actor_bounds(False)],triangles=g.triangle_count,chunked=g.get_editor_property('bChunked'),hidden=g.get_editor_property('hidden'),collision=g.get_actor_enable_collision(),components=[dict(collision=str(c.get_collision_enabled()),triangles=c.get_dynamic_mesh().get_triangle_count()) for c in g.get_components_by_class(unreal.DynamicMeshComponent)]) for g in region.generated_guide_layers])\n            h=unreal.SystemLibrary.line_trace_single")
code=code.replace("move(h[5]+h[6]*160-unreal.Vector(0,0,64));aim(h[5]);s.update(phase='hit',next=now+.8)","move(h[5]+h[6]*420-unreal.Vector(0,0,64));aim(h[5]);s.update(phase='overview',next=now+1.2)")
code=code.replace("        elif phase=='hit':", """        elif phase=='overview':
            helper.capture_preview(s['pc'],str(OUT/('surface_%02d.png'%s['index'])))
            move(s['point']+s['normal']*160-unreal.Vector(0,0,64));aim(s['point']);s.update(phase='hit',next=now+.8)
        elif phase=='hit':""")
exec(compile(code,'verify_debris_reuse','exec'),dict(__name__='__main__',STRICT=False,cases=[(0,.23,False),(0,.23,False),(0,.8,False,'guide'),(1,.4,False),(1,.8,False,'guide'),(2,.8,False)]))
