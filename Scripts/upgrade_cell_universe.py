"""Explicitly rebuild the CellUniverse demo, backing up its old map first.

Run in a clean, dedicated editor. Touches only this demo and its own material.
"""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('create_cell_universe.py')), init_globals={'REBUILD_EXISTING': True})
