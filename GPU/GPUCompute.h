/*
 * This file is part of the VanitySearch distribution (https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput, see OPTIMIZATION.md and the git history.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
        out[pos * ITEM_SIZE32 + 2] = (uint32_t)(incr << 16) | (uint32_t)(1 << 15);
        out[pos * ITEM_SIZE32 + 3] = h0;
        out[pos * ITEM_SIZE32 + 4] = h1;
        out[pos * ITEM_SIZE32 + 5] = h2;
        out[pos * ITEM_SIZE32 + 6] = h3;
        out[pos * ITEM_SIZE32 + 7] = h4;
    }
}
