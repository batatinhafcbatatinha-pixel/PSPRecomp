#!/usr/bin/env python3
"""pspport: build a native PC port of any PSP game from its disc files, with the same steps for every game.

usage: python tools/pspport/pspport.py <step> [--port DIR] [--jobs N] [--force]

A port folder holds game_files/ (the UMD, copied and checked by the launcher). Everything generated goes to
<port>/build. Steps (each skips itself when its output exists; --force redoes it):
  tools      build psp_recomp / psp_analyze once for the whole repository (.local/pspport/fw)
  decrypt    game_files/PSP_GAME/SYSDIR/EBOOT.BIN -> build/EBOOT.dec (pspdecrypt, downloaded and compiled on this PC;
             an EBOOT that is already a plain ELF is copied)
  translate  psp_recomp --auto: the executable as C++ (build/gen)
  ffmpeg     the FFmpeg 7.1 libraries the host plays videos and ATRAC audio with (shared with the ctw profile)
  prepare    tools decrypt translate ffmpeg   (everything before compile)
  compile    compile and link build/native/bin/PSPNative.exe (profile "generic": the shared PSP host, no
             game-specific code)
  play       run the game
"""
import argparse
import io
import os
import shutil
import struct
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
WORK = REPO / '.local/pspport'
FW = WORK / 'fw'
LOAD_BASE = '0x08804000'   # where the PSP kernel loads a user-mode PRX (EBOOT); fixed-address ELFs keep their own
UNIT_SPAN = '0x8000'

PSPDECRYPT_COMMIT = 'c156627db7634d395c380c0a9589130f603307fc'
PSPDECRYPT_URL = f'https://codeload.github.com/John-K/pspdecrypt/zip/{PSPDECRYPT_COMMIT}'
PRXDECRYPT_MAIN = r'''// Decrypts one PRX/EBOOT with pspdecrypt's PRX decrypter (written by tools/pspport).
#include <cstdio>
#include <vector>
extern "C" {
#include "libkirk/kirk_engine.h"
#include "libkirk/SHA1.h"
}
#include "PrxDecrypter.cpp"
int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: prxdecrypt in out\n"); return 2; }
  FILE* f = std::fopen(argv[1], "rb"); if (!f) return 3;
  std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
  std::vector<u8> in(n), out(n + 0x100000);
  std::fread(in.data(), 1, n, f); std::fclose(f);
  kirk_init();
  int r = pspDecryptPRX(in.data(), out.data(), (u32)n, nullptr, true);
  if (r <= 0) { std::fprintf(stderr, "decryption failed (%d)\n", r); return 1; }
  f = std::fopen(argv[2], "wb"); std::fwrite(out.data(), 1, r, f); std::fclose(f);
  std::printf("decrypted %d bytes\n", r);
  return 0;
}
'''


class Port:
    def __init__(self, folder):
        self.dir = Path(folder).resolve()
        self.files = self.dir / 'game_files'
        self.build = self.dir / 'build'
        self.eboot = self.build / 'EBOOT.dec'
        self.gen = self.build / 'gen'
        self.native = self.build / 'native'
        self.exe = self.native / 'bin' / ('PSPNative.exe' if os.name == 'nt' else 'PSPNative')


def log(msg):
    print(msg, flush=True)


def run(cmd, cwd=None, env=None):
    log('$ ' + ' '.join(str(c) for c in cmd))
    r = subprocess.run([str(c) for c in cmd], cwd=cwd, env=env)
    if r.returncode != 0:
        raise SystemExit(f'failed ({r.returncode}): {cmd[0]}')


def need(name):
    path = shutil.which(name)
    if not path:
        raise SystemExit(f'{name} was not found: run this through the launcher or Setup (they put the build tools in PATH)')
    return path


def sfo_title(sfo_path):
    """TITLE of a PARAM.SFO, or None."""
    try:
        data = Path(sfo_path).read_bytes()
        magic, _, keys, values, count = struct.unpack_from('<4sIIII', data, 0)
        if magic != b'\0PSF':
            return None
        for i in range(count):
            key_off, fmt, length, _, data_off = struct.unpack_from('<HHIII', data, 20 + i * 16)
            key = data[keys + key_off:data.index(b'\0', keys + key_off)].decode('ascii', 'replace')
            if key == 'TITLE':
                return data[values + data_off:values + data_off + length].split(b'\0')[0].decode('utf-8', 'replace').strip()
    except (OSError, ValueError, struct.error):
        return None
    return None


# ------------------------------------------------------------------ steps

def step_tools(port, args):
    exes = [FW / ('psp_recomp.exe' if os.name == 'nt' else 'psp_recomp'), FW / ('psp_analyze.exe' if os.name == 'nt' else 'psp_analyze')]
    if all(e.is_file() for e in exes) and not args.force:
        log('- PSPRecomp tools: already built')
        return
    need('cmake'), need('ninja')
    run(['cmake', '-S', REPO, '-B', FW, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DPSPRECOMP_PROFILE=',
         '-DPSPRECOMP_BUILD_TESTS=OFF'])
    run(['cmake', '--build', FW, '--target', 'psp_recomp', 'psp_analyze'])


def prxdecrypt():
    exe = WORK / 'pspdecrypt' / ('prxdecrypt.exe' if os.name == 'nt' else 'prxdecrypt')
    if exe.is_file():
        return exe
    src = WORK / 'pspdecrypt' / f'pspdecrypt-{PSPDECRYPT_COMMIT}'
    if not (src / 'PrxDecrypter.cpp').is_file():
        log(f'Downloading pspdecrypt {PSPDECRYPT_COMMIT[:10]} (John-K, GPL-3.0; compiled and kept on this PC only)')
        with urllib.request.urlopen(urllib.request.Request(PSPDECRYPT_URL, headers={'User-Agent': 'pspport'}), timeout=120) as r:
            data = r.read()
        shutil.rmtree(src.parent, ignore_errors=True)
        src.parent.mkdir(parents=True)
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            top = z.namelist()[0].split('/')[0]
            z.extractall(src.parent)
        (src.parent / top).rename(src)
    (src / 'prxdecrypt_main.cpp').write_text(PRXDECRYPT_MAIN, encoding='utf-8')
    objs = []
    for c in sorted((src / 'libkirk').glob('*.c')):
        obj = src.parent / (c.stem + '.o')
        run([need('clang'), '-c', '-O2', '-w', '-o', obj, c], cwd=src)
        objs.append(obj)
    run([need('clang++'), '-O2', '-std=c++17', '-w', '-static', '-o', exe, 'prxdecrypt_main.cpp', *objs], cwd=src)
    return exe


def step_decrypt(port, args):
    if port.eboot.is_file() and not args.force:
        log('- decrypted EBOOT: already done')
        return
    source = port.files / 'PSP_GAME/SYSDIR/EBOOT.BIN'
    if not source.is_file():
        raise SystemExit(f'{source} is missing: install the game files first (the launcher does it from your disc)')
    port.build.mkdir(parents=True, exist_ok=True)
    head = source.read_bytes()[:4]
    if head == b'\x7fELF':
        log('EBOOT.BIN is a plain ELF: copied')
        shutil.copyfile(source, port.eboot)
        return
    if head != b'~PSP':
        raise SystemExit('EBOOT.BIN is neither an encrypted PSP executable (~PSP) nor an ELF')
    tmp = port.eboot.with_suffix('.part')
    run([prxdecrypt(), source, tmp])
    if tmp.read_bytes()[:4] != b'\x7fELF':
        raise SystemExit('the decrypted EBOOT is not an ELF (compressed or an unknown encryption)')
    tmp.replace(port.eboot)


def step_translate(port, args):
    done = port.gen / 'generated_registry.cpp'
    # Code a run had to interpret (build/learned.txt, written by the runtime's fallback interpreter) is translated
    # too, as extra seeds: a translation older than that list is done again.
    learned = port.build / 'learned.txt'
    relearn = learned.is_file() and done.is_file() and learned.stat().st_mtime > done.stat().st_mtime
    if done.is_file() and not args.force and not relearn:
        log('- translated code: already done')
        return
    if learned.is_file():
        os.environ['PSPRECOMP_EXTRA_SEEDS'] = str(learned)
        log(f'- {sum(1 for _ in learned.open())} addresses a run interpreted: translated this time')
    recomp = FW / ('psp_recomp.exe' if os.name == 'nt' else 'psp_recomp')
    # Written into a temporary folder and renamed at the end, so a stopped translation is redone from scratch
    # instead of leaving half the units behind.
    tmp = port.build / 'gen.part'
    shutil.rmtree(tmp, ignore_errors=True)
    run([recomp, port.eboot, '--auto', tmp, LOAD_BASE, UNIT_SPAN])
    shutil.rmtree(port.gen, ignore_errors=True)
    tmp.rename(port.gen)


def step_ffmpeg(port, args):
    if (REPO / 'profiles/ctw/third_party/ffmpeg/lib/avcodec.lib').is_file() and not args.force:
        log('- FFmpeg: already downloaded')
        return
    run([sys.executable, REPO / 'profiles/ctw/scripts/fetch_ffmpeg.py'])


def step_prepare(port, args):
    for step in (step_tools, step_decrypt, step_translate, step_ffmpeg):
        step(port, args)


def step_compile(port, args):
    need('cmake'), need('ninja')
    title = sfo_title(port.files / 'PSP_GAME/PARAM.SFO') or port.dir.name
    title = ''.join(c for c in title if c not in '"\\;$') or 'PSPRecomp'
    gen = str(port.gen).replace('\\', '/')
    run(['cmake', '-S', REPO, '-B', port.native, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DPSPRECOMP_PROFILE=generic',
         f'-DGENERIC_GENERATED_DIR={gen}', f'-DGENERIC_TITLE={title}', '-DPSPRECOMP_BUILD_TESTS=OFF'])
    run(['cmake', '--build', port.native, '--target', 'PSPNative', '--parallel', str(args.jobs)])


def step_play(port, args):
    if not port.exe.is_file():
        raise SystemExit('The game is not built yet: install it from the launcher.')
    # Untranslated code runs interpreted; the addresses go to build/learned.txt for the next translation.
    env = {**os.environ, 'PSPRECOMP_LEARN_FILE': str(port.build / 'learned.txt')}
    r = subprocess.run([str(port.exe), str(port.eboot), str(port.files)], cwd=port.exe.parent, env=env)
    sys.exit(r.returncode)


STEPS = {'tools': step_tools, 'decrypt': step_decrypt, 'translate': step_translate, 'ffmpeg': step_ffmpeg,
         'prepare': step_prepare, 'compile': step_compile, 'play': step_play,
         'all': lambda p, a: (step_prepare(p, a), step_compile(p, a))}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('step', choices=STEPS)
    ap.add_argument('--port', default='.')
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 1))
    ap.add_argument('--force', action='store_true')
    args = ap.parse_args()
    STEPS[args.step](Port(args.port), args)


if __name__ == '__main__':
    main()
