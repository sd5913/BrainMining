"""Resolve the currently installed MainGame route revision for editor scripts."""
import json
from pathlib import Path
def route_root(project_root):
    project_root=Path(project_root)
    config=project_root/'Config/MainGameRoutes.json'
    return project_root/(json.loads(config.read_text(encoding='utf-8'))['source_root'] if config.exists() else 'Art/MainGame')
