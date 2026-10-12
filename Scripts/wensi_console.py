"""Execute console commands through the discovered native Slate MCP controls."""
import json
import re
import sys
import time
from pathlib import Path
from wensi_editor_tool import call

SLATE='SlateInspectorToolset.SlateInspectorToolset'
def value(result):
    return json.loads(result['result']['content'][0]['text']).get('returnValue')

def console(commands):
    observer=value(call(SLATE,'Observe',{'ref':'','maxDepth':80}))
    try:
        time.sleep(.35)
        tree=value(call(SLATE,'Snapshot',{'ref':'','maxDepth':80}))
        Path('Saved/Validation/WenSiMirror/slate_latest.txt').write_text(tree,encoding='utf-8')
        boxes=[line for line in tree.splitlines() if re.search(r'textbox(?: \[[^\]]+\])* \[pos=',line)]
        if not boxes:raise RuntimeError('No visible native editor console textbox')
        ref=re.search(r'\[ref=([^\]]+)\]',boxes[-1]).group(1)
        for command in commands:
            if not value(call(SLATE,'Click',{'ref':ref})):raise RuntimeError('Console focus failed')
            if not value(call(SLATE,'Type',{'ref':ref,'text':command,'submit':True})):raise RuntimeError('Console input failed')
            print('CONSOLE '+command,flush=True)
            time.sleep(.4)
    finally:
        call(SLATE,'Unobserve',{'identifier':observer})

if __name__=='__main__':
    console(json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig')) if Path(sys.argv[1]).exists() else [sys.argv[1]])
