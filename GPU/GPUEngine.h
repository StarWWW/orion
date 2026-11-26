/*
 * This file is part of Orion (derived from VanitySearch, https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput, see OPTIMIZATION.md and the git history.
 *
 * Original VanitySearch portions are licensed under GPLv3; Orion modifications and the combined
 * work are licensed under GNU AGPLv3 (per GPLv3 section 13). See LICENSE.txt.
 */

#ifndef GPUENGINEH
#define GPUENGINEH

#include <vector>
#include "../SECP256k1.h"

#define ITEM_SIZE 28
#define ITEM_SIZE32 (ITEM_SIZE/4)
#define _64K 65536

typedef uint16_t address_t;
typedef uint32_t addressl_t;

typedef struct {
  uint32_t thId;
  int32_t  incr;
  uint8_t  *hash;
} ITEM;

// Second level lookup
typedef struct {
  address_t sAddress;
  std::vector<addressl_t> lAddresses;
} LADDRESS;

// Final-subbatch jump point passed by value in kernel args (avoids sync cudaMemcpyToSymbol in -random mode)
typedef struct {
  uint32_t x[8];
  uint32_t y[8];
} JumpPoint;

class GPUEngine {

public:

  GPUEngine(int gpuId, uint32_t maxFound);
  ~GPUEngine();
  void FreeGPUEngine();
  bool SetAddress(std::vector<address_t> addresses);
  bool SetAddress(std::vector<LADDRESS> addresses, uint32_t totalAddress);
  bool SetSingleTarget(const uint8_t *hash160);
  bool SetKeys(Point *p);
  bool SetRandomJump(Point p);
  void SetActiveThreads(uint64_t activeThreads);
  bool Launch(std::vector<ITEM> &addressFound);
  int GetNbThread();
  int GetLaunchThreads();
  int GetGroupSize();
  int GetStepSize();
  bool IsInitialised();

  std::string deviceName;

  static void PrintCudaInfo();

private:

  bool callKernel();
  void ReleaseBuffers();

  int nbThread;
  int launchBlocks;
  address_t *inputAddress;
  address_t *inputAddressPinned;
  uint32_t *inputAddressLookUp;
  uint32_t *inputAddressLookUpPinned;
  uint64_t *inputKey;
  uint64_t *inputKeyPinned;
  uint32_t *outputBuffer;
  uint32_t *outputBufferPinned;
  bool initialised;
  int targetMode;
  JumpPoint finalJump;
  bool lostWarning;
  uint32_t maxFound;
  uint32_t outputSize;

};

#endif // GPUENGINEH
