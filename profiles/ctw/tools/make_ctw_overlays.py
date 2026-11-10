"""make_ctw_overlays.py OVERLAYS.PAK EBOOT.dec OUT_DIR

Chinatown Wars keeps its code overlays in USRDIR/WAD/OVERLAY/.../OVERLAYS.PAK:
  "PK02", 32 bytes 0xFF, u32 count at 0x24, u32 sector size, u16 sector table[count+1] at 0x2C; entry i data at (t[i]+1)*0x800,
  each entry "AS01" + u32 compressed size + u32 size + zlib stream; the payload is a fixed-address
  ELF (e_type 0xFFA8) with one RWX PT_LOAD in one of two windows (0x08B87460 or 0x08BB2480).
This writes each distinct overlay as an ET_EXEC ELF (ovl_NNN.elf) for `psp_recomp --overlay`, plus
seeds.txt: every EBOOT JAL target and EBOOT data word that points into the overlay windows (the
overlays' entry points are only referenced from the main image)."""
import hashlib
import struct
import sys
import zlib
from pathlib import Path

WINDOW_LOW, WINDOW_HIGH = 0x08B8744C, 0x08BC7FA0


def pak_entries(data):
    if data[:4] != b'20KP':  # 'PK02' stored as a little-endian u32
        raise SystemExit('not a PK02 archive')
    count, = struct.unpack_from('<I', data, 0x24)  # 0x04..0x23 are 0xFF
    table = struct.unpack_from('<%dH' % (count + 1), data, 0x2C)
    for index in range(1, count):
        offset = (table[index] + 1) * 0x800
        magic, packed, size = struct.unpack_from('<4sII', data, offset)
        if magic != b'10SA':
            continue
        payload = zlib.decompressobj().decompress(data[offset + 12:offset + 12 + packed])
        if len(payload) != size:
            print(f'entry {index}: size {len(payload)} != {size}')
        yield index, payload


def eboot_seeds(path):
    elf = open(path, 'rb').read()
    phoff, = struct.unpack_from('<I', elf, 0x1C)
    phnum, = struct.unpack_from('<H', elf, 0x2C)
    seeds = set()
    for k in range(phnum):
        ptype, off, vaddr, _, filesz, _, _, _ = struct.unpack_from('<8I', elf, phoff + 32 * k)
        if ptype != 1:
            continue
        for at in range(0, filesz - 3, 4):
            word, = struct.unpack_from('<I', elf, off + at)
            if word >> 26 == 3:  # jal
                target = ((vaddr + at + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
                if WINDOW_LOW <= target < WINDOW_HIGH:
                    seeds.add(target)
            elif WINDOW_LOW <= word < WINDOW_HIGH and word % 4 == 0:
                seeds.add(word)
    return seeds


def main():
    pak, eboot, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    seen = {}
    windows = {}
    for index, payload in pak_entries(pak.read_bytes()):
        if payload[:4] != b'\x7fELF':
            continue
        digest = hashlib.sha1(payload).hexdigest()
        if digest in seen:
            print(f'entry {index}: same as {seen[digest]}')
            continue
        seen[digest] = index
        elf = bytearray(payload)
        struct.pack_into('<H', elf, 16, 2)  # ET_EXEC
        phoff, = struct.unpack_from('<I', elf, 0x1C)
        _, _, vaddr, _, filesz, memsz, _, _ = struct.unpack_from('<8I', elf, phoff)
        windows.setdefault(vaddr, []).append(vaddr + memsz)
        (out / f'ovl_{index:03d}.elf').write_bytes(elf)
    for base, ends in sorted(windows.items()):
        print(f'window 0x{base:08X}: {len(ends)} overlays, end max 0x{max(ends):08X}')
    seeds = sorted(eboot_seeds(eboot))
    (out / 'seeds.txt').write_text(''.join(f'{s:08X}\n' for s in seeds))
    # Per-overlay seeds: the EBOOT references plus the overlay's own function starts. An overlay starts
    # with a 16-byte header (count, init function, flag, second function) and its code at base + 0x10;
    # several functions there are only reached through the game's overlay loader or tables.
    for path in sorted(out.glob('ovl_*.elf')):
        own = set(seeds) | overlay_function_starts(path.read_bytes())
        path.with_suffix('.seeds').write_text(''.join(f'{s:08X}\n' for s in sorted(own)))
    back = sorted(overlay_to_eboot_seeds(eboot, sorted(out.glob('ovl_*.elf'))))
    (out / 'eboot_seeds.txt').write_text(''.join(f'{s:08X}\n' for s in back))
    print(f'{len(seen)} overlays, {len(seeds)} overlay seeds, {len(back)} EBOOT seeds from overlays')


def overlay_function_starts(elf):
    ph, = struct.unpack_from('<I', elf, 0x1C)
    _, o, base, _, size, _, _, _ = struct.unpack_from('<8I', elf, ph)
    words = struct.unpack_from('<%dI' % (size // 4), elf, o)
    starts = {base + 0x10}
    for header_word in (words[1], words[3]):
        if base <= header_word < base + size and header_word % 4 == 0:
            starts.add(header_word)
    for i in range(4, len(words) - 2):
        if words[i] == 0x03E00008:  # jr ra; the function after the delay slot (skipping nop padding)
            j = i + 2
            while j < len(words) and words[j] == 0:
                j += 1
            if j < len(words) and (words[j] >> 16) == 0x27BD and words[j] & 0x8000:
                starts.add(base + 4 * j)
            elif j == i + 2 and j < len(words):
                starts.add(base + 4 * j)
        elif (words[i] >> 16) == 0x27BD and words[i] & 0x8000 and words[i - 1] == 0:
            starts.add(base + 4 * i)  # addiu sp, sp, -N after padding
        elif base <= words[i] < base + size and words[i] % 4 == 0:
            target = (words[i] - base) // 4
            if target >= 4 and ((words[target] >> 16) == 0x27BD and words[target] & 0x8000):
                starts.add(words[i])  # data pointer to a prologue (vtables, callback tables)
    return starts


def overlay_to_eboot_seeds(eboot, elves):
    """EBOOT code reached only from overlays: JAL targets, LUI/ADDIU (ORI) pointer pairs, and data words
    that land on a function start (word after `jr ra` + delay slot, or an `addiu sp, sp, -N`)."""
    image = open(eboot, 'rb').read()
    phoff, = struct.unpack_from('<I', image, 0x1C)
    _, off, vaddr, _, filesz, _, _, _ = struct.unpack_from('<8I', image, phoff)
    low, high = vaddr, vaddr + filesz

    def word(address):
        return struct.unpack_from('<I', image, off + address - vaddr)[0]

    def function_start(address):
        if not (low + 8 <= address < high) or address % 4:
            return False
        first = word(address)
        return word(address - 8) == 0x03E00008 or (first >> 16) == 0x27BD and (first & 0x8000)

    seeds = set()
    for path in elves:
        elf = path.read_bytes()
        ph, = struct.unpack_from('<I', elf, 0x1C)
        _, o, base, _, size, _, _, _ = struct.unpack_from('<8I', elf, ph)
        words = struct.unpack_from('<%dI' % (size // 4), elf, o)
        for i, w in enumerate(words):
            if w >> 26 == 3:
                target = ((base + 4 * i + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
                if low <= target < high:
                    seeds.add(target)
            elif function_start(w):
                seeds.add(w)
            if w >> 26 == 0x0F:  # lui rt, hi
                rt, hi = (w >> 16) & 31, w & 0xFFFF
                for w2 in words[i + 1:i + 9]:
                    op = w2 >> 26
                    if op in (0x09, 0x0D) and (w2 >> 21) & 31 == rt:
                        lo = w2 & 0xFFFF
                        value = ((hi << 16) + (lo - 0x10000 if op == 0x09 and lo & 0x8000 else lo)) & 0xFFFFFFFF
                        if function_start(value):
                            seeds.add(value)
                        break
    return seeds


if __name__ == '__main__':
    main()
