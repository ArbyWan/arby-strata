"""Reported-device adapter, not an upstream support-list or engine patch.

Requires the matching official TheRock gfx1010 SDK/runtime installed already.
Requires the pinned GGUF already in workspace/models; may download supporting
packs/build dependencies and compile. Uses private setup APIs at the pinned commit.
"""
import argparse
import importlib.util
import os
from pathlib import Path
import sys

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, required=True)
    options = parser.parse_args()
    source, workspace = options.source.resolve(), options.workspace.resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    model = workspace / 'models/Qwen3.6-35B-A3B-UD-Q3_K_M.gguf'
    if not model.is_file():
        parser.error('Place the pinned, SHA256-verified GGUF in workspace/models first')
    os.chdir(source)
    os.environ['PIP_CACHE_DIR'] = str(workspace / 'pip-cache')
    os.environ['PYTHONUNBUFFERED'] = '1'
    sys.path.insert(0, str(source))
    spec = importlib.util.spec_from_file_location('poly_upstream_setup', source / 'setup.py')
    up = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(up)
    captured = None
    original_parse = argparse.ArgumentParser.parse_args
    class Captured(BaseException):
        pass
    def capture(parser, *args, **kwargs):
        nonlocal captured
        captured = original_parse(parser, *args, **kwargs)
        raise Captured()
    sys.argv = [str(source / 'setup.py'), '--family', 'qwen36', '--model', 'UD-Q3_K_M',
                '--backend', 'hip', '--yes', '--no-start', '--no-browser',
                '--gguf-dir', str(workspace / 'models'), '--data-dir', str(workspace / 'data'),
                '--port', '18085']
    argparse.ArgumentParser.parse_args = capture
    try:
        up.main()
    except Captured:
        pass
    finally:
        argparse.ArgumentParser.parse_args = original_parse
    if captured is None:
        raise RuntimeError('Pinned setup did not expose its expected parser')
    captured._custom_gguf_shards = None
    captured.models_dir = str(workspace / 'data/models')
    gpu = {'name': 'AMD Radeon RX 5700 XT', 'arch': 'gfx1010', 'archs': ['gfx1010'],
           'vram_gb': 8176 / 1024, 'index': 0, 'count': 1, 'driver': 'hip'}
    raise SystemExit(up.poly_setup(captured, 'qwen36', gpu, True, None, None, 256.0,
                                   workspace / 'data', [], 18085, None))

if __name__ == '__main__':
    main()
