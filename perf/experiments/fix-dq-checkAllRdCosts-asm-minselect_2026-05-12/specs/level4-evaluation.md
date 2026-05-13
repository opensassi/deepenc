# Level 4 Evaluation — gtxFracBits Optimization Options

Evaluated against the CPU pipeline model from `cpu-pipeline.spec.md`.

## Option A: SoA Transposition of gtxFracBits

### Current (AoS)

```cpp
struct CoeffFracBits { int32_t bits[6]; };
CoeffFracBits m_gtxFracBits[21];   // 21 × 24 = 504 bytes
// stride for same bits[N].ctx: 24 bytes — scatters across cache lines
```

### Proposed (SoA)

```cpp
struct CoeffFracBitsSoA {
    int32_t bits0[21];   // +0
    int32_t bits1[21];   // +84
    int32_t bits2[21];   // +168
    int32_t bits3[21];   // +252
    int32_t bits4[21];   // +336
    int32_t bits5[21];   // +420
};
// stride for same bits[N].ctx: 4 bytes — contiguous
```

### μop Comparison (cffBits gather in ASM)

| Operation | AoS (Level 2) | SoA (Level 4) | Δ |
|-----------|--------------|--------------|---|
| P2/P3 (load) | 8 | 4 | -4 |
| P5 (shuffle/perm) | 8 | 8 | 0 |
| Total uops | 18 | 14 | -22% |

### Projected speedup

- Microbenchmark: ~5% (from 25.3ms → ~24.0ms)
- Full encoder: <0.5% (below noise floor)

### Risk

- 8 files require modification
- Hard-coded offset 416 in ASM must be updated
- ABI-breaking change to StateMem layout
- All scatter/gather sites (scalar, SIMD, NEON) must be updated

### Verdict: **Rejected**

Complexity far exceeds the sub-noise-level gain.

## Option B: cffBits Prefetch Cache in updateStates

Add a 96-byte cache to StateMem (6 levels × 4 states × 4 bytes):

```cpp
// In struct StateMem:
int32_t cffCached[6][4];
```

### Fill site

After `ctx.cff[stateId]` is computed in `update1State` / `update1StateEOS`,
prefetch all 6 bits values:

```cpp
const CoeffFracBits& cb = curr.m_gtxFracBitsArray[curr.ctx.cff[stateId]];
for (int l = 0; l < 6; l++)
    curr.cffCached[l][stateId] = cb.bits[l];
```

### Read site

`checkAllRdCosts` reads from cache instead of gather:

```cpp
int32_t cffBitsArr[4] = {
    state.cffCached[pqData[2].absLevel][1],
    state.cffCached[pqData[1].absLevel][3],
    state.cffCached[pqData[2].absLevel][0],
    state.cffCached[pqData[1].absLevel][2],
};
```

### Analysis

- **checkAllRdCosts saves**: ~8 load uops + ~8 LEA computations = ~20c saved
- **updateStates costs**: 24 scalar loads + 6 vector stores = ~120c added
- **Net**: +100c per iteration (+26%) — a **regression**

### Verdict: **Rejected**

The cache fill overhead in updateStates (which is already heavier than
checkAllRdCosts) swamps the read-side savings.

## Conclusion

Neither approach is profitable. The function is within ~7% of the compiler's
output in the microbenchmark, and within noise at the full encoder level.
Further optimization requires architectural changes beyond the scope of this
function's optimization.
