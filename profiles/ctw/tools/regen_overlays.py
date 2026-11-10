"""regen_overlays.py PSP_RECOMP OVERLAY_DIR OUT_DIR

Runs `psp_recomp ovl_NNN.elf --overlay OUT_DIR NNN` for every overlay ELF written by
make_ctw_overlays.py (with seeds.txt as PSPRECOMP_EXTRA_SEEDS) and writes OUT_DIR/ovl_registry.cpp,
which defines register_generated_overlays(Runtime&)."""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def main():
    tool, overlay_dir, out = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, PSPRECOMP_EXTRA_SEEDS=str(overlay_dir / 'seeds.txt'))
    elves = sorted(overlay_dir.glob('ovl_*.elf'))
    tags = [elf.stem[4:] for elf in elves]

    def run(item):
        elf, tag = item
        own = elf.with_suffix('.seeds')
        run_env = dict(env, PSPRECOMP_EXTRA_SEEDS=str(own)) if own.exists() else env
        result = subprocess.run([tool, str(elf), '--overlay', str(out), tag], env=run_env,
                                capture_output=True, text=True)
        return tag, result.returncode, (result.stdout + result.stderr).strip()

    failed = []
    with ThreadPoolExecutor(max_workers=4) as pool:
        for tag, code, text in pool.map(run, zip(elves, tags)):
            print(text)
            if code != 0:
                failed.append(tag)
    good = [tag for tag in tags if tag not in failed]
    for stale in out.glob('ovl_*.cpp'):
        if stale.stem[4:] not in good and stale.name != 'ovl_registry.cpp':
            stale.unlink()
    lines = ['#include "psprecomp/runtime.hpp"', '', 'namespace psprecomp {']
    lines += [f'void register_overlay_{tag}(Runtime &runtime);' for tag in good]
    lines += ['void register_generated_overlays(Runtime &runtime) {']
    lines += [f'    register_overlay_{tag}(runtime);' for tag in good]
    lines += ['}', '} // namespace psprecomp', '']
    (out / 'ovl_registry.cpp').write_text('\n'.join(lines))
    print(f'{len(good)} overlays generated, {len(failed)} failed: {failed}')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
