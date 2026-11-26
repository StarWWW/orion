# Orion — Blackwell (`sm_120`) Optimisation Report

This report describes Orion's GPU kernel optimisations for NVIDIA Blackwell (`sm_120`), how they
were measured, and the experiments that did not pay off and why. Every number below was measured;
estimates are explicitly marked.

## TL;DR

| | Baseline (64-bit) | v1.2 | **Orion v2.0** |
|---|---|---|---|
| SM cycles per key (`sm_120`, locked clocks, 3-run median) | 1.000 (`1360.7 M`) | 0.893 (−10.73%) | **0.771 (−22.92% vs base, −13.65% vs v1.2)** |
| SASS instructions per launch (`comp_keys`) | 56.58 G | 57.00 G | **52.84 G (−7.29% vs v1.2)** |
| RTX 5050 Laptop, 80 W power cap, wall clock (3-round A/B, CUDA 13.4) | 989.2 MK/s | 1077.0 MK/s (+8.9%) | **1223.8 MK/s (+23.7% vs base, +13.6% vs v1.2)** |
| RTX 5050 Laptop, 80 W power cap (v1.2 run, CUDA 12.x) | 1070 MK/s | 1166 MK/s (+9.0%) | — |
| RTX PRO 6000 Blackwell (Colab), wall clock | 8625 MK/s | 9479 MK/s (+9.9%)¹ | *(not yet measured; −13.65% cycles implies ≈ +15.8% over v1.2 on clock-limited GPUs)* |

¹ Same card, measured in separate Colab sessions. The intermediate v1 build ran at 9015 MK/s; the
final v1.2 build added +5.1% (predicted +4.9% by cycles), reaching 598 W of its 600 W limit.

## Two metrics, because GPUs hit different limits

* **Wall-clock A/B** (`bench/bench.py speed`). Builds run interleaved (`A B A B …`) on the same
  range and the median of rounds is reported (noise ≈ ±0.5%). The RTX 5050 Laptop sits permanently
  at its **80 W power cap** (`clocks_throttle_reasons = 0x4`), so extra instructions lower the boost
  clock even when they save cycles.
* **SM cycles per key** (`bench/cycles.py`). Nsight Compute measures one `comp_keys` launch with
  clocks locked to base and normalises by keys per launch. This predicts throughput on
  **clock-limited** GPUs (e.g. RTX PRO 6000 at 2430 MHz).

## Changes shipped

### Stage 1 (v1.0 → v1.2)

| Change | Wall clock (laptop) | Cycles / key |
|---|---|---|
| Single full-address search compares `hash160` against constant memory instead of the 128 KB prefix table | +0.8% | |
| Hash and check helpers inlined (`HASH_INLINE`) | +1.1% | |
| 512→320-bit reduction uses real multiplies (`ORION_REDMUL`) instead of shift/add expansion | +5.0% | |
| *Subtotal* | *+7.4%* | *−6.35%* |
| SHA-256 / RIPEMD-160 additions written as `IMAD x·1 + y` (`ORION_FADD=7`) | ≈ 0 (80 W clock drop) | −2.1% |
| 128 threads per block, twice the blocks (`ORION_TPB=128`, `ORION_BPSM=256`; same threads/launch) | | −2.1% |
| **v1.2 Total (vs Baseline)** | **+8.9% … +9.0%** | **−10.73% … −10.8%** |

### Stage 2 (Orion v2.0, on top of v1.2)

| Change | Cycles / key (vs v1.2) | Instructions (vs v1.2) | Wall clock (80 W laptop, vs v1.2) |
|---|---|---|---|
| Compile-time kernel specialisation (`comp_keys<MODE>`), RIPEMD-160 7-round early-exit (`h[2]` after `j=75R/76L` for single & multi-full, 3-round `h[0]` for prefix), compile-time SHA/RMD constants & padding folding | −5.8% | −2.3% | |
| Native 32-bit Comba `ModMult_Wide32` / `ModSqr_Wide32` (`IMAD.WIDE.U32`, eliminating 398 `IADD.64.X` duplicates/pair), branchless `ModSub256_32`/`ModAdd256_32` (zero warp divergence), drop `sxn`/`syn` (−16 live regs) | −13.4% cum. | −6.7% cum. | |
| Drop `__syncthreads()` (`ORION_NOBARRIER=1`, 0% barrier stall after branchless EC), SHA-256 critical-path reorder (`_fadd(t1, S0(a)+Maj)`), `WMIX_init` `_fadd`, parallel `Reduce512_Wide32` (0 `IADD.64`, 8× ILP `mul.wide`), `COMBA_MAC_INIT` (`HFMA2` 98→12, `MOV` 204→118), single-asm `ModSub256_32`/`ModAdd256_32` | **−13.65%** | **−7.29%** | **+13.6%** |
| Host & launch polish: pass `JumpPoint` by value (zero sync `cudaMemcpyToSymbol` in `-random`), `cudaMemsetAsync`, skip 2nd `cudaMemcpy` when `nbFound==0`, multi-threaded `getGPUStartingKeys()`, `-O3` host build | — | — | faster startup & `-random` |
| **Orion v2.0 Cumulative Total (vs Baseline)** | **−22.92% (+29.7% clock-limited)** | **−6.61% (56.58G → 52.84G)** | **+23.7% (989.2 → 1223.8 MK/s)** |

#### `sm_120` microbenchmark results (`bench/sm120_probe.cu`, locked base clocks, 65,536-vector bit-exact verified)

| Primitive | Baseline 64-bit (`_ModMult`/`_ModSqr`) | Orion v2.0 `Wide32` (`ModMult_Wide32`/`ModSqr_Wide32`) | Delta |
|---|---|---|---|
| `ModMult` (256×256 mod p) | 20.92 Mcycles / 596.4 M insts / 52 regs | **12.53 Mcycles / 468.0 M insts / 42 regs** | **−40.1% cycles (1.67×), −21.5% insts** |
| `ModSqr` (256² mod p) | 14.87 Mcycles / 488.0 M insts / 44 regs | **9.01 Mcycles / 389.3 M insts / 40 regs** | **−39.4% cycles (1.65×), −20.2% insts** |
| `ModMult_Karatsuba32` (rejected) | 20.92 Mcycles | 13.02 Mcycles / 583.3 M insts / 48 regs | slower than Comba `Wide32` (+24.6% more insts) |

#### Nsight Compute profile (`comp_keys<TARGET_SINGLE>`, locked base clocks)

| Metric | v1.2 | **Orion v2.0** |
|---|---|---|
| `sm__cycles_elapsed.avg` | 1,218.7 M | **1,050.0 M (−13.8%)** |
| `sm__inst_executed.sum` | 57.00 G | **52.84 G (−7.3%)** |
| `launch__registers_per_thread` | 126 | **122 (0 spills, 0 barriers)** |
| `pipe_aluheavy` / `pipe_fmaheavy` | 77.3% / 50.0% | **79.6% / 48.0%** |
| `stalled_barrier` | 2.27% | **0.00%** |
| `stalled_math_pipe_throttle` | 28.77% | **25.55%** |

Correctness: `bench/bench.py selftest` hides a random key in an aligned range, runs `orion`, and
independently re-derives the reported key's address in pure Python. Every shipped change passed
across `single`, `multi`, `bech32`, `prefix`, `-random`, `-end`, and `-backup` save/resume.

## Why the kernel is where it is

* **Pipe-bound, not memory-bound.** In Orion v2.0 the kernel runs at **3,110 SASS instructions per
  key** (down from 3,397 in v1.2), with ALU-heavy at 79.6%, FMA-heavy at 48.0%, 0% barrier stalls,
  and zero `IADD.64` instructions in the hot loop.
* **sm_120 pipe map**, from `bench/pipes2.cu` and `bench/sm120_probe.cu`:

  | Instructions | Pipe | Warp-instructions / SM / clock |
  |---|---|---|
  | `SHF`, `LOP3`, **`IADD3`**, `LEA` | ALU-heavy only | 2 |
  | `IADD` (2-input), `IADD.X`, `ISETP`/`SEL` | ALU-lite: split between ALU-heavy and FMA's alulite sub-pipe | 4 |
  | `IMAD` | FMA-heavy only | 2 |
  | `IADD.64` / `IADD.64.X` | Consumes both ALU-heavy and FMA-heavy counters | 1–2 |

  SHA-256 and RIPEMD-160 consist almost entirely of rotations (`SHF`/`LEA`) and boolean functions
  (`LOP3`), which must run on ALU-heavy. Stage 1 moved reduction multiplies and 3-input additions
  onto FMA; Stage 2 eliminated 7.3% of raw instructions via native 32-bit Comba `IMAD.WIDE.U32`,
  parallel `mul.wide` reduction (zero `IADD.64`), branchless `ModSub`/`ModAdd`, RIPEMD-160 7-round
  early-exit, and constant/padding folding.
* **Elliptic-curve side.** Affine batch-inversion reaches the 3.5 multiplications + 1 squaring per
  key floor, sharing one modular inverse across 1024 keys.
* **Host side.** Nsight Systems shows the GPU busy 99.8% of the time; v2.0 also removes blocking
  `cudaMemcpyToSymbol` in `-random` mode and parallelises CPU start-key generation.

## Experiments that did not work

| Experiment | Result |
|---|---|
| v1.2: Remove `__syncthreads()` (`ORION_NOBARRIER`) with branch-divergent 64-bit `ModSub256` | laptop −1.0%; did not stack with 128-thread blocks. *Revisited in v2.0:* once `ModSub256_32`/`ModAdd256_32` became branchless, `ORION_NOBARRIER=1` became a net win and is now default. |
| Group size 512 / 256 (`ORION_GRP`) | laptop −0.8% / −2.0%; cycles per key +0.6% (512) |
| 512 threads/block; grid SM×64; `maxrregcount` 96 / 80; `__launch_bounds__(…, 2)` | cycles per key +4.8%, +1.1%, +2.6% / +1.4%, +0.5% |
| Rotations on FMA via `IMAD.WIDE` (halves combined with OR or ADD) | laptop −3.9 … −27%, cycles +3.1% (`IMAD.WIDE` is half-rate and long-latency on a serial hash chain) |
| 64-bit carry chains as 32-bit `IADD`/`IADD.X` pairs | laptop ±0, cycles +0.2% |
| v1.2: 256×256 product with 32-bit row-scanning `mad.lo.cc`/`madc.hi.cc` (`bench/fieldmul.cu`) | bit-exact and +2% in isolation, but +3.6% cycles in the kernel. *Superseded in v2.0* by Comba `IMAD.WIDE.U32` + parallel `mul.wide` reduction (−40% in isolation, major kernel win). |
| Full grid (SMs × 256) instead of rounding sequential grid down to power of two | cycles −0.11%; power-of-two keeps exact splits of power-of-two ranges and backup layout |
| v2.0: Split 3-input `IADD3` (`h+k+w`, `a+x+k`) into `IADD + _fadd` (`ORION_FADD=15`) | cycles +0.8%, insts +3.8% |
| v2.0: Offload 2-input reg-reg adds (`d+=t1`, `S0+Maj`, `w+w9`) to `_fadd` (`IMAD`) | cycles +2.4% (`IADD` already uses FMA's alulite sub-pipe at lower latency) |
| v2.0: Replace `x>>3` / `x>>10` in SHA-256 `s0`/`s1` with `mul.hi.u32` (`IMAD.HI.U32`) | cycles +0.4% (both) / +0.2% (`s0` only) |
| v2.0: 1-level Karatsuba 256×256 (`ModMult_Karatsuba32` in `bench/sm120_probe.cu`) | 48 vs 64 `IMAD.WIDE`, but +24.6% more total instructions and +6 regs; slower than Comba `Wide32` |
| v2.0: 2-way interleaved SHA-256 (`Key+` + `Key-` together) | cycles +3.4% (higher register pressure, bunches ALU-heavy work instead of interleaving EC↔Hash) |
| v2.0: Keep `sx`/`sy` in SoA shared memory (`8 KB/block`) for 5–6 blocks/SM | cycles +0.2% to +5.2% (`LDS`/`STS` overhead and 80-reg spills outweigh extra occupancy) |
| v2.0: Single-pass pair EC + 5 blocks/SM (`ORION_PAIR_EC=1`, `ORION_MINBLOCKS=5`, 96 regs, 0 spills, `TPB=64/128/32`) | within 0.1% on locked-clock cycles, but −1.5% to −2.0% slower on 80 W laptop wall clock |
| v2.0: Sub-batching (`ORION_SUBGRP=128/256/512`) with warp-cooperative binary-tree `WarpModInv32` | L2 hit jumps 30% → 97.5% (`128`), but tree multiplies add +0.3% to +4.4% cycles and −1.5% to −3.2% wall clock; kept as optional `-DORION_SUBGRP` knob |
| v2.0: Warp-level phase staggering (half of warps hash center after `_ModInv`) | cycles +2.1% |
| v2.0: `-Xptxas --allow-expensive-optimizations=true` | neutral to +0.5% cycles |

## Reproducing (Windows; Linux uses the Makefile)

```bash
python bench/build.py opt
python bench/bench.py selftest build/opt/orion.exe --bits 36 --n 3
python bench/bench.py selftest build/opt/orion.exe --bits 34 --n 3 --random
python bench/bench.py speed  build/ref/orion.exe build/opt/orion.exe
python bench/cycles.py       build/ref/orion.exe build/opt/orion.exe
```

`bench/cycles.py` requires Nsight Compute and GPU performance-counter access. A build with a
different amount of work per launch takes a suffix, e.g. `build/x/orion.exe@0.5`.

Build-time options (`-D…`, see `bench/sweep.py`):

| Option | Default | Meaning |
|---|---|---|
| `ORION_FADD` | 7 | `IMAD` additions: 1 = SHA-256 rounds, 2 = SHA-256 schedule (`WMIX` & `WMIX_init`), 4 = RIPEMD-160 |
| `ORION_TPB` / `ORION_BPSM` | 128 / 256 | threads per block / blocks per SM per launch |
| `ORION_GRP` | 1024 | keys per thread per launch (256, 512 or 1024) |
| `ORION_SUBGRP` | `ORION_GRP` (1024) | internal sub-batch size (128, 256, 512 or 1024); values `< ORION_GRP` use `WarpModInv32` |
| `ORION_PAIR_EC` | 0 | 1 = compute `Key+` and `Key-` EC together (fits 96 regs / 5 blocks/SM with 0 spills) |
| `ORION_NOBARRIER` | 1 | drop `__syncthreads()` in the hot loop |
| `ORION_MINBLOCKS` | unset | `__launch_bounds__` minimum blocks per SM |
| `HASH_INLINE` | `__forceinline__` | inlining of the fused hash + early-exit check |

Microbenchmarks: `bench/pipes.cu` (per-instruction throughput), `bench/pipes2.cu` (pipe sharing),
`bench/fieldmul.cu`, and `bench/sm120_probe.cu` (`sm_120` 32-bit Comba `Wide32` vs 64-bit vs
Karatsuba field-multiply/square probes).
