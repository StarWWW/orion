# Orion

High-throughput CUDA Bitcoin key-range scanner engineered for NVIDIA Blackwell (`sm_120`) GPUs.

Orion searches a contiguous private-key range for compressed P2PKH (`1...`) or P2WPKH (`bc1q...`)
addresses or prefixes. Its Blackwell GPU kernel achieves **22.9% fewer SM cycles per key**
and **+23.7% higher wall-clock throughput** over the unoptimised 64-bit baseline, with every change
A/B measured and independently verified in pure Python.

| Metric / GPU | Baseline (64-bit) | v1.2 | **Orion v2.0** |
|---|---|---|---|
| SM cycles per key (`sm_120`, locked clocks) | 1.000 | 0.893 (−10.7%) | **0.771 (−22.9%)** |
| RTX 5050 Laptop (80 W power cap, CUDA 13.4) | 989.2 MK/s | 1077.0 MK/s (+8.9%) | **1223.8 MK/s (+23.7%)** |
| RTX PRO 6000 Blackwell (Google Colab) | 8625 MK/s | 9479 MK/s (+9.9%) | *(not yet measured; −22.9% cycles)* |

Full profiling methodology, microbenchmarks, and rejected experiments are documented in
**[OPTIMIZATION.md](OPTIMIZATION.md)**.

## Architecture & Highlights

* **Native 32-bit `Wide32` secp256k1 arithmetic:** Comba `IMAD.WIDE.U32` field multiplication and
  squaring (−40% cycles in isolation), parallel `mul.wide` 512→256 reduction (zero `IADD.64` in the
  hot loop), and branchless `ModSub256_32`/`ModAdd256_32`.
* **Specialised kernels & RIPEMD-160 early-exit:** Compile-time `comp_keys<MODE>` specialisation
  skips the last 7 RIPEMD-160 half-rounds on `h[2]` mismatch for single and multi full-address
  searches (3 half-rounds for prefix searches).
* **Blackwell pipe balancing:** SHA-256 and RIPEMD-160 compile-time constants, folded rounds and
  padding, critical-path reordering, and `IMAD x·1 + y` additions (`ORION_FADD=7`) offloading work
  from the ALU-heavy pipe to the FMA pipe.
* **Barrier-free hot loop:** `ORION_NOBARRIER=1`, 128 threads/block (`ORION_TPB=128`,
  `ORION_BPSM=256`), 122 registers, 0 register spills, 0% barrier stalls.
* **Fast host & safe resume:** Multi-threaded CPU starting-key generation, by-value `JumpPoint`
  kernel arguments (zero blocking `cudaMemcpyToSymbol` in `-random` mode), atomic
  `orion_backup_gpu<N>.dat` checkpoints with range/layout validation, and strict CLI/address
  validation.
* **Tooling & verification:** Windows build script, 5-mode pure-Python planted-key self-test
  (`single`, `multi`, `bech32`, `prefix`, `--random`), wall-clock A/B and Nsight Compute cycle
  benchmarks, and `sm_120` microbenchmarks (`bench/`).

## Usage

```
orion [-gpuId N] [-start HEX] [-range BITS | -end HEX] [-stop] [-random] [-backup]
      [-i file] [-o file] [-m N] address...
```

| Option | |
|---|---|
| `-start HEX` | First private key, default 1. A `0x` prefix is accepted. |
| `-range BITS` | Scan `start` … `start + 2^BITS − 1` (default 30). |
| `-end HEX` | Last key (inclusive), for ranges that are not a power of two. |
| `-stop` | Stop once every address has been found. |
| `-random` | At every step all GPU threads jump to the same random offset within their own slice of the range. |
| `-backup` | Resume from `orion_backup_gpu<N>.dat`. Written every 60 launches, on pause, on Ctrl+C and at the end. A new run without `-backup` keeps the previous file as `.prev`. |
| `-i file` / `-o file` | Addresses to search (one per line, `#` comments) / file the found keys are appended to (with public key and time). |
| `-gpuId N` | GPU to use. One GPU per process: run one instance per GPU. |
| `-l` | List the CUDA devices. |
| `-h`, `-v` | Help, version. |

While running, `p` pauses and releases the GPU, and `p` again resumes. **Ctrl+C** saves the progress
and exits (a second Ctrl+C exits at once). The exit code is 1 on errors and 0 otherwise.

## Build

**Linux**, CUDA ≥ 12.8 for `sm_120`:

```bash
make                 # -> ./orion for sm_120 (Blackwell)
make CCAP=89         # other GPUs: 89 = Ada, 86 = Ampere, ...
```

**Windows** (Visual Studio 2022 with "Desktop development with C++", CUDA ≥ 12.8, Python 3):

```bash
python bench/build.py opt            # -> build/opt/orion.exe, sm_120 (--arch 89 for Ada, ...)
```

`Orion.sln` also builds in Visual Studio for `sm_75 … sm_120` using the CUDA toolkit pointed to by
`CUDA_PATH` (`msbuild /p:CudaVersion=12.8` selects another installed version).

## Verify before you trust it

```bash
python bench/bench.py selftest build/opt/orion.exe --bits 36 --n 3            # sequential
python bench/bench.py selftest build/opt/orion.exe --bits 34 --n 3 --random   # -random mode
```

The self-test hides a random key in a small range, runs `orion`, and independently re-derives the
reported key's address in pure Python. `--target` selects the GPU path: `single` (default, one `1...`
address), `multi` (several full addresses via `lookup32`), `bech32` (one `bc1q...` address) and
`prefix` (leading characters of the address).

## Documentation

Full guides are in [`docs/`](docs/):

* [Getting started](docs/getting-started.md) — build, self-test, and a first search that finishes in a second.
* [Usage](docs/usage.md) — every option, address formats, the progress line, pause, random mode, backup/resume.
* [Searching a puzzle](docs/puzzle-search.md) — puzzle ranges, realistic odds, and handling a found key safely.
* [Troubleshooting](docs/troubleshooting.md) — build and runtime errors and their fixes.
* [How it works](docs/how-it-works.md) — what the GPU and CPU do for every key.

## License

Licensed under the GNU Affero General Public License v3; see [LICENSE.txt](LICENSE.txt).
Inherited source files retain their original copyright and GPLv3 notices in accordance with
GPLv3 section 13 / AGPLv3.
