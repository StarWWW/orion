/*
 * This file is part of Orion (derived from VanitySearch, https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput, see OPTIMIZATION.md and the git history.
 *
 * Original VanitySearch portions are licensed under GPLv3; Orion modifications and the combined
 * work are licensed under GNU AGPLv3 (per GPLv3 section 13). See LICENSE.txt.
 */

// Single full-address target in constant memory:
// _targetH2_noIV = _targetH160[2] - 0x10325476u so the hot path skips adding RIPEMD IV3.
__device__ __constant__ uint32_t _targetH160[5];
__device__ __constant__ uint32_t _targetH2_noIV = 0;

// Capacity of the output buffer in items: hits past it are counted but not written.
__device__ __constant__ uint32_t _maxFound = 0;

__device__ __forceinline__ void RecordHit(uint32_t h0, uint32_t h1, uint32_t h2, uint32_t h3, uint32_t h4,
                                          int32_t incr, uint32_t *out) {
    uint32_t pos = atomicAdd(out, 1);
    if (pos < _maxFound) {
        uint32_t tid = (blockIdx.x * blockDim.x) + threadIdx.x;
        out[pos * ITEM_SIZE32 + 1] = tid;
        out[pos * ITEM_SIZE32 + 2] = (uint32_t)incr;
        out[pos * ITEM_SIZE32 + 3] = h0;
        out[pos * ITEM_SIZE32 + 4] = h1;
        out[pos * ITEM_SIZE32 + 5] = h2;
        out[pos * ITEM_SIZE32 + 6] = h3;
        out[pos * ITEM_SIZE32 + 7] = h4;
    }
}
