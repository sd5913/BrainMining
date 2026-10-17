"""Restore only this project's editor window before using its native MCP console."""
import ctypes,sys,time,json
from pathlib import Path
from wensi_console import console
u=ctypes.windll.user32
CB=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
def restore(h,_):
 b=ctypes.create_unicode_buffer(512);u.GetWindowTextW(h,b,512)
 if b.value.startswith('BrainMining -') and u.IsIconic(h):u.ShowWindow(h,9)
 return True
u.EnumWindows(CB(restore),0);time.sleep(.35)
arg=sys.argv[1]
console(json.loads(Path(arg).read_text(encoding='utf-8-sig')) if Path(arg).is_file() else [arg])
