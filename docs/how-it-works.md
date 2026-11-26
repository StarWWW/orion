# How it works

A short tour of what happens for every key. For the Blackwell tuning and its measurements, see
[OPTIMIZATION.md](../OPTIMIZATION.md).

## The problem

A Bitcoin address is derived from a private key by a fixed pipeline:

```
private key  --[secp256k1 scalar mult]-->  public key
             --[SHA-256 then RIPEMD-160]-->  hash160 (20 bytes)
             --[Base58Check or bech32]-->    address
```

The tool runs this pipeline for every key in a range and checks the resulting `hash160` against the
target(s). There is no shortcut through it; the work is to run it as fast as the GPU allows and to
check the result cheaply.

## One group of keys at a time

The slow step is the elliptic-curve scalar multiplication. Doing it independently per key would be
far too slow, so Orion uses batched affine point additions:

* Each GPU thread owns a contiguous slice of the range. Before the first launch, the host computes
  every thread's starting point once in parallel across CPU cores (by point additions with a batch
  inversion, not one scalar multiplication per thread) and copies them to the GPU.
* In each launch, a thread holds one point `P` at the middle of its current group and reaches the
  other keys of the group as `P ± i·G` with elliptic-curve **point additions**, which are much cheaper
  than a multiplication. It then moves `P` forward by one group for the next launch.
* Point addition needs a modular inverse, and inverses are expensive. They are done in **batch**: the
  group's denominators are multiplied together, inverted once, and unwound. The cost comes out to
  about 3.5 field multiplications and one squaring per key, with no per-key inverse.

A group is `ORION_GRP` keys per thread per launch (1024 by default). All the arithmetic is in affine
coordinates (Z = 1).

## Hashing and checking on the GPU

For each public key the kernel computes `RIPEMD-160(SHA-256(compressed pubkey))` with compile-time
kernel specialisation (`comp_keys<MODE>`) and early-exit before the final RIPEMD-160 rounds:

* **One full address** — the fast path. The target `hash160` is held in constant memory; word 2
  (`h[2]`, minus the RIPEMD-160 IV) is checked right after half-rounds `75R`/`76L`, skipping the
  remaining 7 half-rounds on virtually every key. Only on a 32-bit match are the remaining rounds
  finished and all 5 words verified.
* **Several full addresses** — `h[2]` indexes a 16-bit table and, on a hit, a sorted 32-bit table
  (`lookup32`), sharing the same 7-half-round RIPEMD-160 early-exit. Most keys are rejected by the
  first table with a single read-only cache load.
* **A prefix** (leading address characters) — `h[0]` is checked after half-rounds `77R`/`78L`
  (skipping 3 half-rounds) against the 16-bit prefix table.

A key that passes the GPU check is written to a small output buffer with its thread id and offset, so
the host can reconstruct the exact private key. The buffer has a fixed capacity (`-m`); if a single
launch somehow overflows it, the extra hits are **counted but not written** (you get a "items lost"
warning) rather than corrupting memory.

## The host confirms every hit

The tables compare only 16 or 32 bits of the hash160, so the GPU also reports hashes that merely share
those bits with a target. So before anything is reported, the CPU:

1. rebuilds the private key from the thread id and offset,
2. recomputes the public key and address in its own code,
3. compares that address to the target.

Only an exact match is printed and saved. This is also what the self-test relies on: it re-derives the
reported key's address in **pure Python**, independently of the tool, so a miscompiled build cannot
fool it. (If a CPU re-derivation ever disagrees with a GPU hit, the original candidate key is still
printed as `CandKey:` so an external check can inspect it.)

## Sequential vs random

* **Sequential** walks each thread's slice in order. Progress is well defined, so it can be saved and
  resumed (`-backup`), and the percentage and ETA are meaningful.
* **Random** (`-random`) makes every thread jump to the same random offset in its slice each step.
  Over a range too large to finish, this samples it uniformly; there is no progress to save. The
  offsets come from a 64-bit generator seeded with 256 bits of entropy, so separate processes and
  restarts do not repeat each other.

## Why Blackwell specifically

The kernel is limited by the GPU's ALU-heavy pipe (`SHF`, `LOP3`, `IADD3`, `LEA`), which is kept busy
by the rotations and boolean functions of SHA-256 and RIPEMD-160, while the FMA (multiply) pipe is
under-used. The Blackwell tuning replaces the 64-bit secp256k1 field arithmetic with native 32-bit
Comba `IMAD.WIDE.U32` multiplication and squaring, parallel `mul.wide` reduction (zero `IADD.64`),
branchless modular addition/subtraction, a barrier-free hot loop, RIPEMD-160 early-exit, SHA-256
constant/padding folding and critical-path reordering, `IMAD x·1 + y` additions, and 128 threads per
block. Net result: **22.9% fewer SM cycles per key** than upstream (**+23.7% MK/s** on an 80 W
RTX 5050 Laptop). The full story, including the ideas that did not pay off, is in
[OPTIMIZATION.md](../OPTIMIZATION.md).
