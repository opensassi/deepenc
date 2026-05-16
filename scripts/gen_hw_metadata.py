#!/usr/bin/env python3
"""Generate test HW metadata sidecar files (CSV + binary grid).

Produces:
  hw_metadata.csv       — frame-level metadata (version, width, height, per-frame rows)
  hw_grids.bin          — per-frame binary MB partition + MV data

Usage:
  python gen_hw_metadata.py \
    --grid-w 52 --grid-h 30 --num-frames 16 \
    --pattern mixed --output /tmp/hw_test/
"""

import argparse
import os
import random
import struct

PATTERNS = ("uniform", "boundary", "mixed", "random")


def gen_uniform(w, h):
    """All MBs identical: type=0, mv=(0,0)."""
    types = [0] * (w * h)
    mvx = [0] * (w * h)
    mvy = [0] * (w * h)
    return types, mvx, mvy


def gen_boundary(w, h):
    """Left half static, right half moving right."""
    types = [0] * (w * h)
    mvx = [0] * (w * h)
    mvy = [0] * (w * h)
    mid = w // 2
    for y in range(h):
        for x in range(w):
            idx = y * w + x
            if x >= mid:
                types[idx] = 1
                mvx[idx] = 10
                mvy[idx] = 0
    return types, mvx, mvy


def gen_mixed(w, h):
    """Checkerboard partitions, smooth motion gradient."""
    types = [0] * (w * h)
    mvx = [0] * (w * h)
    mvy = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            idx = y * w + x
            types[idx] = (x + y) % 4
            mvx[idx] = (x - w // 2) * 2
            mvy[idx] = (y - h // 2) * 2
    return types, mvx, mvy


def gen_random(w, h):
    """Completely random MB types and MVs."""
    types = [random.randint(0, 7) for _ in range(w * h)]
    mvx = [random.randint(-100, 100) for _ in range(w * h)]
    mvy = [random.randint(-100, 100) for _ in range(w * h)]
    return types, mvx, mvy


GENERATORS = {
    "uniform": gen_uniform,
    "boundary": gen_boundary,
    "mixed": gen_mixed,
    "random": gen_random,
}


def gen_sidecar(output_dir, grid_w, grid_h, num_frames, pattern):
    """Generate CSV + binary sidecar files."""
    os.makedirs(output_dir, exist_ok=True)

    width_px = grid_w * 16
    height_px = grid_h * 16

    csv_path = os.path.join(output_dir, "hw_metadata.csv")
    bin_path = os.path.join(output_dir, "hw_metadata_grids.bin")

    gen_fn = GENERATORS[pattern]
    random.seed(42)

    with open(csv_path, "w") as csv, \
         open(bin_path, "wb") as bin_f:

        # CSV header
        csv.write(f"1,{width_px},{height_px}\n")
        csv.write("poc,frameType,qp,bits,sceneCut,mvComplexity\n")

        for poc in range(num_frames):
            # Frame metadata
            frame_type = 0 if poc == 0 else (2 if poc % 3 == 0 else 1)
            qp = 28 + (poc % 8)
            bits = 50000 + (poc * 15000)
            scene_cut = 1 if (poc == 0 or poc == num_frames // 2) else 0
            mv_complexity = round(random.uniform(0.0, 1.0), 4)

            csv.write(f"{poc},{frame_type},{qp},{bits},{scene_cut},{mv_complexity}\n")

            # Generate MB grid data
            types, mvx, mvy = gen_fn(grid_w, grid_h)

            # Compute grid size: header(4) + types(W*H) + mvx(W*H*2) + mvy(W*H*2)
            grid_bytes = 4 + grid_w * grid_h * (1 + 2 + 2)

            # Write binary frame record (interleaved mvX, mvY per MB)
            bin_f.write(struct.pack("<I", grid_bytes))
            bin_f.write(bytes(types))
            mv_interleaved = [val for pair in zip(mvx, mvy) for val in pair]
            bin_f.write(struct.pack(f"<{len(mv_interleaved)}h", *mv_interleaved))

        actual_bin = os.path.getsize(bin_path)

    print(f"Generated {csv_path}")
    print(f"         {bin_path} ({actual_bin} bytes)")
    print(f"  Grid: {grid_w}x{grid_h} MBs = {width_px}x{height_px} px")
    print(f"  Frames: {num_frames}, Pattern: {pattern}")


def main():
    parser = argparse.ArgumentParser(
        description="Generate HW pre-analysis test metadata sidecar files"
    )
    parser.add_argument("--grid-w", type=int, default=52,
                        help="MB grid width (default: 52 = 832px)")
    parser.add_argument("--grid-h", type=int, default=30,
                        help="MB grid height (default: 30 = 480px)")
    parser.add_argument("--num-frames", type=int, default=16,
                        help="Number of frames (default: 16)")
    parser.add_argument("--pattern", choices=PATTERNS, default="mixed",
                        help="MB data pattern (default: mixed)")
    parser.add_argument("--output", default="/tmp/hw_test",
                        help="Output directory (default: /tmp/hw_test)")
    args = parser.parse_args()

    gen_sidecar(args.output, args.grid_w, args.grid_h,
                args.num_frames, args.pattern)


if __name__ == "__main__":
    main()
