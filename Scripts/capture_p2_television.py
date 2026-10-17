"""Capture a discovered native Slate window, rather than a different viewport."""
import sys,re,time,base64
from pathlib import Path
from wensi_console import value,SLATE
from wensi_editor_tool import call
obs=value(call(SLATE,'Observe',{'ref':'','maxDepth':2}))
try:
 tree=value(call(SLATE,'Snapshot',{'ref':'','maxDepth':2}))
 windows=[line for line in tree.splitlines() if line.startswith('window ')]
 print('\n'.join(windows))
 matching=[line for line in windows if sys.argv[1].lower() in line.lower()]
 assert len(matching)==1,matching
 ref=re.search(r'\[ref=([^\]]+)\]',matching[0]).group(1)
 result=value(call(SLATE,'Screenshot',{'ref':ref}))
 Path(sys.argv[2]).write_bytes(base64.b64decode(result['data']))
 print('Saved '+sys.argv[2])
finally:
 call(SLATE,'Unobserve',{'identifier':obs})
