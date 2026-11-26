/*
 * This file is part of Orion (derived from VanitySearch, https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput and robustness, see OPTIMIZATION.md and the git history.
 *
 * Original VanitySearch portions are licensed under GPLv3; Orion modifications and the combined
 * work are licensed under GNU AGPLv3 (per GPLv3 section 13). See LICENSE.txt.
 */

#ifndef ORION_H
#define ORION_H

#include <string>
#include <vector>
#include <atomic>
#include <cstdint>
#include "SECP256k1.h"
#include "GPU/GPUEngine.h"

extern std::atomic<bool> Pause;          // toggled by the 'p' key
extern std::atomic<bool> stopRequested;  // set by Ctrl+C / SIGTERM
extern uint64_t idxcount;                // kernel launches completed (sequential progress)
extern double t_Paused;                  // run time accumulated before this (re)start
extern uint32_t backupThreads;           // GPU thread count recorded in the loaded backup, 0 if unknown
extern uint32_t backupStepSize;          // keys per thread per launch recorded in the loaded backup, 0 if unknown
extern bool randomMode;

typedef struct {

	std::string address;
	bool        found;
	bool        isFull;
	uint8_t     hash160[20];

} ADDRESS_TARGET;

typedef struct {

	Int ksStart;
	Int ksFinish;

} SEARCH_RANGE;

// Sequential progress saved to orion_backup_gpu<N>.dat
typedef struct {

	uint64_t idxcount;   // kernel launches completed
	double   elapsed;    // run time in seconds
	Int      start;      // keyspace the progress belongs to
	Int      finish;
	uint32_t nbThread;   // GPU threads (the key layout depends on it)
	uint32_t stepSize;   // keys per thread per launch
	bool     legacy;     // old 12-byte file without range/layout, cannot be validated

} BACKUP_STATE;

std::string backupFileName(int gpuId);
bool readBackup(const std::string& fileName, BACKUP_STATE* st);
bool writeBackup(const std::string& fileName, const BACKUP_STATE* st);

// How the search ended
enum {
	SEARCH_RUNNING = 0,
	SEARCH_FINISHED,     // the whole range has been scanned
	SEARCH_ALL_FOUND,    // -stop and every address was found
	SEARCH_PAUSED,       // 'p' pressed, GPU released
	SEARCH_STOPPED,      // Ctrl+C
	SEARCH_ERROR
};

class OrionSearch {

public:

	OrionSearch(Secp256K1* secp, std::vector<std::string>& address,
		bool stop, std::string outputFile, uint32_t maxFound, SEARCH_RANGE* range);
	~OrionSearch();

	// Can be called again after SEARCH_PAUSED: the found state is kept across the calls
	int Search(int gpuId);
	int GetNbFound() { return nbFoundKey; }

private:

	void FindKeyGPU(int gpuId);
	bool checkPrivKey(const std::string& addr, const Int& key);
	void checkAddr(int prefIdx, const uint8_t* hash160, const Int& key);
	void scanCPUWindow(const Int& firstKey, const Int& lastKey, const Int* skipKey);
	void output(const std::string& addr, const std::string& pAddr, const std::string& pAddrHex, const std::string& pubKey);

	bool isSingularAddress(const std::string& pref);
	bool initAddress(std::string& address, ADDRESS_TARGET* target, std::vector<address_t>& buckets);
	void getThreadStep(int nbThread, int minStep, Int* stepThread);
	void getGPUStartingKeys(const Int& tRangeStart, const Int& stepThread, int groupSize, int nbThread, Point* publicKeys, const Int& progress);
	void PrintStats(double keysDone, double speed, double ttot, double taskSize, bool isRandom);
	void saveBackup(int nbThread, int stepSize, double elapsed, int gpuid);

	Secp256K1* secp;
	int searchType;
	bool stopWhenFound;
	bool endOfSearch;
	int searchStatus;
	int nbFoundKey;
	uint32_t nbUniqueFound;
	uint32_t nbAddress;
	std::string outputFile;
	bool onlyFull;
	uint32_t maxFound;

	std::vector<ADDRESS_TARGET> targets;
	std::vector<std::vector<uint32_t>> buckets16;
	std::vector<address_t> usedAddress;
	std::vector<LADDRESS> usedAddressL;

	SEARCH_RANGE* bc;
};

#endif // ORION_H
