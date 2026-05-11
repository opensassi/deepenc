#!/usr/bin/env bash
# Compare two benchmark JSON files, output Δ% table with regression detection.
# Usage: ./compare.sh <baseline.json> <candidate.json>

source "$(dirname "$0")/common.sh"

if [[ $# -ne 2 ]]; then
    log_error "Usage: $0 <baseline.json> <candidate.json>"
    exit 1
fi

BASELINE="$1"
CANDIDATE="$2"

if [[ ! -f "$BASELINE" ]]; then log_error "Baseline not found: $BASELINE"; exit 1; fi
if [[ ! -f "$CANDIDATE" ]]; then log_error "Candidate not found: $CANDIDATE"; exit 1; fi

# --- Extract summary values using JSON parsing ---
# Using python for reliable JSON parsing since bc/awk can't handle JSON
python3 -c "
import json, sys

with open('$BASELINE') as f: b = json.load(f)
with open('$CANDIDATE') as f: c = json.load(f)

def avg(items, key):
    vals = [i.get(key) for i in items if i.get(key) is not None]
    if not vals:
        return None
    return sum(vals) / len(vals)

def pct(b_val, c_val):
    if b_val is None or c_val is None or b_val == 0:
        return None
    return ((c_val - b_val) / b_val) * 100

b_t = avg(b['iterations'], 'wall_time_ms')
c_t = avg(c['iterations'], 'wall_time_ms')
b_f = avg(b['iterations'], 'fps')
c_f = avg(c['iterations'], 'fps')
b_br = avg(b['iterations'], 'bitrate_kbps')
c_br = avg(c['iterations'], 'bitrate_kbps')
b_py = avg(b['iterations'], 'psnr_y')
c_py = avg(c['iterations'], 'psnr_y')
b_pu = avg(b['iterations'], 'psnr_u')
c_pu = avg(c['iterations'], 'psnr_u')
b_pv = avg(b['iterations'], 'psnr_v')
c_pv = avg(c['iterations'], 'psnr_v')
b_ssim = avg(b['iterations'], 'ssim')
c_ssim = avg(c['iterations'], 'ssim')
b_vmaf = avg(b['iterations'], 'vmaf')
c_vmaf = avg(c['iterations'], 'vmaf')

# Bitrate increase is bad; all else higher is better for fps/quality
# For time: decrease = improvement = negative Δ%
# For bitrate: decrease = improvement = negative Δ%

print(f\"{'Metric':<25} {'Baseline':<14} {'Candidate':<14} {'Δ%':<10} {'Status'}\")
print(f\"{'-'*25} {'-'*14} {'-'*14} {'-'*10} {'-'*10}\")

def fmt(v, unit=''):
    if v is None: return 'N/A'
    return f'{v:.2f}{unit}'

def flag(v):
    if v is None: return '?'
    return '✓' if abs(v) < 0.01 else ''

regression = False

# Wall time: negative Δ% = good (faster)
dt = pct(b_t, c_t)
status = '✓' if dt is not None and dt < 2.0 else ('⚠ REGRESSION' if dt is not None and dt >= $THRESHOLD_TIME_PCT else '')
if 'REGRESSION' in status: regression = True
print(f\"{'Wall time (ms)':<25} {fmt(b_t):<14} {fmt(c_t):<14} {fmt(dt, '%'):<10} {status}\")

# FPS: positive Δ% = good
df = pct(b_f, c_f)
print(f\"{'FPS':<25} {fmt(b_f):<14} {fmt(c_f):<14} {fmt(df, '%'):<10} {'✓' if df is not None and df >= 0 else ('⚠ REGRESSION' if df is not None and df < -2 else '')}\")

# Bitrate: positive Δ% = bad
dbr = pct(b_br, c_br)
br_status = '✓' if dbr is None or abs(dbr) < $THRESHOLD_BITRATE_PCT else '⚠ REGRESSION'
print(f\"{'Bitrate (kbps)':<25} {fmt(b_br):<14} {fmt(c_br):<14} {fmt(dbr, '%'):<10} {br_status}\")

# PSNR Y: negative Δ% = bad (quality drop)
dpy = pct(b_py, c_py)
dpy_abs = (c_py - b_py) if (c_py is not None and b_py is not None) else None
py_status = '✓'
if dpy_abs is not None and dpy_abs < -$THRESHOLD_PSNR_Y_DB:
    py_status = '⚠ REGRESSION'
    regression = True
print(f\"{'PSNR Y (dB)':<25} {fmt(b_py):<14} {fmt(c_py):<14} {fmt(dpy, '%'):<10} {py_status}\")

dpu = pct(b_pu, c_pu)
dpu_abs = (c_pu - b_pu) if (c_pu is not None and b_pu is not None) else None
pu_status = '✓' if dpu_abs is None or dpu_abs >= -$THRESHOLD_PSNR_Y_DB else '⚠ REGRESSION'
print(f\"{'PSNR U (dB)':<25} {fmt(b_pu):<14} {fmt(c_pu):<14} {fmt(dpu, '%'):<10} {pu_status}\")

dpv = pct(b_pv, c_pv)
dpv_abs = (c_pv - b_pv) if (c_pv is not None and b_pv is not None) else None
pv_status = '✓' if dpv_abs is None or dpv_abs >= -$THRESHOLD_PSNR_Y_DB else '⚠ REGRESSION'
print(f\"{'PSNR V (dB)':<25} {fmt(b_pv):<14} {fmt(c_pv):<14} {fmt(dpv, '%'):<10} {pv_status}\")

if b_ssim is not None and c_ssim is not None:
    dssim = pct(b_ssim, c_ssim)
    print(f\"{'SSIM':<25} {fmt(b_ssim):<14} {fmt(c_ssim):<14} {fmt(dssim, '%'):<10} {'✓' if dssim is not None and dssim > -0.5 else '⚠ REGRESSION'}\")

if b_vmaf is not None and c_vmaf is not None:
    dvmaf = pct(b_vmaf, c_vmaf)
    print(f\"{'VMAF':<25} {fmt(b_vmaf):<14} {fmt(c_vmaf):<14} {fmt(dvmaf, '%'):<10} {'✓' if dvmaf is not None and dvmaf > -1 else '⚠ REGRESSION'}\")

print()
if regression:
    print('⚠  REGRESSION DETECTED: time increased AND quality metric dropped past thresholds.')
else:
    print('✓  PASS: no regression detected.')
" 2>&1 || {
    log_error "Comparison failed. Ensure both files are valid benchmark JSON."
    exit 1
}
