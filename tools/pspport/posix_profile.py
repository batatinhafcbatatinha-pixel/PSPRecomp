#!/usr/bin/env python3
"""Native build and play entry point for existing profile folders."""
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import pspport as port_tools


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('step', choices=('prepare', 'compile', 'all', 'play'))
    parser.add_argument('--port', required=True)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    if os.name == 'nt':
        parser.error('Use the existing Windows scripts on Windows.')
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    port_tools.native_tool_path()
    port = port_tools.Port(args.port)
    if port.dir.name == 'generic':
        port_tools.STEPS[args.step](port, args)
        return
    setup_path = port.dir / 'setup.json'
    setup = json.loads(setup_path.read_text(encoding='utf-8')) if setup_path.exists() else {}
    cmake = (port.dir / 'CMakeLists.txt').read_text(encoding='utf-8')
    target = re.search(r'add_executable\((\w+)', cmake).group(1)
    port.exe = port.native / 'bin' / target
    if not setup:
        port.exe = port.native / 'bin/Release' / target
    if args.step in ('prepare', 'all'):
        if not setup:
            raise SystemExit('This profile needs its existing generated/ sources. Use compile after preparing them, or use the generic profile.')
        port_tools.step_tools(port, args)
        port_tools.step_decrypt(port, args)
        # Keep the profile's existing extraction arguments and seed selection.
        overlays = [s for s in setup.get('steps', [])
                    if any(str(a).endswith('.py') and 'overlay' in str(a) for a in s.get('run', []))]
        for step in overlays[:1]:
            command = [str(a).replace('{python}', sys.executable)
                       .replace('{game_files}', str(port.files)) for a in step['run']]
            port_tools.run(command, cwd=port.dir)
        seeds = port.build / 'overlays/eboot_seeds.txt'
        if seeds.is_file():
            os.environ['PSPRECOMP_EXTRA_SEEDS'] = str(seeds)
        port_tools.step_translate(port, args)
        for step in overlays[1:]:
            command = [str(a).replace('{python}', sys.executable) for a in step['run']]
            command = [str(port_tools.FW / 'psp_recomp') if a.endswith('/psp_recomp.exe') else a for a in command]
            port_tools.run(command, cwd=port.dir)
        port_tools.step_ffmpeg(port, args)
    if args.step in ('compile', 'all'):
        flags = []
        if setup:
            prefix = port.dir.name.upper()
            flags = [f'-D{prefix}_GENERATED_DIR={port.gen.as_posix()}',
                     f'-D{prefix}_OVERLAY_GENERATED_DIR={(port.build / "ovlgen").as_posix()}']
        port_tools.run([port_tools.need('cmake'), '-S', port_tools.REPO, '-B', port.native,
                        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                        f'-DPSPRECOMP_PROFILE={port.dir.name}', '-DPSPRECOMP_BUILD_TESTS=OFF',
                        '-DPSPRECOMP_BUILD_PROFILE_TESTS=OFF', *flags])
        port_tools.run(['cmake', '--build', port.native, '--target', target, '--parallel', str(args.jobs)])
    if args.step == 'play':
        if not port.exe.is_file() or not port.eboot.is_file() or not port.files.is_dir():
            raise SystemExit('Build the profile and provide build/EBOOT.dec and game_files first.')
        env = {**os.environ, 'PSPRECOMP_LEARN_FILE': str(port.build / 'learned.txt')}
        raise SystemExit(subprocess.call([str(port.exe), str(port.eboot), str(port.files)], cwd=port.exe.parent, env=env))


if __name__ == '__main__':
    main()
