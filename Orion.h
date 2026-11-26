/*
 * This file is part of the VanitySearch distribution (https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput and robustness, see the git history.
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

#ifndef ORION_H
#define ORION_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include "SECP256k1.h"
#include "GPU/GPUEngine.h"

extern std::atomic<bool> Pause;          // toggled by the 'p' key
extern std::atomic<bool> Paused;         // set by the GPU thread once the GPU has been released
extern std::atomic<bool> stopRequested;  // set by Ctrl+C / SIGTERM
extern uint64_t idxcount;                // kernel launches completed (sequential progress)
extern double t_Paused;                  // run time accumulated before this (re)start
extern uint32_t backupThreads;           // GPU thread count recorded in the loaded backup, 0 if unknown
extern uint32_t backupStepSize;          // keys per thread per launch recorded in the loaded backup, 0 if unknown
extern bool randomMode;

class OrionSearch;

typedef struct {

	OrionSearch* obj;
	int  threadId;
	std::atomic<bool> isRunning;
	std::atomic<bool> hasStarted;
	int  gpuId;

} TH_PARAM;

typedef struct {

	char* address;
	int addressLength;
	address_t sAddress;
	bool found;

	bool isFull;
	addressl_t lAddress;
	uint8_t hash160[20];

} ADDRESS_ITEM;

typedef struct {

	std::vector<ADDRESS_ITEM>* items;
	bool found;

} ADDRESS_TABLE_ITEM;

typedef struct {

	Int  ksStart;
	Int  ksNext;
	Int  ksFinish;

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

	OrionSearch(Secp256K1* secp, std::vector<std::string>& address, int searchMode,
		bool stop, std::string outputFile, uint32_t maxFound, SEARCH_RANGE* range);
	~OrionSearch();

	// Can be called again after SEARCH_PAUSED: the found state is kept across the calls
	int Search(int gpuId);
	void FindKeyGPU(TH_PARAM* p);
	int GetNbFound() { return nbFoundKey; }

private:

	bool checkPrivKey(std::string addr, Int& key, int32_t incr, int endomorphism, bool mode);
	void checkAddr(int prefIdx, uint8_t* hash160, Int& key, int32_t incr, int endomorphism, bool mode);
	void output(std::string addr, std::string pAddr, std::string pAddrHex, std::string pubKey);

	std::mutex outputMutex;

	bool isSingularAddress(std::string pref);
	bool initAddress(std::string& address, ADDRESS_ITEM* it);
	void updateFound();
	void getThreadStep(int nbThread, Int* stepThread);
	void getGPUStartingKeys(Int& tRangeStart, Int& tRangeEnd, int groupSize, int numThreadsGPU, Point* publicKeys, uint64_t Progress);
	void PrintStats(double keysDone, double speed, double ttot, double taskSize);
	void saveBackup(int nbThread, int stepSize, double elapsed, int gpuid);

	Secp256K1* secp;
	Int startKey;
	int searchType;
	int searchMode;
	bool stopWhenFound;
	std::atomic<bool> endOfSearch;
	std::atomic<int> searchStatus;
	int numGPUs;
	int nbFoundKey;
	uint32_t nbAddress;
	std::string outputFile;
	bool onlyFull;
	uint32_t maxFound;
	std::vector<ADDRESS_TABLE_ITEM> addresses;
	std::vector<address_t> usedAddress;
	std::vector<LADDRESS> usedAddressL;
	std::vector<std::string>& inputAddresses;

	SEARCH_RANGE* bc;

	Int beta;
	Int lambda;
	Int beta2;
	Int lambda2;
};

#endif // ORION_H
