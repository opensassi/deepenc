# LightGBM Training Plan — Next Session

## Goal

Train real ML-accelerated CU split prediction models that achieve measurable speedup (>10%) with minimal BDBR (<2%).

## Current Pipeline State

| Step | Command | Status |
|---|---|---|
| Data generation | `ml data-generate` | Working — pre-split CU features + post-split ground-truth labels |
| Data split | `ml data-split` | Working — 80/20 train/val split |
| Training | `ml train` | Working — shells out to `train_lightgbm.py` |
| ML encode | `ml encode` | Working — sets `--ml-model-dir --ml-confidence` |
| Benchmark | `ml bench` | Working — BDBR via cubic interpolation |
| Threshold sweep | `ml sweep` | Working — loops bench over thresholds |
| Feedback | `ml feedback` | Working — sets `VVENC_ML_FEEDBACK`, appends mispredictions, retrains |

## Test Clips Available

| File | Resolution | Frames | Size |
|---|---|---|---|
| `test/data/park_joy_1080p50.yuv` | 1920×1080 | 50 | 155 MB |
| `test/data/park_joy_1080p10.yuv` | 1920×1080 | 10 | 31 MB |
| `test/data/park_joy_1280x720f50.yuv` | 1280×720 | 50 | 69 MB |
| `test/data/park_joy_1280x720f10.yuv` | 1280×720 | 10 | 14 MB |
| `test/data/park_joy_832x480f50.yuv` | 832×480 | 50 | 30 MB |
| `test/data/park_joy_640x360f50.yuv` | 640×360 | 50 | 17 MB |
| `test/data/RTn23_80x44p15_f15.yuv` | 80×44 | 15 | 79 KB |

Only one scene (park_joy) available. For real training, source 2-3 more diverse clips (see below).

## Step-by-Step

### 0. Prerequisites

```bash
cd /home/pc/projects/deepenc
```

- Build the ML-enabled encoder with both flags (already done in `build_ml/` and `build_ml_release/`)
- Verify vvencapp has ML flags: `./bin/release-static/vvencapp --help | grep ml`
- Verify the harness CLI works: `node deepenc-harness/lib/index.js --help`

### 1. Source Additional Clips

JVET Common Test Conditions clips are available from:
- https://jvet.hhi.fraunhofer.de/
- Or use public domain alternatives: Xiph.org Video Test Media

Create `clips.json`:
```json
[
  {"path": "/data/park_joy_1080p50.yuv", "name": "park_joy", "width": 1920, "height": 1080, "fps": 50},
  {"path": "/data/basketballdrive_1080p50.yuv", "name": "basketballdrive", "width": 1920, "height": 1080, "fps": 50},
  {"path": "/data/campfire_1080p50.yuv", "name": "campfire", "width": 1920, "height": 1080, "fps": 50}
]
```

Or use the single available clip with multiple resolutions:
```json
[
  {"path": "test/data/park_joy_1080p50.yuv", "name": "park_joy_1080p", "width": 1920, "height": 1080, "fps": 50},
  {"path": "test/data/park_joy_832x480f50.yuv", "name": "park_joy_832", "width": 832, "height": 480, "fps": 50},
  {"path": "test/data/park_joy_640x360f50.yuv", "name": "park_joy_640", "width": 640, "height": 360, "fps": 50}
]
```

### 2. Generate Training Data

```bash
deepenc-harness ml data-generate \
  --clips-config ./clips.json \
  --qps 22,27,32,37 \
  --data-dir ./ml-data \
  --vvencapp-path ./bin/release-static/vvencapp
```

**Expected**: 50K-250K rows per clip × QP. With 3 clips × 4 QPs ≈ 600K-3M rows.
**Duration**: ~30s per encode at 1080p, ~12 encodes = ~6 min.

### 3. Split Into Train/Val

```bash
deepenc-harness ml data-split --data-dir ./ml-data --train-ratio 0.8
```

### 4. Train 5 Binary Classifiers

```bash
deepenc-harness ml train \
  --data-dir ./ml-data \
  --model-dir ./ml-models \
  --script-path ./scripts/train_lightgbm.py
```

The script trains 5 one-vs-rest LGBMClassifiers:
- QT vs rest
- BH vs rest
- BV vs rest
- TH vs rest
- TV vs rest

**Hyperparameters** (hardcoded in `train_lightgbm.py`):
- `n_estimators=500`, `num_leaves=128`, `learning_rate=0.05`, `early_stopping_rounds=20`

**Expected**: ~2-5 min for 3M rows.

Label distribution check:
```bash
awk -F',' '{print $NF}' ./ml-data/train.csv | sort | uniq -c | sort -rn
```

If NO_SPLIT dominates (>80%), consider class balancing before the next session.

### 5. Benchmark

Quick check with 2 frames:
```bash
deepenc-harness ml bench \
  --clip test/data/park_joy_832x480f50.yuv \
  --width 832 --height 480 --fps 50 \
  --qps 22,27,32,37 \
  --model-dir ./ml-models \
  --confidence 0.80 \
  --vvencapp-path ./bin/release-static/vvencapp
```

Full 50-frame bench:
```bash
deepenc-harness ml bench \
  --clip test/data/park_joy_832x480f50.yuv \
  --width 832 --height 480 --fps 50 \
  --qps 22,27,32,37 \
  --model-dir ./ml-models \
  --confidence 0.80 \
  --vvencapp-path ./bin/release-static/vvencapp
```

**Expected**: At least 10% speedup with <2% BDBR from a reasonably trained model.

### 6. Sweep Confidence Thresholds

```bash
deepenc-harness ml sweep \
  --clip test/data/park_joy_832x480f50.yuv \
  --width 832 --height 480 --fps 50 \
  --qps 22,27,32,37 \
  --model-dir ./ml-models \
  --thresholds 0.65,0.70,0.75,0.80,0.85,0.90,0.95 \
  --vvencapp-path ./bin/release-static/vvencapp
```

### 7. Feedback Flywheel Iteration

```bash
# One iteration: collect mispredictions + retrain
deepenc-harness ml feedback \
  --clip test/data/park_joy_832x480f50.yuv \
  --width 832 --height 480 --fps 50 \
  --qp 27 \
  --model-dir ./ml-models \
  --confidence 0.80 \
  --data-dir ./ml-data \
  --vvencapp-path ./bin/release-static/vvencapp
```

Repeat 2-3 times. Each iteration typically gives 3-5% accuracy gain.

## Expected Results (from spec)

| Confidence | Speedup | BDBR |
|---|---|---|
| 0.95 | ~23% | ~0.8% |
| 0.90 | ~31% | ~1.4% |
| 0.85 | ~38% | ~2.1% |
| 0.80 | ~43% | ~2.9% |
| 0.75 | ~49% | ~4.1% |
| 0.70 | ~53% | ~5.8% |
| 0.65 | ~57% | ~7.9% |

## Known Issues to Watch

| Issue | Impact | Mitigation |
|---|---|---|
| Only park_joy clip available | Model won't generalize to other content | Source 2-3 additional clips from JVET CTC |
| Class imbalance (NO_SPLIT ~60%+ of rows) | BH/BV/TH/TV models may be weak | Use `scale_pos_weight` or oversample |
| 50 frames × 4 QPs × N clips = slow | Long wait times | Use `--frames 16` during iteration, 50 for final |
| Feedback env var only works with ML_LIGHTGBM=ON | Can't collect feedback without ML inference | Build must have both `ML_LIGHTGBM=ON` and `AI_TRAINING=ON` |

## Suggested Session Timeline

| Phase | Est. Time | Output |
|---|---|---|
| Source clips + create clips.json | 10 min | 3 clip files + config |
| Generate training data (3 clips × 4 QPs) | 6 min | ~2M rows of training data |
| Split + train LightGBM | 5 min | 5 model files |
| Quick bench (2 frames, 1 clip) | 2 min | Speedup/BDBR numbers |
| Full bench (50 frames, 1 clip) | 5 min | Confirmed speedup/BDBR |
| Sweep thresholds | 15 min | Speed/quality curve |
| Feedback iteration | 10 min | Augmented model |
| **Total** | **~53 min** | **Usable ML model** |
