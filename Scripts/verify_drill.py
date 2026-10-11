"""Compatibility entry point for the current V drill / cooldown validation."""
from pathlib import Path
import unreal
script = Path(unreal.Paths.project_dir()) / "Scripts/verify_drill_revision.py"
exec(compile(script.read_text(encoding="utf-8-sig"), str(script), "exec"))
