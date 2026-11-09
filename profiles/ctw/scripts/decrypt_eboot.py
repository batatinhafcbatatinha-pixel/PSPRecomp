"""Decrypt your own EBOOT.BIN into build/EBOOT.dec (the ELF that psp_recomp translates).

PSPRecomp ships no decryption code. This step downloads pspdecrypt (John-K, GPL-3.0) at a pinned commit into
build/pspdecrypt on your PC, compiles its PRX decrypter with the Visual C++ compiler (run from Setup / the launcher,
or an "x64 Native Tools" prompt) and runs it on game_files/PSP_GAME/SYSDIR/EBOOT.BIN. Nothing is published.
"""
import io
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMIT = 'c156627db7634d395c380c0a9589130f603307fc'
URL = f'https://codeload.github.com/John-K/pspdecrypt/zip/{COMMIT}'
WORK = ROOT / 'build/pspdecrypt'
EBOOT = ROOT / 'game_files/PSP_GAME/SYSDIR/EBOOT.BIN'
OUT = ROOT / 'build/EBOOT.dec'

MAIN = r'''// Decrypts one PRX/EBOOT with pspdecrypt's PRX decrypter (written by PSPRecomp's ctw profile setup).
// libkirk is C (compiled as C); its headers are included here first with C linkage, then the decrypter itself.
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


def main():
    if not EBOOT.is_file():
        raise SystemExit(f'{EBOOT} is missing: install your game files first')
    src = WORK / f'pspdecrypt-{COMMIT}'
    if not (src / 'PrxDecrypter.cpp').is_file():
        print('Downloading pspdecrypt', COMMIT[:10], flush=True)
        with urllib.request.urlopen(urllib.request.Request(URL, headers={'User-Agent': 'psprecomp-setup'}), timeout=120) as r:
            data = r.read()
        shutil.rmtree(WORK, ignore_errors=True)
        WORK.mkdir(parents=True)
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            top = z.namelist()[0].split('/')[0]
            z.extractall(WORK)
        (WORK / top).rename(src)
    exe = WORK / 'prxdecrypt.exe'
    if not exe.is_file():
        (src / 'prxdecrypt_main.cpp').write_text(MAIN, encoding='utf-8')
        kirk = [f'libkirk/{p.name}' for p in sorted((src / 'libkirk').glob('*.c'))]
        if shutil.which('clang') and shutil.which('clang++'):
            # llvm-mingw (the launcher ZIP's compiler): C files as C, then the wrapper, linked statically.
            objs = []
            for k in kirk:
                obj = WORK / (Path(k).stem + '.o')
                subprocess.run(['clang', '-c', '-O2', '-w', '-o', str(obj), k], cwd=src, check=True)
                objs.append(str(obj))
            subprocess.run(['clang++', '-O2', '-std=c++17', '-w', '-static', '-o', str(exe), 'prxdecrypt_main.cpp'] + objs,
                           cwd=src, check=True)
        elif shutil.which('cl'):
            objdir = '/Fo:' + str(WORK) + os.sep
            subprocess.run(['cl', '/nologo', '/c', '/O2', '/TC', '/w', objdir] + kirk, cwd=src, check=True)
            objs = [str(WORK / (Path(k).stem + '.obj')) for k in kirk]
            subprocess.run(['cl', '/nologo', '/O2', '/EHsc', '/std:c++17', '/w', f'/Fe:{exe}', objdir, 'prxdecrypt_main.cpp'] + objs,
                           cwd=src, check=True)
        else:
            raise SystemExit('No C++ compiler found: run this from Setup / the launcher')
    OUT.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(exe), str(EBOOT), str(OUT)], check=True)


if __name__ == '__main__':
    sys.exit(main())
