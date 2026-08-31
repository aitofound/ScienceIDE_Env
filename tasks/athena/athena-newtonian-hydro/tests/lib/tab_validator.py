"""Small compatibility surface for official native-output validation."""
from __future__ import annotations
import json
from pathlib import Path

def _load(path: Path): return json.loads(path.read_text(encoding='utf-8'))
def load_artifact(*args, **kwargs):
    return None, {'reason':'official native result is checked by tests/lib/official.py'}
def validate(*args, **kwargs):
    return {'passed':False,'reason':'use official.py acceptance logic'}
