"""Decoder for VTS (Melown) mesh tiles, format version 3 (as in vts-browser-js submesh.js)."""
import gzip, struct

def _delta(d, i):
    v = d[i]
    if v & 0x80:
        v = (v & 0x7f) | (d[i + 1] << 7); i += 2
    else:
        i += 1
    return (-((v >> 1) + 1) if v & 1 else v >> 1), i

def _word(d, i):
    v = d[i]
    if v & 0x80:
        return (v & 0x7f) | (d[i + 1] << 7), i + 2
    return v, i + 1

def decode(raw):
    d = gzip.decompress(raw) if raw[:2] == b"\x1f\x8b" else raw
    assert d[:2] == b"ME", d[:2]
    ver, = struct.unpack_from("<H", d, 2); i = 4
    if ver >= 3: i += 8
    n, = struct.unpack_from("<H", d, i); i += 2
    subs = []
    for _ in range(n):
        flags, sref, layer = struct.unpack_from("<BBH", d, i); i += 4
        bb = struct.unpack_from("<6d", d, i); i += 48
        lo, hi = bb[:3], bb[3:]
        c = [(lo[k] + hi[k]) / 2 for k in range(3)]
        scale = max(hi[k] - lo[k] for k in range(3))
        nv, q = struct.unpack_from("<HH", d, i); i += 4
        x = y = z = 0; verts = []
        for _ in range(nv):
            a, i = _delta(d, i); x += a
            a, i = _delta(d, i); y += a
            a, i = _delta(d, i); z += a
            verts.append((x / q * scale + c[0], y / q * scale + c[1], z / q * scale + c[2]))
        ext = None
        if flags & 2:
            q, = struct.unpack_from("<H", d, i); i += 2
            x = y = 0; ext = []
            for _ in range(nv):
                a, i = _delta(d, i); x += a
                a, i = _delta(d, i); y += a
                ext.append((x / q, 1 - y / q))
        uvs = None
        if flags & 1:
            nu, qu, qv = struct.unpack_from("<HHH", d, i); i += 6
            x = y = 0; uvs = []
            for _ in range(nu):
                a, i = _delta(d, i); x += a
                a, i = _delta(d, i); y += a
                uvs.append((x / qu, 1 - y / qv))
        nf, = struct.unpack_from("<H", d, i); i += 2
        faces = []; high = 0
        for _ in range(nf):
            f = []
            for _ in range(3):
                w, i = _word(d, i); f.append(high - w)
                if w == 0: high += 1
            faces.append(f)
        tfaces = None
        if flags & 1:
            tfaces = []; high = 0
            for _ in range(nf):
                f = []
                for _ in range(3):
                    w, i = _word(d, i); f.append(high - w)
                    if w == 0: high += 1
                tfaces.append(f)
        subs.append(dict(flags=flags, sref=sref, layer=layer, lo=lo, hi=hi, verts=verts, ext=ext, uvs=uvs, faces=faces, tfaces=tfaces))
    assert i == len(d), (i, len(d))
    return ver, subs
