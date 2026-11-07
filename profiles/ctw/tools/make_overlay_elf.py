"""make_overlay_elf.py EBOOT.dec MEMDUMP DUMP_BASE OUT.elf START END [START END ...]

CTW copies code overlays (stored compressed in 100MEG_A.BIN) into the gap between the EBOOT image and its bss
(0x08B8744C..0x08BC7FA0) and calls into them. To recompile them statically, a run with
PSPRECOMP_DUMP_ON_STOP saves guest memory; this script appends each captured range as an extra PT_LOAD
(RWX) to a copy of the EBOOT so psp_recomp --auto analyzes and translates it with the rest of the game."""
import struct
import sys


def main():
    elf, dump, dump_base, out = sys.argv[1], sys.argv[2], int(sys.argv[3], 0), sys.argv[4]
    ranges = [(int(sys.argv[i], 0), int(sys.argv[i + 1], 0)) for i in range(5, len(sys.argv), 2)]
    d = bytearray(open(elf, 'rb').read())
    m = open(dump, 'rb').read()
    phoff, = struct.unpack_from('<I', d, 0x1C)
    phentsize, phnum = struct.unpack_from('<HH', d, 0x2A)
    first_data = min(struct.unpack_from('<8I', d, phoff + i * phentsize)[1] for i in range(phnum))
    if phoff + (phnum + len(ranges)) * phentsize > first_data:
        raise SystemExit('no room for extra program headers')
    for k, (start, end) in enumerate(ranges):
        blob = m[start - dump_base:end - dump_base]
        while len(d) % 16:
            d.append(0)
        off = len(d)
        d += blob
        struct.pack_into('<8I', d, phoff + (phnum + k) * phentsize, 1, off, start, start, len(blob), len(blob), 7, 16)
        print(f'segment 0x{start:08X}-0x{end:08X} at file 0x{off:X}')
    struct.pack_into('<H', d, 0x2C, phnum + len(ranges))
    open(out, 'wb').write(d)


if __name__ == '__main__':
    main()
