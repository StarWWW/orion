/*
 * This file is part of Orion (derived from VanitySearch, https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput, see OPTIMIZATION.md and the git history.
 *
 * Original VanitySearch portions are licensed under GPLv3; Orion modifications and the combined
 * work are licensed under GNU AGPLv3 (per GPLv3 section 13). See LICENSE.txt.
 */

#ifndef WIN64
#include <unistd.h>
#include <stdio.h>
#endif

#include "GPUEngine.h"
#include <cuda.h>
#include <cuda_runtime.h>
#include <stdint.h>
#include "../hash/sha256.h"
#include "../hash/ripemd160.h"
#include "../Timer.h"
#include "../Orion.h"

#include "GPUGroup.h"
#include "GPUMath.h"
#include "GPUCompute.h"
#include "GPUHash.h"
#include <iostream>

int _ConvertSMVer2Cores(int major, int minor) {

    // Defines for GPU Architecture types (using the SM version to determine
    // the # of cores per SM
    typedef struct {
        int SM;  // 0xMm (hexidecimal notation), M = SM Major version,
        // and m = SM minor version
        int Cores;
    } sSMtoCores;

    sSMtoCores nGpuArchCoresPerSM[] = {
        {0x60,  64},
        {0x61, 128},
        {0x62, 128},
        {0x70,  64},
        {0x72,  64},
        {0x75,  64},
        {0x80,  64},
        {0x86,  128},
        {0x87,  128},
        {0x89,  128},
        {0x90,  128},
        {0xa0,  128},
        {0xa1,  128},
        {0xa3,  128},
        {0xb0,  128},
        {0xc0,  128},
        {0xc1,  128},
        {-1, -1} };

    int index = 0;

    while (nGpuArchCoresPerSM[index].SM != -1) {
        if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor)) {
            return nGpuArchCoresPerSM[index].Cores;
        }

        index++;
    }

    return 0;

}



#define GRP_SIZE ORION_GRP
#define STEP_SIZE GRP_SIZE*1

// Internal sub-batch size (must divide ORION_GRP: 128, 256, 512, or 1024).
// Default 1024 achieves peak throughput on both clock-limited and power-capped Blackwell GPUs.
// Smaller values (e.g. 512/256/128) shrink subp[] to fit L2 cache and use WarpModInv32.
#ifndef ORION_SUBGRP
#define ORION_SUBGRP ORION_GRP
#endif
#if ORION_SUBGRP < 2 || ORION_SUBGRP > ORION_GRP || ORION_GRP % ORION_SUBGRP != 0
#error "ORION_SUBGRP must divide ORION_GRP (128, 256, 512 or 1024)"
#endif

// Optional single-pass pair EC (1 computes P+(i+1)G and P-(i+1)G together to fit 96 regs / 5 blk/SM;
// 0 interleaves EC+ -> Hash+ -> EC- -> Hash- which wins on both cycles and 80W wall-clock).
#ifndef ORION_PAIR_EC
#define ORION_PAIR_EC 0
#endif

// Launch configuration, overridable at build time (-DORION_TPB=... etc.)
#ifndef ORION_TPB
#define ORION_TPB 128
#endif
#ifndef ORION_BPSM
#define ORION_BPSM 256
#endif
#ifdef ORION_MINBLOCKS
#define ORION_BOUNDS __launch_bounds__(ORION_TPB, ORION_MINBLOCKS)
#else
#define ORION_BOUNDS
#endif

// Drop block barriers by default: Wide32 eliminates warp divergence in ModSub/ModAdd so
// independent warps naturally overlap EC (FMA) and Hash (ALU) without barrier stalls.
#ifndef ORION_NOBARRIER
#define ORION_NOBARRIER 1
#endif

#if ORION_NOBARRIER
#define ORION_SYNC()
#else
#define ORION_SYNC() __syncthreads()
#endif

template <int MODE>
__global__ void ORION_BOUNDS comp_keys(const address_t* __restrict__ sAddress,
                                    const uint32_t* __restrict__ lookup32,
                                    uint64_t* __restrict__ keys,
                                    uint32_t* __restrict__ out,
                                    const JumpPoint finalJump) {

    constexpr uint32_t SUB_M = ORION_SUBGRP / 2;
    constexpr uint32_t N_SUB = ORION_GRP / ORION_SUBGRP;

    uint64_t* startx = keys + (blockIdx.x * blockDim.x) * 8;
    uint64_t* starty = keys + (blockIdx.x * blockDim.x) * 8 + 4 * blockDim.x;

    uint32_t sx[8];
    uint32_t sy[8];

#if ORION_SUBGRP < ORION_GRP
    __shared__ uint32_t s_tree[ORION_TPB / 32][8][64];
    const uint32_t warpId = threadIdx.x >> 5;
#endif

    uint32_t inv32[8];
    uint32_t dx[8];
    uint32_t dy[8];
    uint32_t px[8];
    uint32_t py[8];
    uint32_t gx[8];
    uint32_t gy[8];
    uint32_t sx_gx[8];
    uint32_t odd_py;

    uint32_t subp[SUB_M][8];

    Load256A_32(sx, startx);
    Load256A_32(sy, starty);

    for (uint32_t sb = 0; sb < N_SUB; sb++) {
        const int32_t baseIncr = (int32_t)(sb * ORION_SUBGRP);
        const bool isLastSub = (sb == N_SUB - 1u);

        // Center point P
        _HashAndCheckComp<MODE>(sx, sy[0] & 1u, baseIncr + (int32_t)SUB_M, sAddress, lookup32, out);

        ORION_SYNC();

        // Pre-loop: product chain of denominators (jumpX - sx) and (Gx[i] - sx)
#if ORION_SUBGRP < ORION_GRP
        if (isLastSub) {
            Load256_32(gx, finalJump.x);
        } else {
            LoadConst256_32(gx, Gx[ORION_SUBGRP - 1]);
        }
#else
        Load256_32(gx, finalJump.x);
#endif
        ModSub256_32(dx, gx, sx);
        Load256_32(subp[SUB_M - 1], dx);

        for (uint32_t i = SUB_M - 1; i > 0; i--) {
            LoadConst256_32(gx, Gx[i]);
            ModSub256_32(dy, gx, sx);
            ModMult_Wide32(dx, dx, dy);
            Load256_32(subp[i - 1], dx);
        }

        LoadConst256_32(gx, Gx[0]);
        ModSub256_32(dy, gx, sx);
        ModMult_Wide32(inv32, dx, dy);

#if ORION_SUBGRP < ORION_GRP
        // Warp-cooperative shared-memory binary tree inversion (1 non-divergent _ModInv / 32 lanes)
        WarpModInv32(inv32, s_tree[warpId]);
#else
        {
            uint64_t inverse[5];
            inverse[0] = (uint64_t)inv32[0] | ((uint64_t)inv32[1] << 32);
            inverse[1] = (uint64_t)inv32[2] | ((uint64_t)inv32[3] << 32);
            inverse[2] = (uint64_t)inv32[4] | ((uint64_t)inv32[5] << 32);
            inverse[3] = (uint64_t)inv32[6] | ((uint64_t)inv32[7] << 32);
            inverse[4] = 0ULL;
            _ModInv(inverse);
            inv32[0] = (uint32_t)inverse[0]; inv32[1] = (uint32_t)(inverse[0] >> 32);
            inv32[2] = (uint32_t)inverse[1]; inv32[3] = (uint32_t)(inverse[1] >> 32);
            inv32[4] = (uint32_t)inverse[2]; inv32[5] = (uint32_t)(inverse[2] >> 32);
            inv32[6] = (uint32_t)inverse[3]; inv32[7] = (uint32_t)(inverse[3] >> 32);
        }
#endif

        ORION_SYNC();

        for (uint32_t i = 0; i < SUB_M - 1; i++) {

#if ORION_PAIR_EC
            uint32_t px2[8], odd_py2;
            ORION_SYNC();
            LoadConst256_32(gx, Gx[i]);
            LoadConst256_32(gy, Gy[i]);
            ModAdd256_32(sx_gx, gx, sx);
            ModMult_Wide32(dx, subp[i], inv32);

            // Key +: P + (i+1)G
            ModSub256_32(dy, gy, sy);
            ModMult_Wide32(dy, dy, dx);
            ModSqr_Wide32(px, dy);
            ModSub256_32(px, px, sx_gx);
            ModSub256_32(py, sx, px);
            ModMult_Wide32(py, py, dy);
            odd_py = ModSub256isOdd_32(py, sy);

            // Key -: P - (i+1)G (reuses gx, gy, sx_gx, dx)
            ModAdd256_32(dy, sy, gy);
            ModMult_Wide32(dy, dy, dx);
            ModSqr_Wide32(px2, dy);
            ModSub256_32(px2, px2, sx_gx);
            ModSub256_32(py, px2, sx);
            ModMult_Wide32(py, py, dy);
            odd_py2 = ModSub256isOdd_32(py, sy);

            // Advance inverse while gx and sx are in registers
            ModSub256_32(dx, gx, sx);
            ModMult_Wide32(inv32, inv32, dx);

            _HashAndCheckComp<MODE>(px, odd_py, baseIncr + (int32_t)(SUB_M + i + 1u), sAddress, lookup32, out);
            _HashAndCheckComp<MODE>(px2, odd_py2, baseIncr + (int32_t)(SUB_M - i - 1u), sAddress, lookup32, out);
#else
            ORION_SYNC();
            ModMult_Wide32(dx, subp[i], inv32);

            // Key +: P + (i+1)G
            LoadConst256_32(gx, Gx[i]);
            LoadConst256_32(gy, Gy[i]);
            ModAdd256_32(sx_gx, gx, sx);

            ModSub256_32(dy, gy, sy);
            ModMult_Wide32(dy, dy, dx);
            ModSqr_Wide32(px, dy);
            ModSub256_32(px, px, sx_gx);

            ModSub256_32(py, sx, px);
            ModMult_Wide32(py, py, dy);
            odd_py = ModSub256isOdd_32(py, sy);

            _HashAndCheckComp<MODE>(px, odd_py, baseIncr + (int32_t)(SUB_M + i + 1u), sAddress, lookup32, out);

            ORION_SYNC();

            // Key -: P - (i+1)G using m = (sy + Gy[i]) * dx
            LoadConst256_32(gx, Gx[i]);
            LoadConst256_32(gy, Gy[i]);
            ModAdd256_32(sx_gx, gx, sx);

            ModAdd256_32(dy, sy, gy);
            ModMult_Wide32(dy, dy, dx);
            ModSqr_Wide32(px, dy);
            ModSub256_32(px, px, sx_gx);

            ModSub256_32(py, px, sx);
            ModMult_Wide32(py, py, dy);
            odd_py = ModSub256isOdd_32(py, sy);

            ModSub256_32(dx, gx, sx);
            ModMult_Wide32(inv32, inv32, dx);

            _HashAndCheckComp<MODE>(px, odd_py, baseIncr + (int32_t)(SUB_M - i - 1u), sAddress, lookup32, out);
#endif

        }

        // Final key of sub-batch: P - SUB_M*G (incr = baseIncr + 0)
        ORION_SYNC();
        ModMult_Wide32(dx, subp[SUB_M - 1], inv32);

        LoadConst256_32(gx, Gx[SUB_M - 1]);
        LoadConst256_32(gy, Gy[SUB_M - 1]);
        ModAdd256_32(sx_gx, gx, sx);

        ModAdd256_32(dy, sy, gy);
        ModMult_Wide32(dy, dy, dx);
        ModSqr_Wide32(px, dy);
        ModSub256_32(px, px, sx_gx);

        ModSub256_32(py, px, sx);
        ModMult_Wide32(py, py, dy);
        odd_py = ModSub256isOdd_32(py, sy);

        // Advance inv32 to 1/(jumpX - sx)
        ModSub256_32(dx, gx, sx);
        ModMult_Wide32(inv32, inv32, dx);

        _HashAndCheckComp<MODE>(px, odd_py, baseIncr, sAddress, lookup32, out);

        // Advance center point P <- P + jumpPoint
        ORION_SYNC();
#if ORION_SUBGRP < ORION_GRP
        if (isLastSub) {
            Load256_32(gx, finalJump.x);
            Load256_32(gy, finalJump.y);
        } else {
            LoadConst256_32(gx, Gx[ORION_SUBGRP - 1]);
            LoadConst256_32(gy, Gy[ORION_SUBGRP - 1]);
        }
#else
        Load256_32(gx, finalJump.x);
        Load256_32(gy, finalJump.y);
#endif
        ModSub256_32(dy, gy, sy);
        ModMult_Wide32(dy, dy, inv32);
        ModSqr_Wide32(px, dy);
        ModSub256_32(px, px, sx);
        ModSub256_32(px, px, gx);

        ModSub256_32(py, gx, px);
        ModMult_Wide32(py, py, dy);
        ModSub256_32(py, py, gy);

        if (isLastSub) {
            Store256A_32(startx, px);
            Store256A_32(starty, py);
        } else {
            Load256_32(sx, px);
            Load256_32(sy, py);
        }
    }
}


// ---------------------------------------------------------------------------------------
static constexpr int NB_THREAD_PER_GROUP = ORION_TPB;

using namespace std;



GPUEngine::GPUEngine(int gpuId, uint32_t maxFound) {

    // Every buffer starts out unallocated so that the destructor is safe after a failed init
    inputAddress = NULL;
    inputAddressPinned = NULL;
    inputAddressLookUp = NULL;
    inputAddressLookUpPinned = NULL;
    inputKey = NULL;
    inputKeyPinned = NULL;
    outputBuffer = NULL;
    outputBufferPinned = NULL;
    initialised = false;
    lostWarning = false;
    nbThread = 0;
    launchBlocks = 0;
    this->maxFound = maxFound;
    outputSize = 0;

    int deviceCount = 0;
    cudaError_t error_id = cudaGetDeviceCount(&deviceCount);

    if (error_id != cudaSuccess) {
        fprintf(stderr, "GPUEngine: CudaGetDeviceCount %s\n", cudaGetErrorString(error_id));
        return;
    }

    // This function call returns 0 if there are no CUDA capable devices.
    if (deviceCount == 0) {
        fprintf(stderr, "GPUEngine: There are no available device(s) that support CUDA\n");
        return;
    }

    if (gpuId < 0 || gpuId >= deviceCount) {
        fprintf(stderr, "GPUEngine: invalid GPU id %d (%d device(s) found, use -l to list them)\n", gpuId, deviceCount);
        return;
    }

    cudaDeviceProp deviceProp;
    error_id = cudaGetDeviceProperties(&deviceProp, gpuId);
    if (error_id != cudaSuccess) {
        fprintf(stderr, "GPUEngine: cudaGetDeviceProperties %s\n", cudaGetErrorString(error_id));
        return;
    }

    int nbThreadGroup = deviceProp.multiProcessorCount * ORION_BPSM;

    if (!randomMode) {
        uint64_t powerOfTwo = 1;
        while (powerOfTwo <= (uint64_t)nbThreadGroup) {  // GET THE CLOSEST POWER OF 2
            powerOfTwo <<= 1;
        }

        powerOfTwo >>= 1;
        nbThreadGroup = (int)powerOfTwo;
    }


    // Initialise CUDA
    cudaError_t err;

    err = cudaSetDevice(gpuId);
    if (err != cudaSuccess) {
        fprintf(stderr, "GPUEngine: %s\n", cudaGetErrorString(err));
        return;
    }

    err = cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync);
    if (err != cudaSuccess) {
        fprintf(stderr, "GPUEngine: %s\n", cudaGetErrorString(err));
        return;
    }

   

    this->nbThread = nbThreadGroup * NB_THREAD_PER_GROUP;
    this->launchBlocks = nbThreadGroup;
    this->maxFound = maxFound;
    this->outputSize = (maxFound * ITEM_SIZE + 4);

    char tmp[512];
    sprintf(tmp,"GPU #%d %s (%dx%d cores) Grid(%dx%d)",
    gpuId,deviceProp.name,deviceProp.multiProcessorCount,
    _ConvertSMVer2Cores(deviceProp.major, deviceProp.minor),
    nbThread / NB_THREAD_PER_GROUP,
    NB_THREAD_PER_GROUP);

    deviceName = std::string(tmp);

    // Request max shared memory carveout (100 KB/SM on sm_120) when sub-batching uses SMEM;
    // otherwise prefer L1 cache for local-memory subp[].
#if ORION_SUBGRP < ORION_GRP
    cudaFuncSetAttribute(comp_keys<TARGET_SINGLE>,     cudaFuncAttributePreferredSharedMemoryCarveout, cudaSharedmemCarveoutMaxShared);
    cudaFuncSetAttribute(comp_keys<TARGET_MULTI_FULL>, cudaFuncAttributePreferredSharedMemoryCarveout, cudaSharedmemCarveoutMaxShared);
    cudaFuncSetAttribute(comp_keys<TARGET_PREFIX>,     cudaFuncAttributePreferredSharedMemoryCarveout, cudaSharedmemCarveoutMaxShared);
#else
    err = cudaDeviceSetCacheConfig(cudaFuncCachePreferL1);
    if (err != cudaSuccess) {
        printf("GPUEngine: %s\n", cudaGetErrorString(err));
        return;
    }
#endif

    // Allocate memory
    err = cudaMalloc((void**)&inputAddress, _64K * 2);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate address memory: %s\n", cudaGetErrorString(err));
        return;
    }
    err = cudaHostAlloc(&inputAddressPinned, _64K * 2, cudaHostAllocWriteCombined);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate address pinned memory: %s\n", cudaGetErrorString(err));
        return;
    }
    err = cudaMalloc((void**)&inputKey, (size_t)nbThread * 32 * 2);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate input memory: %s\n", cudaGetErrorString(err));
        return;
    }
    err = cudaHostAlloc(&inputKeyPinned, (size_t)nbThread * 32 * 2, cudaHostAllocWriteCombined);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate input pinned memory: %s\n", cudaGetErrorString(err));
        return;
    }
    err = cudaMalloc((void**)&outputBuffer, outputSize);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate output memory: %s\n", cudaGetErrorString(err));
        return;
    }
    err = cudaHostAlloc(&outputBufferPinned, outputSize, cudaHostAllocDefault);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate output pinned memory: %s\n", cudaGetErrorString(err));
        return;
    }

    // The kernel drops hits beyond the output buffer instead of writing past its end
    err = cudaMemcpyToSymbol(_maxFound, &maxFound, sizeof(maxFound));
    if (err != cudaSuccess) {
        printf("GPUEngine: Set maxFound: %s\n", cudaGetErrorString(err));
        return;
    }

    targetMode = TARGET_PREFIX;

    // Default finalJump = ORION_SUBGRP * G (so N_SUB sub-batches advance by STEP_SIZE = ORION_GRP)
    {
        uint64_t jx[4], jy[4];
#if ORION_SUBGRP == ORION_GRP
        cudaMemcpyFromSymbol(jx, _2Gnx, sizeof(jx));
        cudaMemcpyFromSymbol(jy, _2Gny, sizeof(jy));
#else
        cudaMemcpyFromSymbol(jx, Gx, sizeof(jx), (size_t)(ORION_SUBGRP - 1) * sizeof(jx));
        cudaMemcpyFromSymbol(jy, Gy, sizeof(jy), (size_t)(ORION_SUBGRP - 1) * sizeof(jy));
#endif
        for (int k = 0; k < 4; k++) {
            finalJump.x[2 * k]     = (uint32_t)jx[k];
            finalJump.x[2 * k + 1] = (uint32_t)(jx[k] >> 32);
            finalJump.y[2 * k]     = (uint32_t)jy[k];
            finalJump.y[2 * k + 1] = (uint32_t)(jy[k] >> 32);
        }
    }

    initialised = true;

}

GPUEngine::~GPUEngine() {

    ReleaseBuffers();

}

void GPUEngine::ReleaseBuffers() {

    if (inputKey) cudaFree(inputKey);
    if (inputAddress) cudaFree(inputAddress);
    if (inputAddressLookUp) cudaFree(inputAddressLookUp);
    if (outputBuffer) cudaFree(outputBuffer);
    if (inputAddressPinned) cudaFreeHost(inputAddressPinned);
    if (inputAddressLookUpPinned) cudaFreeHost(inputAddressLookUpPinned);
    if (inputKeyPinned) cudaFreeHost(inputKeyPinned);
    if (outputBufferPinned) cudaFreeHost(outputBufferPinned);

    inputKey = NULL;
    inputAddress = NULL;
    inputAddressLookUp = NULL;
    outputBuffer = NULL;
    inputAddressPinned = NULL;
    inputAddressLookUpPinned = NULL;
    inputKeyPinned = NULL;
    outputBufferPinned = NULL;

}

bool GPUEngine::IsInitialised() {
    return initialised;
}

void GPUEngine::PrintCudaInfo() {

    int deviceCount = 0;
    cudaError_t error_id = cudaGetDeviceCount(&deviceCount);
    if (error_id != cudaSuccess) {
        fprintf(stderr, "GPUEngine: CudaGetDeviceCount %s\n", cudaGetErrorString(error_id));
        return;
    }

    if (deviceCount == 0) {
        printf("No CUDA capable device found\n");
        return;
    }

    for (int i = 0;i < deviceCount;i++) {

        cudaDeviceProp deviceProp;
        if (cudaGetDeviceProperties(&deviceProp, i) != cudaSuccess)
            continue;

        printf("GPU #%d %s (sm_%d%d, %d SMs, %.1f GB)\n", i, deviceProp.name, deviceProp.major, deviceProp.minor,
            deviceProp.multiProcessorCount, (double)deviceProp.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));
        if (jsonMode) {
            char nums[128];
            snprintf(nums, sizeof(nums), ",\"sm\":%d,\"sms\":%d,\"memory\":%.0f}",
                deviceProp.major * 10 + deviceProp.minor, deviceProp.multiProcessorCount,
                (double)deviceProp.totalGlobalMem);
            emitJson("{\"event\":\"gpu\",\"id\":" + std::to_string(i) + ",\"name\":" + jsonString(deviceProp.name) + nums);
        }

    }

}



int GPUEngine::GetNbThread() {
    return nbThread;
}

int GPUEngine::GetLaunchThreads() {
    return launchBlocks * NB_THREAD_PER_GROUP;
}

void GPUEngine::SetActiveThreads(uint64_t activeThreads) {
    uint64_t maxBlocks = (uint64_t)(nbThread / NB_THREAD_PER_GROUP);
    uint64_t blocks = (activeThreads + (uint64_t)NB_THREAD_PER_GROUP - 1ULL) / (uint64_t)NB_THREAD_PER_GROUP;
    if (blocks < 1ULL) blocks = 1ULL;
    if (blocks > maxBlocks) blocks = maxBlocks;
    launchBlocks = (int)blocks;
}

bool GPUEngine::SetAddress(std::vector<address_t> addresses) {

    memset(inputAddressPinned, 0, _64K * 2);
    for (int i = 0;i < (int)addresses.size();i++)
        inputAddressPinned[addresses[i]] = 1;

    // Fill device memory
    cudaMemcpy(inputAddress, inputAddressPinned, _64K * 2, cudaMemcpyHostToDevice);

    // We do not need the input pinned memory anymore
    cudaFreeHost(inputAddressPinned);
    inputAddressPinned = NULL;
    targetMode = TARGET_PREFIX;
    lostWarning = false;

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("GPUEngine: SetAddress: %s\n", cudaGetErrorString(err));
        return false;
    }
    return true;

}

bool GPUEngine::SetAddress(std::vector<LADDRESS> addresses, uint32_t totalAddress) {

    // Allocate memory for the second level of lookup tables
    cudaError_t err = cudaMalloc((void**)&inputAddressLookUp, (_64K + totalAddress) * 4);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate address lookup memory: %s\n", cudaGetErrorString(err));
        return false;
    }
    err = cudaHostAlloc(&inputAddressLookUpPinned, (size_t)(_64K + totalAddress) * 4, cudaHostAllocWriteCombined);
    if (err != cudaSuccess) {
        printf("GPUEngine: Allocate address lookup pinned memory: %s\n", cudaGetErrorString(err));
        return false;
    }

    uint32_t offset = _64K;
    memset(inputAddressPinned, 0, _64K * 2);
    memset(inputAddressLookUpPinned, 0, _64K * 4);
    for (int i = 0; i < (int)addresses.size(); i++) {
        size_t nbLAddress = addresses[i].lAddresses.size();
        if (nbLAddress > 65535) {
            printf("GPUEngine: lookup16 bucket %u overflow (%zu > 65535 addresses)\n",
                   (unsigned)addresses[i].sAddress, nbLAddress);
            return false;
        }
        inputAddressPinned[addresses[i].sAddress] = (uint16_t)nbLAddress;
        inputAddressLookUpPinned[addresses[i].sAddress] = offset;
        for (size_t j = 0; j < nbLAddress; j++) {
            inputAddressLookUpPinned[offset++] = addresses[i].lAddresses[j];
        }
    }

    if (offset != (_64K + totalAddress)) {
        printf("GPUEngine: Wrong totalAddress %d!=%d!\n", offset - _64K, totalAddress);
        return false;
    }

    // Fill device memory
    cudaMemcpy(inputAddress, inputAddressPinned, _64K * 2, cudaMemcpyHostToDevice);
    cudaMemcpy(inputAddressLookUp, inputAddressLookUpPinned, (_64K + totalAddress) * 4, cudaMemcpyHostToDevice);


    // We do not need the input pinned memory anymore
    cudaFreeHost(inputAddressPinned);
    inputAddressPinned = NULL;
    cudaFreeHost(inputAddressLookUpPinned);
    inputAddressLookUpPinned = NULL;
    targetMode = TARGET_MULTI_FULL;
    lostWarning = false;

    err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("GPUEngine: SetAddress (large): %s\n", cudaGetErrorString(err));
        return false;
    }
    return true;

}

bool GPUEngine::SetSingleTarget(const uint8_t* hash160) {

    uint32_t h[5];
    memcpy(h, hash160, 20);
    uint32_t h2_noIV = h[2] - 0x10325476u;
    cudaMemcpyToSymbol(_targetH160, h, sizeof(h));
    cudaMemcpyToSymbol(_targetH2_noIV, &h2_noIV, sizeof(h2_noIV));
    targetMode = TARGET_SINGLE;

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("GPUEngine: SetSingleTarget: %s\n", cudaGetErrorString(err));
        return false;
    }
    return true;

}

int GPUEngine::GetStepSize() {

    return STEP_SIZE;

}

int GPUEngine::GetGroupSize() {

    return ORION_SUBGRP;

}


bool GPUEngine::callKernel() {


    // Reset nbFound asynchronously (zero host sync!)
    cudaMemsetAsync(outputBuffer, 0, 4);

    if (targetMode == TARGET_SINGLE) {
        comp_keys<TARGET_SINGLE> <<< launchBlocks, NB_THREAD_PER_GROUP >>>
            (inputAddress, inputAddressLookUp, inputKey, outputBuffer, finalJump);
    } else if (targetMode == TARGET_MULTI_FULL) {
        comp_keys<TARGET_MULTI_FULL> <<< launchBlocks, NB_THREAD_PER_GROUP >>>
            (inputAddress, inputAddressLookUp, inputKey, outputBuffer, finalJump);
    } else {
        comp_keys<TARGET_PREFIX> <<< launchBlocks, NB_THREAD_PER_GROUP >>>
            (inputAddress, inputAddressLookUp, inputKey, outputBuffer, finalJump);
    }

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("GPUEngine: Kernel: %s\n", cudaGetErrorString(err));
        return false;
    }

    return true;

}


bool GPUEngine::SetKeys(Point* p) {

    // Sets the starting keys for each thread
    // p must contains nbThread public keys

    for (int i = 0; i < nbThread; i += NB_THREAD_PER_GROUP) {
        for (int j = 0; j < NB_THREAD_PER_GROUP; j++) {

            inputKeyPinned[8 * i + j + 0 * NB_THREAD_PER_GROUP] = p[i + j].x.bits64[0];
            inputKeyPinned[8 * i + j + 1 * NB_THREAD_PER_GROUP] = p[i + j].x.bits64[1];
            inputKeyPinned[8 * i + j + 2 * NB_THREAD_PER_GROUP] = p[i + j].x.bits64[2];
            inputKeyPinned[8 * i + j + 3 * NB_THREAD_PER_GROUP] = p[i + j].x.bits64[3];

            inputKeyPinned[8 * i + j + 4 * NB_THREAD_PER_GROUP] = p[i + j].y.bits64[0];
            inputKeyPinned[8 * i + j + 5 * NB_THREAD_PER_GROUP] = p[i + j].y.bits64[1];
            inputKeyPinned[8 * i + j + 6 * NB_THREAD_PER_GROUP] = p[i + j].y.bits64[2];
            inputKeyPinned[8 * i + j + 7 * NB_THREAD_PER_GROUP] = p[i + j].y.bits64[3];

        }
    }

    // Fill device memory

    cudaMemcpy(inputKey, inputKeyPinned, nbThread * 32 * 2, cudaMemcpyHostToDevice);
    // We do not need the input pinned memory anymore
    cudaFreeHost(inputKeyPinned);
    inputKeyPinned = NULL;

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("GPUEngine: SetKeys: %s\n", cudaGetErrorString(err));
        return false;
    }

    return callKernel();

}


bool GPUEngine::SetRandomJump(Point p) {

    // Stored on host and passed by value in kernel args: avoids blocking cudaMemcpyToSymbol!
    for (int k = 0; k < 4; k++) {
        finalJump.x[2 * k]     = (uint32_t)p.x.bits64[k];
        finalJump.x[2 * k + 1] = (uint32_t)(p.x.bits64[k] >> 32);
        finalJump.y[2 * k]     = (uint32_t)p.y.bits64[k];
        finalJump.y[2 * k + 1] = (uint32_t)(p.y.bits64[k] >> 32);
    }
    return true;

}



bool GPUEngine::Launch(std::vector<ITEM>& addressFound) {

    addressFound.clear();

    // Waits for the kernel (uses BlockingSync); only the hit counter is needed here, payloads are copied below
    cudaMemcpy(outputBufferPinned, outputBuffer, 4, cudaMemcpyDeviceToHost);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
      printf("GPUEngine: Launch: %s\n", cudaGetErrorString(err));
      return false;
    }

    // Look for address found
    uint32_t nbFound = outputBufferPinned[0];

    if (nbFound > maxFound) {
      // address has been lost
      if (!lostWarning) {
        printf("\nWarning, %u items lost\nHint: Search with less addresses/prefixes or increase maxFound (-m)\n", (nbFound - maxFound));
        lostWarning = true;
      }
      nbFound = maxFound;
    }

    // The kernel has ended; only fetch hit payloads when at least one hit was recorded
    if (nbFound > 0) {
        cudaMemcpy(outputBufferPinned + 1, outputBuffer + 1, nbFound * ITEM_SIZE, cudaMemcpyDeviceToHost);
    }

    for (uint32_t i = 0; i < nbFound; i++) {
        uint32_t* itemPtr = outputBufferPinned + (i * ITEM_SIZE32 + 1);
        ITEM it;
        it.thId = itemPtr[0];
        it.incr = (int32_t)itemPtr[1];
        it.hash = (uint8_t*)(itemPtr + 2);
        addressFound.push_back(it);
    }

    return callKernel();

}

void GPUEngine::FreeGPUEngine() {  //free gpu for Pause function

    // Ensure all operations have completed before freeing memory
    cudaDeviceSynchronize();

    ReleaseBuffers();
    initialised = false;

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("GPUEngine: Error freeing memory: %s\n", cudaGetErrorString(err));
    }

    cudaDeviceReset();

}
