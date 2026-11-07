"""FFmpeg 7.1 shared libraries (import .lib + DLLs) for CTWNative, matching the headers in third_party/ffmpeg/include.

Downloads the pinned Gyan.dev build (SHA-256 checked) and copies the five libraries the game uses into
third_party/ffmpeg/lib and bin (both ignored by Git).
"""
import hashlib
import io
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
URL = 'https://github.com/GyanD/codexffmpeg/releases/download/7.1/ffmpeg-7.1-full_build-shared.zip'
SHA256 = 'fedf38dbdd00fa189da06afb0abba69a19312412e05fa2e8e89b3c06aafebcdc'
LIBS = ('avcodec', 'avformat', 'avutil', 'swresample', 'swscale')


def main():
    dest = ROOT / 'third_party/ffmpeg'
    print('Downloading FFmpeg 7.1 (about 70 MB)', flush=True)
    with urllib.request.urlopen(urllib.request.Request(URL, headers={'User-Agent': 'psprecomp-setup'}), timeout=300) as r:
        data = r.read()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise SystemExit('FFmpeg download damaged or changed (SHA-256 mismatch); run Setup again')
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for name in z.namelist():
            parts = name.split('/')
            if len(parts) == 3 and parts[1] in ('bin', 'lib') and any(parts[2].startswith(lib + ('-' if parts[1] == 'bin' else '.')) for lib in LIBS):
                if parts[1] == 'bin' and not parts[2].endswith('.dll'):
                    continue
                out = dest / parts[1] / parts[2]
                out.parent.mkdir(parents=True, exist_ok=True)
                out.write_bytes(z.read(name))
                print(' ', out.relative_to(ROOT))


if __name__ == '__main__':
    main()
