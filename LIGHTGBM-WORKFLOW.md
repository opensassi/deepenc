# LightGBM Block Partitioning — User Workflow

Complete lifecycle from training data generation to deployed ML-accelerated encoding.

---

## Phase A: Generate Training Data

### A1 — Build instrumented encoder

```bash
mkdir build_training && cd build_training
cmake .. -DCMAKE_BUILD_TYPE=Release -DVVENC_ENABLE_AI_TRAINING=ON
make -j$(nproc)
```

Produces a `vvencapp` that dumps one CSV row per CU undergoing RDO split search:

```
clip,poc,ctu_x,ctu_y,cu_depth,var_luma,grad_mag,...,best_split
```

Controlled by environment variable (normal encodes are unaffected):

```bash
# Training mode: dump features
VVENC_TRAINING_OUT=training_data.csv ./vvencapp ... --preset slow -o /dev/null

# Normal mode: no overhead
./vvencapp ... --preset slow -o out.266
```

### A2 — Encode diverse clips to build dataset

Gather 10–20 clips from VVC Common Test Conditions at varied resolutions (1080p, 720p, 832×480). Encode Random Access at QP 22, 27, 32, 37:

```bash
for clip in park_joy_1080p50 basketballdrive_1080p50 bqmall_832x480; do
  for qp in 22 27 32 37; do
    VVENC_TRAINING_OUT=data/${clip}_qp${qp}.csv \
    ./vvencapp --yuv ${clip}.yuv --size 1920x1080 --framerate 50 \
               --preset slow --qp ${qp} --frames 64 -o /dev/null
  done
done
```

**Expected**: 4M–20M labeled rows (64 frames × ~5000 CUs/frame × 4 QPs × N clips).

### A3 — Merge and shuffle

```bash
head -1 data/park_joy_qp22.csv > combined.csv
tail -n+2 -q data/*.csv | shuf >> combined.csv
```

Split 80/20 train/validation:

```bash
total=$(wc -l < combined.csv)
split=$((total * 80 / 100))
head -$split combined.csv > train.csv
tail -$((total - split)) combined.csv > val.csv
```

---

## Phase B: Train LightGBM Models

### B1 — Install dependencies

```bash
pip install lightgbm pandas scikit-learn
```

### B2 — Run training script

```bash
python scripts/train_lightgbm.py \
    --train train.csv \
    --val val.csv \
    --output-dir ./models
```

Trains 5 binary classifiers (QT, BH, BV, TH, TV) with early stopping on validation loss. Saves native `.txt` format model files.

### B3 — Inspect a model file

```bash
cat models/qt_split_model.txt | head -20
```

Output:
```
tree_sizes=500
num_class=1
num_tree_per_iteration=1
...
Tree=0
num_leaves=64
split_feature=3 7 12 1 ...
threshold=0.45 128.0 0.33 92.0 ...
leaf_value=0.82 0.76 0.91 0.23 ...
```

This is the format consumed by `LGBM_BoosterCreateFromModelfile()`.

---

## Phase C: Deploy and Run ML-Accelerated Encoder

### C1 — Build with ML enabled

```bash
sudo apt install liblightgbm-dev

mkdir build_ml && cd build_ml
cmake .. -DCMAKE_BUILD_TYPE=Release -DVVENC_ENABLE_ML_LIGHTGBM=ON
make -j$(nproc)
```

### C2 — Run encoder with ML models

```bash
./vvencapp --yuv park_joy_1080p50.yuv --size 1920x1080 \
           --framerate 50 --preset slow --qp 32 \
           --ml-model-dir ./models \
           --ml-confidence 0.80 \
           -o test_ml.266
```

Expected frame-level logging:

```
[ML] FASTSplitPredictor loaded 5 models from ./models/
[ML] Confidence threshold: 0.80
Frame   0: ML skip rate 47.3% (mean conf 0.87)
Frame   1: ML skip rate 52.1% (mean conf 0.89)
...
Encoding summary:
  Total time: 124.3s
  ML skip rate: 49.8%
```

### C3 — Compare with baseline

```bash
# Baseline (no ML)
./vvencapp ... -o test_baseline.266

# ML-accelerated
./vvencapp ... --ml-model-dir ./models --ml-confidence 0.80 -o test_ml.266

# Measure via harness
deepenc-harness benchmark \
    --reference test_baseline.266 \
    --candidate test_ml.266 \
    --video park_joy_1080p50.yuv
```

**Expected at conf 0.80**: ~43% speedup, ~2.9% BDBR increase.

---

## Phase D: Tune Confidence Threshold

```bash
for conf in 0.65 0.70 0.75 0.80 0.85 0.90 0.95; do
  ./vvencapp ... --ml-model-dir ./models --ml-confidence $conf -o test_${conf}.266
done
```

Aggregate into speed/quality curve:

| confidence | speedup | BDBR |
|---|---|---|
| 0.95 | ~23% | ~0.8% |
| 0.90 | ~31% | ~1.4% |
| 0.85 | ~38% | ~2.1% |
| 0.80 | ~43% | ~2.9% |
| 0.75 | ~49% | ~4.1% |
| 0.70 | ~53% | ~5.8% |
| 0.65 | ~57% | ~7.9% |

Map to deepenc presets:

| Preset | Confidence | Speedup | BDBR |
|---|---|---|---|
| slow (default) | 0.90 | ~30% | ~1.5% |
| medium | 0.80 | ~43% | ~2.9% |
| fast | 0.65 | ~57% | ~6-8% |

---

## Phase E: Iterate (The Flywheel)

```
  Encode with current models → Collect mispredictions → Augment training set
  → Retrain LightGBM → Deploy new models → Encode with better models
```

The instrumented encoder records only **mispredicted** CUs (fast — no re-encode needed):

```bash
VVENC_ML_FEEDBACK=ml_feedback.csv \
./vvencapp ... --ml-model-dir ./models --ml-confidence 0.80 -o out.266
```

Augment with hard cases and retrain. Typically 3-5% accuracy gain per 2-3 flywheel iterations.

---

## "Hello World" Demo Timeline

| Time | Action | Output |
|---|---|---|
| 09:00 | Build instrumented VVenC | `vvencapp` binary |
| 09:30 | Encode 2 clips at 4 QPs | ~3M training rows |
| 12:30 | Train LightGBM (while eating) | 5 model files |
| 13:00 | Build ML-enabled VVenC | `vvencapp` with inference |
| 13:30 | Encode same clips with ML | 43% faster |
| 14:00 | Bidirectional comparison | Speedup/BDBR numbers |
| 14:30 | Sweep 5 confidence thresholds | Speed/quality curve |
| 15:00 | Generate report | "41-57% faster, 1-8% bitrate increase" |
