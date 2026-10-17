"""Scale magic emission for P2's physical lighting exposure, without changing scene lights."""
from pathlib import Path
import unreal
f=Path(unreal.Paths.project_dir())/'Scripts/refine_countryside_p2.py'
setup=f.read_text(encoding='utf-8').split("\nbuild('M_P2RuneRing'")[0]
exec(compile(setup,str(f),'exec'),globals())
build('M_P2RuneRing','float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);float ring=1-smoothstep(.012,.035,abs(r-.82));float outer=(1-smoothstep(.007,.018,abs(r-.96)))*step(.35,frac(a*6.366));float glyph=(1-smoothstep(.014,.03,abs(r-.66)))*step(.65,frac(a*3.183));return saturate(ring+outer+glyph)*(1-smoothstep(.99,1,r));')
build('M_P2EnergyCore','float a=sin(UV.y*65+sin(UV.x*29+T*3)*2-T*6);float b=sin(UV.x*58+UV.y*21+T*7);return .18+.5*pow(saturate(a*b),3);',True)
unreal.log('P2_ENERGY_EMISSION_EXPOSURE_MATCHED')
