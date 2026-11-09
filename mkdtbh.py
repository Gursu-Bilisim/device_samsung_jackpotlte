#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Gursu Bilisim
# SPDX-License-Identifier: Apache-2.0
#
# Builds a Samsung "DTBH" device tree image (the format the jackpotlte
# bootloader reads from the boot image's dt field, mkbootimg --dt).
#
# Format (little endian), as written by hardware/samsung/dtbhtool:
#   "DTBH", version (2), entry count
#   entries of 8 x u32: chip, platform, subtype, hw_rev, hw_rev_end,
#                       offset, size (page aligned), space (0x20)
#   u32 0 (end marker), zero padding to a page boundary,
#   then every dtb padded to a page boundary.
#
# Differences from hardware/samsung/dtbhtool:
# - platform/subtype codes are read from the command line instead of a
#   compile-time header (the shared header carries the "k3g" codes);
# - the header size is computed once, so more than four entries no
#   longer place the blobs at an offset different from the one recorded;
# - only the dtb files passed on the command line are used, in the order
#   of (chip, hw_rev), so other regional variants cannot shadow a revision.

import argparse
import struct
import sys

FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE, FDT_END_NODE, FDT_PROP, FDT_NOP, FDT_END = 1, 2, 3, 4, 9


def root_props(blob):
    magic, totalsize, off_struct, off_strings = struct.unpack_from(">IIII", blob, 0)
    if magic != FDT_MAGIC:
        raise ValueError("not a flattened device tree")
    pos, depth, props = off_struct, 0, {}
    while True:
        (tag,) = struct.unpack_from(">I", blob, pos)
        pos += 4
        if tag == FDT_BEGIN_NODE:
            end = blob.index(b"\0", pos)
            pos = (end + 4) & ~3
            depth += 1
        elif tag == FDT_END_NODE:
            depth -= 1
            if depth == 0:
                return props, totalsize
        elif tag == FDT_PROP:
            length, nameoff = struct.unpack_from(">II", blob, pos)
            pos += 8
            if depth == 1:
                name_end = blob.index(b"\0", off_strings + nameoff)
                name = blob[off_strings + nameoff:name_end].decode()
                props[name] = blob[pos:pos + length]
            pos = (pos + length + 3) & ~3
        elif tag == FDT_NOP:
            continue
        elif tag == FDT_END:
            return props, totalsize
        else:
            raise ValueError("bad fdt tag %d" % tag)


def u32_prop(props, name, path):
    if name not in props or len(props[name]) < 4:
        raise SystemExit("%s: missing %s" % (path, name))
    return struct.unpack(">I", props[name][:4])[0]


def str_prop(props, name):
    return props.get(name, b"").split(b"\0")[0].decode()


def align(value, page):
    return (value + page - 1) // page * page


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("-s", "--pagesize", type=int, default=2048)
    ap.add_argument("--platform", default="android")
    ap.add_argument("--subtype", default="samsung")
    ap.add_argument("--platform-code", type=lambda v: int(v, 0), default=0x50A6)
    ap.add_argument("--subtype-code", type=lambda v: int(v, 0), default=0x217584DA)
    ap.add_argument("dtbs", nargs="+")
    args = ap.parse_args()

    entries = []
    for path in args.dtbs:
        with open(path, "rb") as f:
            data = f.read()
        props, totalsize = root_props(data)
        data = data[:totalsize]
        platform, subtype = str_prop(props, "model_info-platform"), str_prop(props, "model_info-subtype")
        if platform != args.platform or subtype != args.subtype:
            raise SystemExit("%s: model_info platform/subtype is %s/%s, expected %s/%s"
                             % (path, platform, subtype, args.platform, args.subtype))
        entries.append({
            "chip": u32_prop(props, "model_info-chip", path),
            "hw_rev": u32_prop(props, "model_info-hw_rev", path),
            "hw_rev_end": u32_prop(props, "model_info-hw_rev_end", path),
            "data": data,
            "path": path,
        })

    entries.sort(key=lambda e: (e["chip"], e["hw_rev"]))
    seen = {}
    for e in entries:
        key = (e["chip"], e["hw_rev"])
        if key in seen:
            raise SystemExit("%s and %s share chip %d hw_rev %d"
                             % (seen[key], e["path"], key[0], key[1]))
        seen[key] = e["path"]

    page = args.pagesize
    header_size = align(12 + 32 * len(entries) + 4, page)
    offset = header_size
    out = bytearray(struct.pack("<4sII", b"DTBH", 2, len(entries)))
    for e in entries:
        size = align(len(e["data"]), page)
        out += struct.pack("<8I", e["chip"], args.platform_code, args.subtype_code,
                           e["hw_rev"], e["hw_rev_end"], offset, size, 0x20)
        e["offset"], e["size"] = offset, size
        offset += size
    out += b"\0" * (header_size - len(out))
    for e in entries:
        out += e["data"] + b"\0" * (e["size"] - len(e["data"]))

    with open(args.output, "wb") as f:
        f.write(out)
    for e in entries:
        print("dtbh: chip %d rev %d-%d offset %d size %d %s"
              % (e["chip"], e["hw_rev"], e["hw_rev_end"], e["offset"], e["size"], e["path"]),
              file=sys.stderr)


if __name__ == "__main__":
    main()
