"""Use the project's configured local Unreal MCP endpoint when app tools are unavailable."""
import json
import sys
import urllib.request
from pathlib import Path


def rpc(method, params):
    headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream'}
    init=urllib.request.Request('http://127.0.0.1:8000/mcp',
        data=json.dumps({'jsonrpc':'2.0','id':0,'method':'initialize','params':{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'WenSiProjectTools','version':'1.0'}}}).encode(),headers=headers)
    with urllib.request.urlopen(init,timeout=30) as response:
        session=response.headers.get('Mcp-Session-Id')
        response.read()
    if session:
        headers['Mcp-Session-Id']=session
    notification=urllib.request.Request('http://127.0.0.1:8000/mcp',data=json.dumps({'jsonrpc':'2.0','method':'notifications/initialized'}).encode(),headers=headers)
    with urllib.request.urlopen(notification,timeout=30) as response:
        response.read()
    request = urllib.request.Request('http://127.0.0.1:8000/mcp',
        data=json.dumps({'jsonrpc':'2.0','id':1,'method':method,'params':params}).encode(),
        headers=headers)
    with urllib.request.urlopen(request, timeout=180) as response:
        body = response.read().decode()
    if body.lstrip().startswith('{'):
        return json.loads(body)
    for line in body.splitlines():
        if line.startswith('data:'):
            result=json.loads(line[5:])
            if 'result' in result or 'error' in result:
                return result
    raise RuntimeError(body[:500])


if __name__ == '__main__':
    method=sys.argv[1]
    params=json.loads(Path(sys.argv[2]).read_text(encoding='utf-8-sig')) if len(sys.argv)>2 else {}
    result=rpc(method,params)
    if len(sys.argv)>3:
        Path(sys.argv[3]).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        print('Saved MCP result: '+sys.argv[3])
    else:
        print(json.dumps(result,ensure_ascii=True))
