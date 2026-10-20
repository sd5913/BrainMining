"""Compare the original and softened wall at the exact same live camera pose."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/verify_vessel_lighting_feedback.py').read_text(encoding='utf8')
code=code.replace("'Saved/Validation/TraversalFeedback/Blood'","'Saved/Validation/VesselDetail/Compare'")
code=code.replace('c.intensity==18000','c.intensity==900')
start=code.index("            capture('vessel_lit')")
end=code.index("    except Exception:",start)
code=code[:start]+"""            walls=[c for c in d.get_components_by_class(unreal.SplineMeshComponent) if c.get_name().startswith('BloodCirculationVessel')]
            check(len(walls)==30,'All vessel wall sections are present');s['walls']=walls
            check(all(c.get_material(0).get_name()=='MI_EE_SoftVein' for c in walls),'Production runtime selects the softened wall material')
            for wall in walls:wall.set_material(0,unreal.load_asset('/Game/BloodCirculation/Materials/MaterialInstance/BloodVessel/MI_Vein'))
            s.update(phase='original',next=now+2);return
        if p=='original':
            capture('01_original_wall')
            for wall in s['walls']:wall.set_material(0,unreal.load_asset('/Game/EndingEmergence/Materials/MI_EE_SoftVein'))
            s.update(phase='soft',next=now+4);return
        if p=='soft':
            capture('02_soft_wall')
            check(all(c.get_material(0).get_name()=='MI_EE_SoftVein' for c in s['walls']),'Only vessel walls use the softened material')
            finish();return
"""+code[end:]
code=code.replace("unreal.EditorPythonScripting.set_keep_python_script_alive(False)","unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()")
exec(compile(code,'compare_vessel_detail','exec'))
