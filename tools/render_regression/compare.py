#!/usr/bin/env python3
"""Render-regression perceptual-image comparator (TEST-01).

Self-contained: stdlib only (zlib), so it runs on any Python 3 without pip installs.
Decodes 8-bit RGB/RGBA non-interlaced PNGs, computes a perceptual distance against a
golden image, and reports pass/fail with an optional diff heat-map.

Pass criteria (a shot passes only if BOTH hold):
  mean_abs_error      <= tol_mae      (default 2.0, units: 0-255)
  hot_pixel_fraction  <= tol_hot      (default 0.002 = 0.2% of pixels)
where a "hot" pixel is one whose max per-channel delta exceeds hot_threshold (default 16).

Usage:
  compare.py --golden GOLD.png --candidate CAND.png [--diff OUT.png]
  compare.py --golden-dir golden/ --candidate-dir captured/ [--diff-dir diffs/]
  compare.py --selftest

Exit code: 0 if all compared shots pass, 1 if any fail (or on error). The non-zero exit is
what wires this into CI (TEST-02).

See docs/superpowers/specs/2026-06-03-render-regression-harness.md.
"""
import argparse
import os
import struct
import sys
import zlib

PNG_SIG = b"\x89PNG\r\n\x1a\n"

DEFAULT_TOL_MAE = 2.0
DEFAULT_TOL_HOT = 0.002
DEFAULT_HOT_THRESHOLD = 16


# --------------------------------------------------------------------------- PNG I/O

def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png_rgba(path):
    """Decode an 8-bit, non-interlaced RGB/RGBA PNG to (w, h, bytearray RGBA)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != PNG_SIG:
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    width = height = 0
    color_type = bit_depth = 0
    idat = bytearray()
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length  # length + type + data + crc
        if ctype == b"IHDR":
            width, height, bit_depth, color_type, comp, filt, interlace = struct.unpack(
                ">IIBBBBB", chunk)
            if bit_depth != 8:
                raise ValueError(f"{path}: only 8-bit PNGs supported (got {bit_depth})")
            if color_type not in (2, 6):
                raise ValueError(f"{path}: only RGB/RGBA supported (color_type {color_type})")
            if interlace != 0:
                raise ValueError(f"{path}: interlaced PNGs unsupported")
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    out = bytearray(width * height * 4)
    prev = bytearray(stride)
    rpos = 0
    for y in range(height):
        ftype = raw[rpos]
        rpos += 1
        line = bytearray(raw[rpos:rpos + stride])
        rpos += stride
        # Reconstruct scanline filters (PNG spec 9.2).
        if ftype == 1:  # Sub
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif ftype == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:  # Average
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:  # Paeth
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                c = prev[i - channels] if i >= channels else 0
                line[i] = (line[i] + _paeth(a, prev[i], c)) & 0xFF
        elif ftype != 0:
            raise ValueError(f"{path}: bad filter type {ftype}")
        # Expand to RGBA.
        obase = y * width * 4
        if channels == 4:
            out[obase:obase + stride] = line
        else:
            for x in range(width):
                s = x * 3
                d = obase + x * 4
                out[d] = line[s]
                out[d + 1] = line[s + 1]
                out[d + 2] = line[s + 2]
                out[d + 3] = 255
        prev = line
    return width, height, out


def write_png_rgba(path, width, height, rgba):
    """Encode an 8-bit RGBA PNG (filter 0 per row)."""
    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]

    def chunk(ctype, payload):
        return (struct.pack(">I", len(payload)) + ctype + payload
                + struct.pack(">I", zlib.crc32(ctype + payload) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(PNG_SIG)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


# ----------------------------------------------------------------------- comparison

def compare_images(gold, cand, hot_threshold=DEFAULT_HOT_THRESHOLD, want_diff=False):
    """Return a metrics dict; optionally a diff heat-map (RGBA bytes)."""
    gw, gh, gpx = gold
    cw, ch, cpx = cand
    if (gw, gh) != (cw, ch):
        raise ValueError(f"dimension mismatch: golden {gw}x{gh} vs candidate {cw}x{ch}")
    n = gw * gh
    total = 0
    hot = 0
    diff = bytearray(n * 4) if want_diff else None
    for p in range(n):
        b = p * 4
        dr = abs(gpx[b] - cpx[b])
        dg = abs(gpx[b + 1] - cpx[b + 1])
        db = abs(gpx[b + 2] - cpx[b + 2])
        # luma-weighted channel error
        total += 0.299 * dr + 0.587 * dg + 0.114 * db
        mx = dr if dr > dg else dg
        if db > mx:
            mx = db
        if mx > hot_threshold:
            hot += 1
        if want_diff:
            v = mx if mx <= 255 else 255
            diff[b] = v
            diff[b + 1] = 0 if v else 0
            diff[b + 2] = 255 - v
            diff[b + 3] = 255
    metrics = {
        "width": gw,
        "height": gh,
        "mean_abs_error": total / n if n else 0.0,
        "hot_pixel_fraction": hot / n if n else 0.0,
        "hot_pixels": hot,
    }
    return (metrics, (gw, gh, diff)) if want_diff else (metrics, None)


def passes(metrics, tol_mae=DEFAULT_TOL_MAE, tol_hot=DEFAULT_TOL_HOT):
    return (metrics["mean_abs_error"] <= tol_mae
            and metrics["hot_pixel_fraction"] <= tol_hot)


# ------------------------------------------------------------------------------- CLI

def _compare_one(gold_path, cand_path, diff_path, tol_mae, tol_hot, hot_threshold):
    gold = read_png_rgba(gold_path)
    cand = read_png_rgba(cand_path)
    metrics, diff = compare_images(gold, cand, hot_threshold, want_diff=bool(diff_path))
    ok = passes(metrics, tol_mae, tol_hot)
    if diff_path and not ok:
        os.makedirs(os.path.dirname(diff_path) or ".", exist_ok=True)
        write_png_rgba(diff_path, *diff)
    status = "PASS" if ok else "FAIL"
    print(f"[{status}] {os.path.basename(cand_path)}  "
          f"mae={metrics['mean_abs_error']:.3f}  "
          f"hot={metrics['hot_pixel_fraction']*100:.3f}%")
    return ok


def run_dir(golden_dir, candidate_dir, diff_dir, tol_mae, tol_hot, hot_threshold):
    all_ok = True
    found = False
    for root, _dirs, files in os.walk(golden_dir):
        for name in sorted(files):
            if not name.lower().endswith(".png"):
                continue
            rel = os.path.relpath(os.path.join(root, name), golden_dir)
            cand = os.path.join(candidate_dir, rel)
            if not os.path.exists(cand):
                print(f"[MISS] {rel}  (no candidate capture)")
                all_ok = False
                continue
            found = True
            diff = os.path.join(diff_dir, rel) if diff_dir else None
            all_ok &= _compare_one(os.path.join(root, name), cand, diff,
                                   tol_mae, tol_hot, hot_threshold)
    if not found:
        print("no golden/candidate pairs found")
        return False
    return all_ok


def selftest():
    """Validate the comparator with synthetic images — no GPU required."""
    import tempfile
    w, h = 64, 48
    base = bytearray()
    for y in range(h):
        for x in range(w):
            base += bytes((x * 4 % 256, y * 5 % 256, (x + y) % 256, 255))

    tmp = tempfile.mkdtemp(prefix="rrh_selftest_")
    gold_p = os.path.join(tmp, "gold.png")
    write_png_rgba(gold_p, w, h, base)

    # 1. Round-trip identity: re-decoded image must be byte-identical (encoder/decoder sane).
    rg = read_png_rgba(gold_p)
    assert rg == (w, h, base), "PNG round-trip mismatch"

    # 2. Identical image passes with zero error.
    m, _ = compare_images(rg, rg)
    assert m["mean_abs_error"] == 0.0 and m["hot_pixel_fraction"] == 0.0
    assert passes(m), "identical images must pass"

    # 3. Tiny uniform jitter (+1 per channel) stays under tolerance.
    jitter = bytearray(base)
    for i in range(0, len(jitter), 4):
        jitter[i] = min(255, jitter[i] + 1)
    m, _ = compare_images(rg, (w, h, jitter))
    assert passes(m), f"+1 jitter should pass, got {m}"

    # 4. A bright corrupt block fails (simulates a broken render pass).
    broken = bytearray(base)
    for y in range(0, 10):
        for x in range(0, 10):
            b = (y * w + x) * 4
            broken[b] = 255
            broken[b + 1] = 255
            broken[b + 2] = 255
    m, diff = compare_images(rg, (w, h, broken), want_diff=True)
    assert not passes(m), f"corrupt block should fail, got {m}"
    assert diff[2] is not None

    # 5. Dimension mismatch raises.
    try:
        compare_images(rg, (w, h - 1, base[:(w * (h - 1) * 4)]))
        raise AssertionError("dimension mismatch must raise")
    except ValueError:
        pass

    print("selftest: OK (5 checks passed)")
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description="Render-regression perceptual comparator")
    ap.add_argument("--golden")
    ap.add_argument("--candidate")
    ap.add_argument("--diff")
    ap.add_argument("--golden-dir")
    ap.add_argument("--candidate-dir")
    ap.add_argument("--diff-dir")
    ap.add_argument("--tol-mae", type=float, default=DEFAULT_TOL_MAE)
    ap.add_argument("--tol-hot", type=float, default=DEFAULT_TOL_HOT)
    ap.add_argument("--hot-threshold", type=int, default=DEFAULT_HOT_THRESHOLD)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)

    try:
        if args.selftest:
            return 0 if selftest() else 1
        if args.golden_dir and args.candidate_dir:
            ok = run_dir(args.golden_dir, args.candidate_dir, args.diff_dir,
                         args.tol_mae, args.tol_hot, args.hot_threshold)
            return 0 if ok else 1
        if args.golden and args.candidate:
            ok = _compare_one(args.golden, args.candidate, args.diff,
                              args.tol_mae, args.tol_hot, args.hot_threshold)
            return 0 if ok else 1
    except (ValueError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    ap.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
