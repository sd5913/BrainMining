from Scripts.wensi_mcp import rpc; import json; print(json.dumps(rpc("tools/call",{"name":"list_toolsets","arguments":{}}),ensure_ascii=False))
