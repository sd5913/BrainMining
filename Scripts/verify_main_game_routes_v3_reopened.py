"""Verify the reopened compiled editor, after synchronizing seam metadata."""
import unreal
from pathlib import Path

root = Path(unreal.Paths.project_dir()).resolve()
for name in ['sync_main_game_routes_v3_manifest.py', 'verify_main_game_routes_v3_saved.py']:
    path = root / 'Scripts' / name
    exec(compile(path.read_text(encoding='utf-8'), str(path), 'exec'), {'__name__': '__main__'})
