"""Dispatch a tool after checking its locally discovered MCP schema."""
import json
import sys
from pathlib import Path
from wensi_mcp import rpc

def call(toolset,tool,args):
    cache=Path('Saved/Validation/WenSiMirror')/(toolset.split('.')[-1]+'.json')
    if not cache.exists():
        cache.write_text(json.dumps(rpc('tools/call',{'name':'describe_toolset','arguments':{'toolset_name':toolset}})),encoding='utf-8')
    schema=json.loads(json.loads(cache.read_text(encoding='utf-8'))['result']['content'][0]['text'])
    if not any(t['name'].split('.')[-1]==tool for t in schema['tools']):
        raise RuntimeError('Undiscovered tool '+tool)
    result=rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':toolset,'tool_name':tool,'arguments':args}})
    if result.get('error') or result.get('result',{}).get('isError'):
        raise RuntimeError(str(result))
    return result

if __name__=='__main__':
    result=call(sys.argv[1],sys.argv[2],json.loads(sys.argv[3]) if len(sys.argv)>3 else {})
    if len(sys.argv)>4:
        Path(sys.argv[4]).write_text(json.dumps(result,ensure_ascii=False),encoding='utf-8')
        print('Saved '+sys.argv[4])
    else:
        print(json.dumps(result,ensure_ascii=True))
