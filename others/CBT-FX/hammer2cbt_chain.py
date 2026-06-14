# Frequency table
CH2freqs = [44,156,262,363,457,547,631,710,786,854,923,986,1046,1102,1155,1205,1253,1297,1339,1379,1417,1452,1486,1517,1546,1575,1602,1627,1650,1673,1694,1714,1732,1750,1767,1783,1798,1812,1825,1837,1849,1860,1871,1881,1890,1899,1907,1915,1923,1930,1936,1943,1949,1954,1959,1964,1969,1974,1978,1982,1985,1988,1992,1995,1998,2001,2004,2006,2009,2011,2013,2015]

"""
FX HAMMER DATA LOCATIONS:

Header:
SFX Priority:   0x200 + fxnum
CH Used:        0x300 + fxnum (0x30 = CH2 used, 0x03 = CH4 used)

SFX data:       0x400 + (fxnum * 256)
- Frame duration
- CH2 pan       NR51 format
- CH2 vol       NR22 format
- CH2 duty      NR21 format
- CH2 note      Note = (this - 0x40) / 2
- CH4 pan       NR51 format
- CH4 vol       NR42 format
- CH4 freq      NR43 format

WARNING: FX Hammer pan values are inverted
"""

import argparse
import os
import sys
import textwrap


def parse_fxnum(value):
    """Parse SFX numbers as decimal, 0x-prefixed hex, or bare hex like 0A/0B."""
    value = str(value).strip()
    if value.lower().startswith("0x"):
        return int(value, 16)
    if any(c in value.upper() for c in "ABCDEF"):
        return int(value, 16)
    return int(value, 10)


# Taken from some website, ikik
def swapbits(n, p1, p2):
    bit1 = (n >> p1) & 1
    bit2 = (n >> p2) & 1
    x = (bit1 ^ bit2)
    x = (x << p1) | (x << p2)
    result = n ^ x
    return result


def array_to_hex(a):
    b = []
    for i in range(0, len(a)):
        b.append("0x%0.2X" % a[i])

    b = str(b).replace("'", "").replace(" ", "")[1:-1]
    return '\n'.join(textwrap.wrap(b, 45))


def sanitize_symbol(name):
    """Make a C-friendly symbol name."""
    clean = []
    for ch in name:
        if ch.isalnum() or ch == "_":
            clean.append(ch)
        else:
            clean.append("_")
    name = "".join(clean)
    if not name:
        name = "SFX"
    if name[0].isdigit():
        name = "SFX_" + name
    return name


def normalize_out_dir(out_dir):
    if out_dir == ".":
        out_dir = ""
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    return out_dir


def write_c_and_h(out_dir, filename, fxh_buf, fxh_len, fxh_pry, ch_used_str, header, sgb):
    filename = sanitize_symbol(filename)
    out_dir = normalize_out_dir(out_dir)

    info = """/*

    {filename}

    Sound Effect File.

    Info:
        Length          :   {length}
        Priority        :   {priority}
        Channels used   :   {channels}
        SGB Support     :   {sgb_support}""".format(
        filename=filename,
        length=fxh_len,
        priority=fxh_pry,
        channels=ch_used_str,
        sgb_support=["No", "Yes"][(header & 64) >> 6],
    )

    if sgb:
        info += """
        SGB SFX Table   :   {tab}
        SGB SFX ID      :   {sid}
        SGB SFX Pitch   :   {pitch}
        SGB SFX Volume  :   {vol}""".format(
            tab=sgb[0], sid=sgb[1], pitch=sgb[2], vol=sgb[3]
        )

    info += """
*/

"""

    c_path = os.path.join(out_dir, filename + ".c")
    h_path = os.path.join(out_dir, filename + ".h")

    with open(c_path, "w") as Cfile:
        Cfile.write(info)
        Cfile.write("const unsigned char " + filename + "[] = {\n")
        Cfile.write(array_to_hex(fxh_buf))
        Cfile.write("\n};")

    with open(h_path, "w") as Hfile:
        guard = "__" + filename + "_h_INCLUDE"
        Hfile.write(info)
        Hfile.write("#ifndef " + guard + "\n")
        Hfile.write("#define " + guard + "\n")
        Hfile.write("#define CBTFX_PLAY_" + filename + " CBTFX_init(&" + filename + "[0])\n")
        Hfile.write("extern const unsigned char " + filename + "[];\n")
        Hfile.write("#endif")

    print(filename + ".c succesfully written.")
    print(filename + " Size: " + str(len(fxh_buf) + 2) + " bytes.\n")
    return len(fxh_buf) + 2


def fxh_get(fxsav):
    b = fxsav.read(1)
    if not b:
        raise EOFError("Unexpected end of file while reading FX Hammer save")
    return int.from_bytes(b, "big")


def get_fx_info(fxsav, fxnum):
    """Return priority, raw FX Hammer channel mask, CBTFX channel mask, channel string."""
    fxsav.seek(0x200 + fxnum)
    fxh_pry = fxh_get(fxsav)

    fxsav.seek(0x300 + fxnum)
    raw_chu = fxh_get(fxsav)

    cbt_chu = 0
    names = []
    if raw_chu & 0x30:  # FX Hammer CH2 / Duty channel 2
        cbt_chu |= 0x80
        names.append("Duty channel 2")
    if raw_chu & 0x03:  # FX Hammer CH4 / Noise channel
        cbt_chu |= 0x20
        names.append("Noise channel")

    if not names:
        ch_used_str = "None"
    else:
        ch_used_str = " & ".join(names)

    return fxh_pry, raw_chu, cbt_chu, ch_used_str


def read_fxh_frames(fxsav, fxnum):
    """Read up to 32 FX Hammer rows for one SFX."""
    rows = []
    fxsav.seek(0x400 + (fxnum * 256))

    for f in range(32):
        temp_buf = [0] * 8
        temp_buf[0] = fxh_get(fxsav)  # frame length
        if temp_buf[0] == 0:
            break
        temp_buf[1] = fxh_get(fxsav)          # CH2 pan
        temp_buf[2] = fxh_get(fxsav) & 0xf0   # CH2 vol
        temp_buf[3] = fxh_get(fxsav)          # CH2 duty
        temp_buf[4] = fxh_get(fxsav)          # CH2 note
        temp_buf[5] = fxh_get(fxsav)          # CH4 pan
        temp_buf[6] = fxh_get(fxsav)          # CH4 vol
        temp_buf[7] = fxh_get(fxsav)          # CH4 freq
        rows.append(temp_buf)

    return rows


def encode_rows(rows_with_masks, output_chu, args, start_pan=-1):
    """Encode rows to CBTFX format.

    rows_with_masks is a list of (row, source_cbt_channel_mask).
    output_chu decides the global channels used by the exported SFX.
    """
    fxh_buf = []
    bk_pan = start_pan

    for temp_buf, source_chu in rows_with_masks:
        row = list(temp_buf)

        # If this chained segment didn't originally use a channel that the final
        # combined SFX uses, silence that channel for this row. This keeps the
        # CBTFX row layout valid when 0A and 0B use different channels.
        if not (source_chu & 0x80):  # no Duty channel 2 in source
            row[1] = 0
            row[2] = 0
            row[3] = 0
            row[4] = 0x40  # safe note index 0
        if not (source_chu & 0x20):  # no Noise channel in source
            row[5] = 0
            row[6] = 0
            row[7] = 0

        # Length and frame pan
        pan = row[1] | row[5]
        if not args.fxinv:
            pan = swapbits(pan, 7, 3)
            pan = swapbits(pan, 5, 1)
        if args.fxmono:
            pan = 0xaa
        if pan != bk_pan:
            fxh_buf.append((row[0] - 1) | 0x80)
            fxh_buf.append(pan)  # NR51 values
            bk_pan = pan
        else:
            fxh_buf.append(row[0] - 1)

        # CH2 Duty (NR21)
        if output_chu & 0x80:
            fxh_buf.append(row[3])

        # Frame Volume, high nibble CH2 + low nibble CH4
        fxh_buf.append(row[2] | (row[6] >> 4))

        # CH2 Frequency (NR23/NR24)
        if output_chu & 0x80:
            note_index = (row[4] - 0x40) >> 1
            if note_index < 0:
                note_index = 0
            if note_index >= len(CH2freqs):
                note_index = len(CH2freqs) - 1
            freq = CH2freqs[note_index]
            fxh_buf.append(freq & 0xff)
            fxh_buf.append((freq >> 8) | 0x80)

        # CH4 Freq (NR43)
        if output_chu & 0x20:
            fxh_buf.append(row[7])

    return fxh_buf


def add_sgb_header(fxh_buf, sgb):
    if not sgb:
        return
    fxh_buf.append(65)  # Command byte ((SGB_SOUND << 3) | 1)
    if sgb[0] == "A":
        fxh_buf.append(int(sgb[1]))
        fxh_buf.append(0)
    elif sgb[0] == "B":
        fxh_buf.append(0)
        fxh_buf.append(int(sgb[1]))
    else:
        raise ValueError("SGB FX_TAB must be A or B")

    if sgb[0] == "A":
        fxh_buf.append(int(sgb[2]) | (int(sgb[3]) << 2))
    else:
        fxh_buf.append((int(sgb[2]) << 4) | (int(sgb[3]) << 6))
    fxh_buf.append(0)  # Music Score Code (Unused)


def export_single(fxsav, fxnum, filename, out_dir, args, sgb):
    fxh_pry, raw_chu, fxh_chu, ch_used_str = get_fx_info(fxsav, fxnum)
    if fxh_chu == 0:
        sys.exit("ERROR: SFX #" + (("{0:X}").format(fxnum)).zfill(2) + " is empty, aborting conversion.")

    rows = read_fxh_frames(fxsav, fxnum)
    fxh_len = len(rows)
    if fxh_len + 1 > 255:
        sys.exit("ERROR: SFX is too long for CBTFX length byte.")

    header = fxh_chu | fxh_pry
    if sgb:
        header |= 64

    fxh_buf = [header, 0]
    add_sgb_header(fxh_buf, sgb)
    fxh_buf.extend(encode_rows([(r, fxh_chu) for r in rows], fxh_chu, args))
    fxh_buf[1] = fxh_len + 1

    return write_c_and_h(out_dir, filename, fxh_buf, fxh_len, fxh_pry, ch_used_str, header, sgb)


def export_chain(fxsav, fxnums, filename, out_dir, args, sgb):
    chain = []
    output_chu = 0
    fxh_pry = 0

    for fxnum in fxnums:
        pry, raw_chu, cbt_chu, ch_used = get_fx_info(fxsav, fxnum)
        if cbt_chu == 0:
            sys.exit("ERROR: SFX #" + (("{0:X}").format(fxnum)).zfill(2) + " is empty, aborting conversion.")
        output_chu |= cbt_chu
        fxh_pry = max(fxh_pry, pry)
        rows = read_fxh_frames(fxsav, fxnum)
        for row in rows:
            chain.append((row, cbt_chu))

    fxh_len = len(chain)
    if fxh_len + 1 > 255:
        sys.exit("ERROR: Chained SFX has " + str(fxh_len) + " rows; max supported is 254 rows.")

    names = []
    if output_chu & 0x80:
        names.append("Duty channel 2")
    if output_chu & 0x20:
        names.append("Noise channel")
    ch_used_str = " & ".join(names)

    header = output_chu | fxh_pry
    if sgb:
        header |= 64

    fxh_buf = [header, 0]
    add_sgb_header(fxh_buf, sgb)
    fxh_buf.extend(encode_rows(chain, output_chu, args))
    fxh_buf[1] = fxh_len + 1

    return write_c_and_h(out_dir, filename, fxh_buf, fxh_len, fxh_pry, ch_used_str, header, sgb)


parser = argparse.ArgumentParser()
parser.add_argument("fxsav", help="FX Hammer .sav file (Normally called 'hammered.sav').")
parser.add_argument("fxnum", help="Index of the desired SFX to export. Accepts decimal, 0x hex, or bare hex like 0A.")
parser.add_argument("out", help="Folder where .c and .h files will be saved.")
parser.add_argument("--fxammo", help="Number of SFX to export (kept compatible with the original script: starts at fxnum and exports this many entries).")
parser.add_argument("--fxnamelist", help="Text file with all the names for the SFX, each on one line (SFX names shouldn't have spaces), you can add 4 values after the SFX name to define SGB sound values (See --sgb parameter).")
parser.add_argument("--sgb", help="Add Super Game Boy support.", nargs=4, metavar=("FX_TAB", "FX_ID", "FX_PITCH", "FX_VOL"))
parser.add_argument("--fxinv", help="Invert pan values for SFX (FX Hammer has them inverted by default, without this flag, the panning will be corrected)", action="store_true")
parser.add_argument("--fxmono", help="Avoid all panning writes and store as mono.", action="store_true")
parser.add_argument("--fxchain", help="Comma-separated SFX indexes to concatenate into one exported SFX, for example: 0x0A,0x0B or 0A,0B. When used, fxnum is ignored except for compatibility.")
parser.add_argument("--fxchain-name", help="Output symbol/file name for --fxchain. Default: SFX_CHAIN_<indexes>.")

args = parser.parse_args()

sgb = []
if args.sgb:
    sgb = list(args.sgb)

size_count = 0

with open(args.fxsav, "rb") as fxsav:
    if args.fxchain:
        fxnums = [parse_fxnum(x) for x in args.fxchain.replace(";", ",").split(",") if x.strip()]
        if not fxnums:
            sys.exit("ERROR: --fxchain was provided, but no SFX indexes were found.")
        if args.fxchain_name:
            filename = args.fxchain_name
        else:
            filename = "SFX_CHAIN_" + "_".join((("{0:X}").format(n)).zfill(2) for n in fxnums)
        size_count += export_chain(fxsav, fxnums, filename, args.out, args, sgb)
        print("Final size: " + str(size_count) + " bytes.\n")
        sys.exit(0)

    loop = 1
    if args.fxammo:
        loop = int(args.fxammo)

    fxnamelist = None
    if args.fxnamelist:
        fxnamelist = open(args.fxnamelist, "r")

    try:
        start_fxnum = parse_fxnum(args.fxnum)
        for n in range(start_fxnum, start_fxnum + loop):
            filename = "SFX_" + (("{0:X}").format(n)).zfill(2)
            row_sgb = list(sgb)

            if fxnamelist:
                txt = fxnamelist.readline().replace("\n", "").split(" ")
                if txt and txt[0]:
                    filename = txt[0]
                if len(txt) > 1:
                    row_sgb = [txt[1], txt[2], txt[3], txt[4]]

            size_count += export_single(fxsav, n, filename, args.out, args, row_sgb)
    finally:
        if fxnamelist:
            fxnamelist.close()

if args.fxammo:
    print("Final size: " + str(size_count) + " bytes.\n")
