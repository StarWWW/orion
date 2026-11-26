/*
 * This file is part of Orion (derived from VanitySearch, https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput and robustness, see OPTIMIZATION.md and the git history.
 *
 * Original VanitySearch portions are licensed under GPLv3; Orion modifications and the combined
 * work are licensed under GNU AGPLv3 (per GPLv3 section 13). See LICENSE.txt.
 */

#include "Orion.h"
#include "Base58.h"
#include "Bech32.h"
#include "hash/sha256.h"
#include "hash/ripemd160.h"
#include "Timer.h"
#include <string.h>
#include <math.h>
#include <algorithm>
#include <thread>
#include <atomic>
#include <ctime>
#include <iostream>
#include <fstream>
#include <random>
#include <stdio.h>
#include <cctype>
#include <unordered_set>
#ifdef WIN64
#include <Windows.h>
#endif

using namespace std;

static inline address_t readLE16(const uint8_t* p) {
	return (address_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline addressl_t readLE32(const uint8_t* p) {
	return (addressl_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

// ----------------------------------------------------------------------------

OrionSearch::OrionSearch(Secp256K1* secp, vector<std::string>& inputAddresses,
	bool stop, string outputFile, uint32_t maxFound, SEARCH_RANGE* bc)
{
	this->secp = secp;
	this->stopWhenFound = stop;
	this->outputFile = outputFile;
	this->maxFound = maxFound;
	this->searchType = -1;
	this->bc = bc;
	this->endOfSearch = false;
	this->searchStatus = SEARCH_RUNNING;
	this->nbFoundKey = 0;
	this->nbUniqueFound = 0;

	buckets16.assign(65536, {});

	bool loadingProgress = (inputAddresses.size() > 1000);
	if (loadingProgress)
		fprintf(stdout, "[Building lookup16   0.0%%]\r");

	nbAddress = 0;
	onlyFull = true;
	size_t nbDuplicates = 0;
	vector<string> accepted;
	unordered_set<string> seenAddresses;
	seenAddresses.reserve(inputAddresses.size());
	vector<uint8_t> bucketUsed(65536, 0);

	for (size_t i = 0; i < inputAddresses.size(); i++)
	{
		string addr = inputAddresses[i];
		ADDRESS_TARGET target;
		vector<address_t> addrBuckets;

		if (initAddress(addr, &target, addrBuckets)) {
			if (!seenAddresses.insert(target.address).second) {
				nbDuplicates++;
			}
			else {
				uint32_t targetIdx = (uint32_t)targets.size();
				targets.push_back(target);

				for (address_t p : addrBuckets) {
					if (!bucketUsed[p]) {
						bucketUsed[p] = 1;
						usedAddress.push_back(p);
					}
					buckets16[p].push_back(targetIdx);
				}

				onlyFull &= target.isFull;
				nbAddress++;
				if (accepted.size() < 10)
					accepted.push_back(target.address);
			}
		}

		if (loadingProgress && (i % 1000 == 0) && inputAddresses.size() > 1)
			fprintf(stdout, "[Building lookup16 %5.1f%%]\r", ((double)i / (double)(inputAddresses.size() - 1)) * 100.0);
	}

	if (loadingProgress)
		fprintf(stdout, "\n");

	if (nbDuplicates > 0)
		fprintf(stdout, "[input]     skipped %zu duplicate address(es)\n", nbDuplicates);

	if (nbAddress == 0)
	{
		fprintf(stderr, "[ERROR] Orion: nothing to search !\n");
		exit(1);
	}

	// Second level lookup: when searching multiple full addresses, index by hash160 word 2
	// (bytes 8..11) so the GPU can early-exit 7 RIPEMD-160 half-rounds before computing h[0].
	uint32_t unique_sAddress = 0;
	uint32_t minI = 0xFFFFFFFFu;
	uint32_t maxI = 0;
	if (onlyFull) {
		vector<vector<addressl_t>> h2Buckets(65536);
		for (size_t i = 0; i < targets.size(); i++) {
			const uint8_t* h160 = targets[i].hash160;
			address_t h2s = readLE16(h160 + 8);
			addressl_t h2l = readLE32(h160 + 8);
			h2Buckets[h2s].push_back(h2l);
		}
		for (int i = 0; i < 65536; i++) {
			if (!h2Buckets[i].empty()) {
				LADDRESS lit;
				lit.sAddress = (address_t)i;
				lit.lAddresses = std::move(h2Buckets[i]);
				sort(lit.lAddresses.begin(), lit.lAddresses.end());
				if ((uint32_t)lit.lAddresses.size() > maxI) maxI = (uint32_t)lit.lAddresses.size();
				if ((uint32_t)lit.lAddresses.size() < minI) minI = (uint32_t)lit.lAddresses.size();
				usedAddressL.push_back(std::move(lit));
				unique_sAddress++;
			}
			if (loadingProgress && (i % 4096) == 0)
				fprintf(stdout, "[Building lookup32 %.1f%%]\r", ((double)i * 100.0) / 65536.0);
		}
	}
	else {
		for (int i = 0; i < 65536; i++) {
			if (!buckets16[i].empty()) {
				uint32_t sz = (uint32_t)buckets16[i].size();
				if (sz > maxI) maxI = sz;
				if (sz < minI) minI = sz;
				unique_sAddress++;
			}
		}
	}

	if (loadingProgress && onlyFull)
		fprintf(stdout, "\n");

	if (nbAddress < 10)
	{
		for (size_t i = 0; i < accepted.size(); i++)
		{
			fprintf(stdout, "Search: %s [Compressed]\n", accepted[i].c_str());
		}
	}
	else
	{
		fprintf(stdout, "Search: %u (Lookup size %u,[%u,%u]) [Compressed]\n", nbAddress, unique_sAddress, minI, maxI);
	}

	time_t now = time(NULL);
	fprintf(stdout, "Current task START time: %s", ctime(&now));
	fflush(stdout);
}

OrionSearch::~OrionSearch() {
}

bool OrionSearch::isSingularAddress(const std::string& pref) {

	for (char c : pref) {
		if (c != '1')
			return false;
	}
	return !pref.empty();
}

bool OrionSearch::initAddress(std::string& address, ADDRESS_TARGET* target, std::vector<address_t>& buckets) {

	buckets.clear();
	target->found = false;
	target->isFull = false;
	memset(target->hash160, 0, sizeof(target->hash160));

	if (address.length() < 2) {
		fprintf(stdout, "Ignoring address \"%s\" (too short)\n", address.c_str());
		return false;
	}

	int aType = -1;

	switch (address[0]) {
	case '1':
		aType = P2PKH;
		break;
	case '3':
		// The GPU kernel hashes the compressed public key only (P2PKH / P2WPKH),
		// a P2SH script hash can never match
		fprintf(stdout, "Ignoring address \"%s\" (P2SH addresses are not supported, use 1... or bc1q...)\n", address.c_str());
		return false;
	case 'b':
	case 'B': {
		// BIP-173 forbids mixed case in Bech32 strings
		bool hasLower = false, hasUpper = false;
		for (unsigned char c : address) {
			if (c >= 'a' && c <= 'z') hasLower = true;
			else if (c >= 'A' && c <= 'Z') hasUpper = true;
		}
		if (hasLower && hasUpper) {
			fprintf(stdout, "Ignoring address \"%s\" (mixed case is invalid in Bech32)\n", address.c_str());
			return false;
		}
		std::transform(address.begin(), address.end(), address.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
		if (strncmp(address.c_str(), "bc1q", 4) == 0)
			aType = BECH32;
		break;
	}
	}

	if (aType == -1) {
		fprintf(stdout, "Ignoring address \"%s\" (must start with 1 or bc1q)\n", address.c_str());
		return false;
	}

	if (searchType == -1) searchType = aType;
	if (aType != searchType) {
		fprintf(stdout, "Ignoring address \"%s\" (P2PKH or BECH32 allowed at once, not both)\n", address.c_str());
		return false;
	}

	if (aType == BECH32) {

		uint8_t witprog[40];
		size_t witprog_len = 0;
		int witver = -1;
		const char* hrp = "bc";

		int ret = segwit_addr_decode(&witver, witprog, &witprog_len, hrp, address.c_str());

		if (ret && witver == 0 && witprog_len == 20) {
			target->isFull = true;
			memcpy(target->hash160, witprog, 20);
			target->address = address;
			buckets.push_back(readLE16(target->hash160));
			return true;
		}

		if (ret && witver == 0 && witprog_len == 32) {
			fprintf(stdout, "Ignoring address \"%s\" (P2WSH addresses are not supported, use 1... or 42-char bc1q...)\n", address.c_str());
			return false;
		}

		if (address.length() == 42) {
			fprintf(stdout, "Ignoring address \"%s\" (wrong checksum, mistyped address?)\n", address.c_str());
			return false;
		}

		if (address.length() < 5) {
			fprintf(stdout, "Ignoring address \"%s\" (too short, length < 5)\n", address.c_str());
			return false;
		}

		if (address.length() >= 36) {
			fprintf(stdout, "Ignoring address \"%s\" (too long for a prefix, max 35 chars)\n", address.c_str());
			return false;
		}

		uint8_t data[64];
		memset(data, 0, sizeof(data));
		size_t data_length = 0;
		if (!bech32_decode_nocheck(data, &data_length, address.c_str() + 4)) {
			fprintf(stdout, "Ignoring address \"%s\" (Only \"023456789acdefghjklmnpqrstuvwxyz\" allowed)\n", address.c_str());
			return false;
		}

		// Each Bech32 char after "bc1q" fixes 5 MSB bits of hash160[0..19].
		// If fewer than 16 bits are fixed (< 4 chars), expand all possible values of the free bits.
		size_t bitsKnown = (address.length() - 4) * 5;
		if (bitsKnown >= 16) {
			buckets.push_back(readLE16(data));
		}
		else {
			uint32_t baseBE = ((uint32_t)data[0] << 8) | (uint32_t)data[1];
			uint32_t count = 1u << (16 - bitsKnown);
			buckets.reserve(count);
			for (uint32_t v = 0; v < count; v++) {
				uint32_t be = baseBE | v;
				address_t sAddr = (address_t)(((be >> 8) & 0xFFu) | ((be & 0xFFu) << 8));
				buckets.push_back(sAddr);
			}
		}

		target->isFull = false;
		target->address = address;
		return true;
	}
	else {

		// P2PKH
		std::vector<unsigned char> result;
		if (!DecodeBase58(address, result)) {
			fprintf(stdout, "Ignoring address \"%s\" (not base58 or too long: 0, I, O and l are not allowed)\n", address.c_str());
			return false;
		}

		// Prefix containing only '1' (must be checked before result.size() > 21)
		if (isSingularAddress(address)) {
			if (address.length() > 21) {
				fprintf(stdout, "Ignoring address \"%s\" (too many leading '1's, max 21)\n", address.c_str());
				return false;
			}
			target->isFull = false;
			target->address = address;
			if (address.length() == 2) {
				// "11...": version=0x00 and hash160[0]=0x00; hash160[1] can be any byte 0x00..0xFF
				buckets.reserve(256);
				for (uint32_t b1 = 0; b1 < 256; b1++)
					buckets.push_back((address_t)(b1 << 8));
			}
			else {
				// "111...": hash160[0] == 0x00 and hash160[1] == 0x00
				buckets.push_back(0);
			}
			return true;
		}

		// Full address?
		if (result.size() > 21) {
			if (result.size() != 25 || result[0] != 0) {
				fprintf(stdout, "Ignoring address \"%s\" (too long for a prefix, not a valid P2PKH address)\n", address.c_str());
				return false;
			}
			uint8_t chk[4];
			sha256_checksum(result.data(), 21, chk);
			if (memcmp(chk, result.data() + 21, 4) != 0) {
				fprintf(stdout, "Ignoring address \"%s\" (wrong checksum, mistyped address?)\n", address.c_str());
				return false;
			}

			target->isFull = true;
			memcpy(target->hash160, result.data() + 1, 20);
			target->address = address;
			buckets.push_back(readLE16(target->hash160));
			return true;
		}

		// General P2PKH prefix: enumerate every 16-bit (hash160[0], hash160[1]) bucket whose
		// 25-byte Base58Check encoding can begin with `address` (covering all valid address lengths).
		size_t z = 0;
		while (z < address.size() && address[z] == '1') z++;
		if (z > 21) {
			fprintf(stdout, "Ignoring address \"%s\" (too many leading '1's)\n", address.c_str());
			return false;
		}

		// Decode non-leading-'1' part of the prefix as a big integer V
		Int V;
		V.SetInt32(0);
		for (size_t k = 0; k < result.size(); k++) {
			V.ShiftL(8);
			V.Add((uint64_t)result[k]);
		}
		Int Vplus1(&V);
		Vplus1.AddOne();

		// Valid 25-byte payloads with exactly z leading zero bytes have integer value in
		// [2^(8*(24-z)), 2^(8*(25-z)) - 1]
		Int lowerBound, upperBound;
		lowerBound.SetInt32(1);
		lowerBound.ShiftL((uint32_t)(8 * (24 - z)));
		upperBound.SetInt32(1);
		upperBound.ShiftL((uint32_t)(8 * (25 - z)));
		upperBound.SubOne();

		Int pow58;
		pow58.SetInt32(1);
		vector<uint8_t> seen(65536, 0);

		for (int r = 0; r <= 34; r++) {
			Int minN, maxN;
			minN.Mult(&V, &pow58);
			if (minN.IsGreater(&upperBound))
				break;
			maxN.Mult(&Vplus1, &pow58);
			maxN.SubOne();

			if (!maxN.IsLower(&lowerBound)) {
				Int lo(minN.IsLower(&lowerBound) ? &lowerBound : &minN);
				Int hi(maxN.IsGreater(&upperBound) ? &upperBound : &maxN);

				if (z >= 3) {
					if (!seen[0]) {
						seen[0] = 1;
						buckets.push_back(0);
					}
				}
				else {
					// Bits 176..191 of the 24-byte integer are (hash160[0] << 8) | hash160[1],
					// which sit directly in bits 48..63 of bits64[2].
					uint32_t loBE = (uint32_t)(lo.bits64[2] >> 48);
					uint32_t hiBE = (uint32_t)(hi.bits64[2] >> 48);
					for (uint32_t be = loBE; be <= hiBE; be++) {
						address_t sAddr = (address_t)(((be >> 8) & 0xFFu) | ((be & 0xFFu) << 8));
						if (!seen[sAddr]) {
							seen[sAddr] = 1;
							buckets.push_back(sAddr);
						}
					}
				}
			}
			pow58.Mult(58);
		}

		if (buckets.empty()) {
			fprintf(stdout, "Ignoring address \"%s\" (invalid P2PKH prefix)\n", address.c_str());
			return false;
		}

		target->isFull = false;
		target->address = address;
		return true;
	}
}

// ----------------------------------------------------------------------------

void OrionSearch::output(const string& addr, const string& pAddr, const string& pAddrHex, const std::string& pubKey) {

	// "%064s" is undefined for strings (glibc pads with spaces), pad the key explicitly
	string paddedHex = pAddrHex;
	if (paddedHex.length() < 64)
		paddedHex.insert(0, 64 - paddedHex.length(), '0');

	FILE* f = stdout;
	bool needToClose = false;

	if (!outputFile.empty()) {
		f = fopen(outputFile.c_str(), "a");
		if (f == NULL) {
			fprintf(stderr, "Cannot open %s for writing\n", outputFile.c_str());
			f = stdout;
		}
		else {
			needToClose = true;
		}
	}

	if (f != stdout)
		fprintf(f, "\nPublic Addr: %s\n", addr.c_str());
	fprintf(stdout, "\nPublic Addr: %s\n", addr.c_str());

	switch (searchType) {
	case P2PKH:
		if (f != stdout)
			fprintf(f, "Priv (WIF): p2pkh:%s\n", pAddr.c_str());
		fprintf(stdout, "Priv (WIF): p2pkh:%s\n", pAddr.c_str());
		break;
	case BECH32:
		if (f != stdout)
			fprintf(f, "Priv (WIF): p2wpkh:%s\n", pAddr.c_str());
		fprintf(stdout, "Priv (WIF): p2wpkh:%s\n", pAddr.c_str());
		break;
	}

	if (f != stdout)
		fprintf(f, "Priv (HEX): 0x%s\n", paddedHex.c_str());
	fprintf(stdout, "Priv (HEX): 0x%s\n", paddedHex.c_str());

	if (f != stdout)
		fprintf(f, "Pub  Key  : %s\n", pubKey.c_str());
	fprintf(stdout, "Pub  Key  : %s\n", pubKey.c_str());

	if (f != stdout) {
		time_t now = time(NULL);
		fprintf(f, "Found at  : %s", ctime(&now));
	}
	fprintf(stdout, "\n");

	if (f != stdout)
		fflush(f);
	fflush(stdout);

	if (needToClose)
		fclose(f);
}

bool OrionSearch::checkPrivKey(const string& addr, const Int& key) {

	Int k((Int*)&key);
	Point p = secp->ComputePublicKey(&k);

	string chkAddr = secp->GetAddress(searchType, true, p);
	if (chkAddr != addr) {
		// Check the negated key as a fallback
		Int negK(&k);
		negK.Neg();
		negK.Add(&secp->order);
		Point negP = secp->ComputePublicKey(&negK);
		string negChkAddr = secp->GetAddress(searchType, true, negP);
		if (negChkAddr != addr) {
			fprintf(stdout, "\nWarning, wrong private key generated !\n");
			fprintf(stdout, "  Addr :%s\n", addr.c_str());
			fprintf(stdout, "  Check:%s\n", chkAddr.c_str());
			// Print the original (un-negated) candidate key so an external check can verify it
			fprintf(stdout, "  CandKey: %s\n", k.GetBase16().c_str());
			return false;
		}
		k.Set(&negK);
		p = negP;
	}

	output(addr, secp->GetPrivAddress(true, k), k.GetBase16(), secp->GetPublicKeyHex(true, p));
	return true;
}

void OrionSearch::checkAddr(int prefIdx, const uint8_t* hash160, const Int& key) {

	const vector<uint32_t>& idxs = buckets16[prefIdx];
	if (idxs.empty())
		return;

	if (onlyFull) {
		for (uint32_t idx : idxs) {
			ADDRESS_TARGET& t = targets[idx];
			if (stopWhenFound && t.found)
				continue;

			if (ripemd160_comp_hash(t.hash160, hash160)) {
				if (checkPrivKey(secp->GetAddress(searchType, true, (unsigned char*)hash160), key)) {
					if (!t.found) {
						t.found = true;
						nbUniqueFound++;
					}
					nbFoundKey++;
					if (stopWhenFound && nbUniqueFound >= targets.size()) {
						endOfSearch = true;
						return;
					}
				}
			}
		}
	}
	else {
		string addr = secp->GetAddress(searchType, true, (unsigned char*)hash160);

		for (uint32_t idx : idxs) {
			ADDRESS_TARGET& t = targets[idx];
			if (stopWhenFound && t.found)
				continue;

			if (addr.compare(0, t.address.size(), t.address) == 0) {
				if (checkPrivKey(addr, key)) {
					if (!t.found) {
						t.found = true;
						nbUniqueFound++;
					}
					nbFoundKey++;
					if (stopWhenFound && nbUniqueFound >= targets.size()) {
						endOfSearch = true;
						return;
					}
				}
			}
		}
	}
}

void OrionSearch::scanCPUWindow(const Int& firstKey, const Int& lastKey, const Int* skipKey) {

	Int k((Int*)&firstKey);
	Int endK((Int*)&lastKey);
	uint8_t h160[20];

	while (!endOfSearch && !k.IsGreater(&endK)) {
		if (skipKey == NULL || !k.IsEqual((Int*)skipKey)) {
			Point p = secp->ComputePublicKey(&k);
			if (!p.isZero()) {
				secp->GetHash160(searchType, true, p, h160);
				checkAddr(readLE16(h160), h160, k);
			}
		}
		k.AddOne();
	}
}

// Keys per GPU thread. Rounded up so that the last (taskSize mod nbThread) keys of the range
// are scanned too; clamped to minStep in sequential mode so thread slices never overlap.
void OrionSearch::getThreadStep(int nbThread, int minStep, Int* stepThread) {

	Int numthread;
	Int rem;

	stepThread->Set(&bc->ksFinish);
	stepThread->Sub(&bc->ksStart);
	stepThread->AddOne();
	numthread.SetInt32(nbThread);
	stepThread->Div(&numthread, &rem);
	if (!rem.IsZero())
		stepThread->AddOne();

	if (minStep > 0) {
		Int minS((uint64_t)minStep);
		if (stepThread->IsLower(&minS))
			stepThread->Set(&minS);
	}
}

void OrionSearch::getGPUStartingKeys(const Int& tRangeStart, const Int& stepThread, int groupSize, int nbThread, Point* p, const Int& progress) {

	Int baseOffset((Int*)&tRangeStart);
	baseOffset.Add((uint64_t)(groupSize / 2));
	baseOffset.Add((Int*)&progress);

	auto computeDirect = [&](size_t first, size_t count) {
		for (size_t idx = first; idx < first + count; idx++) {
			Int k((Int*)&stepThread);
			k.Mult((uint64_t)idx);
			k.Add(&baseOffset);
			k.Mod(&secp->order);
			p[idx] = secp->ComputePublicKey(&k);
		}
	};

	// Fast parallel batch-inversion path requires nbThread divisible by 512 (half >= 2).
	if (nbThread < 512 || (nbThread % 512) != 0) {
		computeDirect(0, (size_t)nbThread);
		return;
	}

	uint32_t grp_startkeys = (uint32_t)nbThread / 256u;
	uint32_t half = grp_startkeys / 2u;
	uint32_t nbBlocks = (uint32_t)nbThread / grp_startkeys;

	Point Pdouble;
	Int kDouble((Int*)&stepThread);
	kDouble.Mult((uint64_t)grp_startkeys);
	kDouble.Mod(&secp->order);
	Pdouble = secp->ComputePublicKey(&kDouble);

	Int kStart((Int*)&stepThread);
	kStart.Mult((uint64_t)half);
	kStart.Add(&baseOffset);
	kStart.Mod(&secp->order);

	// Precompute all block anchor points P[b*grp + half] so the 256 batch-inversion blocks run in parallel
	p[half] = secp->ComputePublicKey(&kStart);
	for (uint32_t b = 1; b < nbBlocks; b++) {
		if (p[(b - 1) * grp_startkeys + half].isZero() || Pdouble.isZero() ||
		    p[(b - 1) * grp_startkeys + half].x.IsEqual(&Pdouble.x)) {
			Int kb((Int*)&stepThread);
			kb.Mult((uint64_t)b * grp_startkeys + half);
			kb.Add(&baseOffset);
			kb.Mod(&secp->order);
			p[b * grp_startkeys + half] = secp->ComputePublicKey(&kb);
		}
		else {
			p[b * grp_startkeys + half] = secp->AddDirect(p[(b - 1) * grp_startkeys + half], Pdouble);
		}
	}

	Int key_delta((Int*)&stepThread);
	key_delta.Mod(&secp->order);
	Point* p_delta = new Point[half];
	p_delta[0] = secp->ComputePublicKey(&key_delta);
	Int key_delta2(&key_delta);
	key_delta2.Add(&key_delta);
	key_delta2.Mod(&secp->order);
	p_delta[1] = secp->ComputePublicKey(&key_delta2);
	for (size_t i = 2; i < half; i++) {
		if (p_delta[i - 1].isZero() || p_delta[0].isZero() || p_delta[i - 1].x.IsEqual(&p_delta[0].x)) {
			Int kd((Int*)&stepThread);
			kd.Mult((uint64_t)(i + 1));
			kd.Mod(&secp->order);
			p_delta[i] = secp->ComputePublicKey(&kd);
		}
		else {
			p_delta[i] = secp->AddDirect(p_delta[i - 1], p_delta[0]);
		}
	}

	unsigned int nWorkers = std::thread::hardware_concurrency();
	if (nWorkers == 0) nWorkers = 4;
	if (nWorkers > nbBlocks) nWorkers = nbBlocks;

	std::atomic<uint32_t> nextBlock(0);
	auto worker = [&]() {
		Int* subp = new Int[half];
		Int* dx = new Int[half];
		Int newValue, inverse, _s, _p, dy, syn;

		while (true) {
			uint32_t b = nextBlock.fetch_add(1);
			if (b >= nbBlocks) break;
			size_t i = (size_t)b * grp_startkeys + half;

			bool singular = p[i].isZero();
			if (!singular) {
				for (uint32_t j = 0; j < half; j++) {
					if (p_delta[j].isZero()) {
						singular = true;
						break;
					}
					dx[j].ModSub(&p_delta[j].x, &p[i].x);
					if (dx[j].IsZero()) {
						singular = true;
						break;
					}
				}
			}

			if (singular) {
				computeDirect((size_t)b * grp_startkeys, grp_startkeys);
				continue;
			}

			subp[0].Set(&dx[0]);
			for (uint32_t j = 1; j < half; j++) {
				subp[j].ModMulK1(&subp[j - 1], &dx[j]);
			}

			inverse.Set(&subp[half - 1]);
			inverse.ModInv();

			for (uint32_t j = half - 1; j > 0; j--) {
				newValue.ModMulK1(&subp[j - 1], &inverse);
				inverse.ModMulK1(&dx[j]);
				dx[j].Set(&newValue);
			}
			dx[0].Set(&inverse);

			syn.Set(&p[i].y);
			syn.ModNeg();

			uint32_t j;
			for (j = 0; j < half - 1; j++) {
				dy.ModSub(&p_delta[j].y, &p[i].y);
				_s.ModMulK1(&dy, &dx[j]);
				_p.ModSquareK1(&_s);

				p[i + j + 1].x.ModSub(&_p, &p[i].x);
				p[i + j + 1].x.ModSub(&p_delta[j].x);

				p[i + j + 1].y.ModSub(&p_delta[j].x, &p[i + j + 1].x);
				p[i + j + 1].y.ModMulK1(&_s);
				p[i + j + 1].y.ModSub(&p_delta[j].y);

				dy.ModSub(&syn, &p_delta[j].y);
				_s.ModMulK1(&dy, &dx[j]);
				_p.ModSquareK1(&_s);

				p[i - j - 1].x.ModSub(&_p, &p[i].x);
				p[i - j - 1].x.ModSub(&p_delta[j].x);

				p[i - j - 1].y.ModSub(&p[i - j - 1].x, &p_delta[j].x);
				p[i - j - 1].y.ModMulK1(&_s);
				p[i - j - 1].y.ModSub(&p_delta[j].y, &p[i - j - 1].y);
			}

			dy.ModSub(&syn, &p_delta[j].y);
			_s.ModMulK1(&dy, &dx[j]);
			_p.ModSquareK1(&_s);

			p[i - j - 1].x.ModSub(&_p, &p[i].x);
			p[i - j - 1].x.ModSub(&p_delta[j].x);

			p[i - j - 1].y.ModSub(&p[i - j - 1].x, &p_delta[j].x);
			p[i - j - 1].y.ModMulK1(&_s);
			p[i - j - 1].y.ModSub(&p_delta[j].y, &p[i - j - 1].y);
		}

		delete[] subp;
		delete[] dx;
	};

	std::vector<std::thread> pool;
	pool.reserve(nWorkers);
	for (unsigned int w = 0; w < nWorkers; w++)
		pool.emplace_back(worker);
	for (auto& t : pool)
		t.join();

	delete[] p_delta;
}

// Uniform random offset in [0, maxOffset] (inclusive)
static Int pickRandomOffset(std::mt19937_64& rng, const Int& maxOffset) {
	Int modVal((Int*)&maxOffset);
	modVal.AddOne();
	Int r;
	r.SetInt32(0);
	for (int i = 0; i < 4; i++)
		r.bits64[i] = rng();
	r.Mod(&modVal);
	return r;
}

// Pick nextOffset in [0, maxOffset] such that jumpDelta = (nextOffset - curOffset) - subAdvance
// satisfies |jumpDelta| > minDelta (guarantees jumpDelta != 0 and no collision with Gx[0..M-1]).
// Precondition: maxOffset > 2 * minDelta + subAdvance (or maxOffset > 2 * minDelta when subAdvance == 0).
static void pickNextRandomJump(std::mt19937_64& rng, const Int& curOffset, const Int& maxOffset,
                               uint64_t subAdvance, const Int& minDelta,
                               Int& nextOffset, Int& jumpDelta) {
	for (int tries = 0; tries < 64; tries++) {
		nextOffset = pickRandomOffset(rng, maxOffset);
		jumpDelta.Set(&nextOffset);
		jumpDelta.Sub((Int*)&curOffset);
		if (subAdvance > 0)
			jumpDelta.Sub(subAdvance);

		Int absDelta(&jumpDelta);
		if (absDelta.IsNegative()) absDelta.Neg();
		if (absDelta.IsGreater((Int*)&minDelta))
			return;
	}

	// Deterministic fallback: pick whichever endpoint (0 or maxOffset) gives a larger |jumpDelta|
	Int dLow;
	dLow.SetInt32(0);
	dLow.Sub((Int*)&curOffset);
	if (subAdvance > 0) dLow.Sub(subAdvance);
	Int absLow(&dLow);
	if (absLow.IsNegative()) absLow.Neg();

	Int dHigh((Int*)&maxOffset);
	dHigh.Sub((Int*)&curOffset);
	if (subAdvance > 0) dHigh.Sub(subAdvance);
	Int absHigh(&dHigh);
	if (absHigh.IsNegative()) absHigh.Neg();

	if (absHigh.IsGreater(&absLow)) {
		nextOffset.Set((Int*)&maxOffset);
		jumpDelta.Set(&dHigh);
	}
	else {
		nextOffset.SetInt32(0);
		jumpDelta.Set(&dLow);
	}
}

static Point jumpDeltaToPoint(Secp256K1* secp, const Int& jumpDelta) {
	Int d((Int*)&jumpDelta);
	if (d.IsNegative()) {
		d.Neg();
		Point P = secp->ComputePublicKey(&d);
		P.y.ModNeg();
		return P;
	}
	return secp->ComputePublicKey(&d);
}

void OrionSearch::FindKeyGPU(int gpuId) {

	bool ok = true;

	double t0;
	double ttot = t_Paused;

	// Global init
	GPUEngine g(gpuId, maxFound);
	if (!g.IsInitialised()) {
		fprintf(stderr, "[ERROR] GPU #%d initialisation failed\n", gpuId);
		searchStatus = SEARCH_ERROR;
		endOfSearch = true;
		return;
	}

	int numThreadsGPU = g.GetNbThread();
	int STEP_SIZE = g.GetStepSize();
	int subGrp = g.GetGroupSize();

	// A backup only maps to the same key layout: same range (checked when loaded), thread count and
	// keys per launch (ORION_GRP)
	if (!randomMode && backupStepSize != 0 && backupStepSize != (uint32_t)STEP_SIZE) {
		fprintf(stderr, "[ERROR] The backup was made with %u keys per thread and launch, this build uses %d (ORION_GRP).\n"
			"        Resume with a build of the same group size or start again without -backup.\n",
			backupStepSize, STEP_SIZE);
		searchStatus = SEARCH_ERROR;
		endOfSearch = true;
		return;
	}
	if (!randomMode && backupThreads != 0 && backupThreads != (uint32_t)numThreadsGPU) {
		fprintf(stderr, "[ERROR] The backup was made with %u GPU threads, this GPU runs %d. The progress cannot be\n"
			"        mapped to this GPU: resume on the original GPU model or start again without -backup.\n",
			backupThreads, numThreadsGPU);
		searchStatus = SEARCH_ERROR;
		endOfSearch = true;
		return;
	}

	fprintf(stdout, "GPU: %s\n", g.deviceName.c_str());
	fflush(stdout);

	Int taskSize;
	taskSize.Set(&bc->ksFinish);
	taskSize.Sub(&bc->ksStart);
	taskSize.AddOne();

	Int stepThread;
	getThreadStep(numThreadsGPU, randomMode ? 0 : STEP_SIZE, &stepThread);

	// -random needs enough room inside each thread's slice for valid non-zero jumps without
	// colliding with [-subGrp/2, +subGrp/2] or overflowing the slice. If the whole range already
	// fits in ~2 GPU launches, fall back cleanly to a sequential scan (without writing a backup).
	uint64_t subAdvance = (uint64_t)(STEP_SIZE - subGrp);
	Int minRandomStep((uint64_t)STEP_SIZE + (uint64_t)subGrp + subAdvance + 1ULL);
	bool effectiveRandom = randomMode;
	bool saveBackups = !randomMode;
	if (effectiveRandom && stepThread.IsLower(&minRandomStep)) {
		fprintf(stdout, "[WARNING] Range is too small for -random mode (fits in <= 2 GPU launches); scanning sequentially.\n");
		fflush(stdout);
		effectiveRandom = false;
		getThreadStep(numThreadsGPU, STEP_SIZE, &stepThread);
	}

	// For ranges smaller than one full-GPU launch, only launch the blocks needed to cover [ksStart, ksFinish]
	// so idle out-of-range threads cannot flood the hit buffer on short prefixes.
	if (!effectiveRandom) {
		Int fullLaunchKeys((uint64_t)STEP_SIZE);
		fullLaunchKeys.Mult((uint64_t)numThreadsGPU);
		if (taskSize.IsLower(&fullLaunchKeys)) {
			Int activeThreads(&taskSize);
			Int rem;
			Int stepInt((uint64_t)STEP_SIZE);
			activeThreads.Div(&stepInt, &rem);
			if (!rem.IsZero())
				activeThreads.AddOne();
			g.SetActiveThreads(activeThreads.bits64[0]);
		}
	}

	// Handle edge keys where GPU batch-inversion affine addition would hit dx == 0:
	//   1) ksStart == subGrp/2 (e.g. Puzzle #10: -start 200) -> thread 0 center P[0] = STEP_SIZE*G == finalJump
	//   2) ksFinish within STEP_SIZE of curve order -> wraps past order into small multiples of G
	Int cpuHandledFrom;
	bool hasTailCpuWindow = false;
	if (!effectiveRandom && idxcount == 0) {
		Int halfGrp((uint64_t)(subGrp / 2));
		if (bc->ksStart.IsEqual(&halfGrp)) {
			Int winEnd(&bc->ksStart);
			winEnd.Add((uint64_t)(STEP_SIZE - 1));
			if (winEnd.IsGreater(&bc->ksFinish))
				winEnd.Set(&bc->ksFinish);
			Int centerKey(&bc->ksStart);
			centerKey.Add(&halfGrp); // GPU checks center BEFORE batch inversion
			scanCPUWindow(bc->ksStart, winEnd, &centerKey);
		}

		Int tailThreshold(&secp->order);
		tailThreshold.Sub((uint64_t)(STEP_SIZE * 2));
		if (!endOfSearch && bc->ksFinish.IsGreaterOrEqual(&tailThreshold)) {
			cpuHandledFrom.Set(bc->ksStart.IsGreater(&tailThreshold) ? &bc->ksStart : &tailThreshold);
			hasTailCpuWindow = true;
			scanCPUWindow(cpuHandledFrom, bc->ksFinish, NULL);
		}

		if (endOfSearch) {
			searchStatus = SEARCH_ALL_FOUND;
			return;
		}
	}

	if (onlyFull && nbAddress == 1) {
		ok = g.SetSingleTarget(targets[0].hash160);
	}
	else if (onlyFull) {
		ok = g.SetAddress(usedAddressL, nbAddress);
	}
	else {
		ok = g.SetAddress(usedAddress);
	}

	// -random jumps: a 64-bit generator seeded with 256 bits, so that processes (one per GPU) and
	// restarts never share a sequence
	std::random_device rd;
	std::seed_seq seed{ rd(), rd(), rd(), rd(), rd(), rd(), rd(), (unsigned)time(NULL) };
	std::mt19937_64 rng(seed);

	Int maxOffset(&stepThread);
	maxOffset.Sub((uint64_t)STEP_SIZE);
	Int minDelta((uint64_t)(subGrp / 2));
	Int offsetInFlight;
	Int offsetNext;
	offsetInFlight.SetInt32(0);
	offsetNext.SetInt32(0);

	Point* publicKeys = new Point[numThreadsGPU];
	vector<ITEM> found;

	t0 = Timer::get_tick();

	Int startProgress;
	if (effectiveRandom) {
		offsetInFlight = pickRandomOffset(rng, maxOffset);
		Int firstJumpDelta;
		pickNextRandomJump(rng, offsetInFlight, maxOffset, subAdvance, minDelta, offsetNext, firstJumpDelta);
		ok = ok && g.SetRandomJump(jumpDeltaToPoint(secp, firstJumpDelta));
		startProgress.Set(&offsetInFlight);
	}
	else {
		startProgress.SetInt32(0);
		startProgress.Add(idxcount);
		startProgress.Mult((uint64_t)STEP_SIZE);
	}

	if (ok) {
		getGPUStartingKeys(bc->ksStart, stepThread, subGrp, numThreadsGPU, publicKeys, startProgress);
		ok = g.SetKeys(publicKeys);
	}
	delete[] publicKeys;

	printf("Starting keys set in %.2f seconds \n", Timer::get_tick() - t0);
	fflush(stdout);

	if (ok) {
		printf("GPU Started ! \r");
		fflush(stdout);
	}

	t0 = Timer::get_tick();

	double keysPerLaunch = (double)STEP_SIZE * (double)g.GetLaunchThreads();
	double keysDone = keysPerLaunch * (double)idxcount;
	double keysPrev = keysDone;
	double tprev = ttot;
	double speedAvg = 0.0;
	uint64_t launches = 0;

	Int part_key;
	Int privkey;
	Int keycount;
	Int localOffset;
	Int threadDone;

	while (ok && !endOfSearch) {

		if (stopRequested) {
			searchStatus = SEARCH_STOPPED;
			break;
		}

		if (Pause) {
			printf("\nPausing...\n");
			fflush(stdout);
			// The kernel in flight is not counted, it is scanned again on resume
			t_Paused = ttot;
			if (saveBackups)
				saveBackup(numThreadsGPU, STEP_SIZE, ttot, gpuId);
			g.FreeGPUEngine();
			printf("Paused (GPU released) - press p to resume, Ctrl+C to quit\n");
			fflush(stdout);
			searchStatus = SEARCH_PAUSED;
			break;
		}

		Int offsetAfterNext;
		if (effectiveRandom) {
			Int jumpDelta;
			pickNextRandomJump(rng, offsetNext, maxOffset, subAdvance, minDelta, offsetAfterNext, jumpDelta);
			ok = g.SetRandomJump(jumpDeltaToPoint(secp, jumpDelta));
			if (!ok) break;
		}

		ok = g.Launch(found);
		if (!ok) break;
		idxcount += 1;
		launches++;

		ttot = Timer::get_tick() - t0 + t_Paused;

		// Offset of the kernel that just completed, within each thread's slice
		if (effectiveRandom) {
			keycount.Set(&offsetInFlight);
		}
		else {
			keycount.SetInt32(0);
			keycount.Add((uint64_t)(idxcount - 1));
			keycount.Mult((uint64_t)STEP_SIZE);
		}

		for (size_t i = 0; i < found.size() && !endOfSearch; i++) {

			ITEM it = found[i];
			localOffset.Set(&keycount);
			if (it.incr >= 0)
				localOffset.Add((uint64_t)it.incr);
			else
				localOffset.Sub((uint64_t)(-(int64_t)it.incr));

			// Stay strictly inside this thread's slice and [ksStart, ksFinish]
			if (localOffset.IsNegative() || localOffset.IsGreaterOrEqual(&stepThread))
				continue;

			part_key.Set(&stepThread);
			part_key.Mult((uint64_t)it.thId);

			privkey.Set(&bc->ksStart);
			privkey.Add(&part_key);
			privkey.Add(&localOffset);

			if (privkey.IsLower(&bc->ksStart) || privkey.IsGreater(&bc->ksFinish))
				continue;
			if (hasTailCpuWindow && privkey.IsGreaterOrEqual(&cpuHandledFrom))
				continue;

			checkAddr(readLE16(it.hash), it.hash, privkey);
		}

		if (effectiveRandom) {
			offsetInFlight.Set(&offsetNext);
			offsetNext.Set(&offsetAfterNext);
		}

		if (saveBackups && idxcount % 60 == 0)
			saveBackup(numThreadsGPU, STEP_SIZE, ttot, gpuId);

		keysDone = keysPerLaunch * (double)idxcount;
		double speed = 0.0;
		if (ttot > tprev)
			speed = (keysDone - keysPrev) / (ttot - tprev) / 1000000.0; // speed in Mkey/s
		// The first launch also includes the start-up, leave it out of the average
		if (launches > 1)
			speedAvg = (speedAvg == 0.0) ? speed : 0.9 * speedAvg + 0.1 * speed;
		PrintStats(keysDone, (speedAvg > 0.0) ? speedAvg : speed, ttot, taskSize.ToDouble(), effectiveRandom);
		keysPrev = keysDone;
		tprev = ttot;

		// Each thread scans its own slice of stepThread keys
		threadDone.SetInt32(0);
		threadDone.Add((uint64_t)idxcount);
		threadDone.Mult((uint64_t)STEP_SIZE);
		if (!effectiveRandom && threadDone.IsGreaterOrEqual(&stepThread))
		{
			double avg_speed = (ttot > 0.0) ? keysDone / (ttot * 1000000.0) : 0.0; // Avg speed in MK/s
			printf("\n");
			printf("Range Finished! - Average Speed: %.1f [MK/s] - Found: %d   \n", avg_speed, nbFoundKey);
			fflush(stdout);

			time_t now = time(NULL);
			printf("Current task END time: %s", ctime(&now));

			if (saveBackups)
				saveBackup(numThreadsGPU, STEP_SIZE, ttot, gpuId);
			searchStatus = SEARCH_FINISHED;
			endOfSearch = true;
		}
	}

	if (!ok) {
		fprintf(stderr, "\n[ERROR] GPU #%d failed, search aborted\n", gpuId);
		searchStatus = SEARCH_ERROR;
	}
	else if (searchStatus == SEARCH_STOPPED) {
		printf("\nStopped by user.\n");
		if (saveBackups) {
			saveBackup(numThreadsGPU, STEP_SIZE, ttot, gpuId);
			printf("Progress saved to %s, resume with -backup\n", backupFileName(gpuId).c_str());
		}
	}
	else if (searchStatus == SEARCH_RUNNING && endOfSearch) {
		// -stop and every address has been found
		if (saveBackups)
			saveBackup(numThreadsGPU, STEP_SIZE, ttot, gpuId);
		searchStatus = SEARCH_ALL_FOUND;
	}
	t_Paused = ttot;
	fflush(stdout);

	endOfSearch = true;
}


static void formatDuration(char* buf, size_t len, double t) {

	if (!(t >= 0.0) || t > 3.15e13) {             // negative, NaN or beyond a million years
		snprintf(buf, len, "--:--:--");
		return;
	}
	double years = t / (365.25 * 86400.0);
	if (years >= 100.0) {
		snprintf(buf, len, "%.3g years", years);
		return;
	}
	uint64_t s = (uint64_t)t;
	uint64_t d = s / 86400;
	int h = (int)((s % 86400) / 3600);
	int m = (int)((s % 3600) / 60);
	int sec = (int)(s % 60);
	if (d > 0)
		snprintf(buf, len, "%llud %02d:%02d:%02d", (unsigned long long)d, h, m, sec);
	else
		snprintf(buf, len, "%02d:%02d:%02d", h, m, sec);
}

void OrionSearch::PrintStats(double keysDone, double speed, double ttot, double taskSize, bool isRandom) {

	double perc = (taskSize > 0.0) ? 100.0 * keysDone / taskSize : 0.0;
	// The last launch can run past the end of a small range
	if (!isRandom && perc > 100.0)
		perc = 100.0;
	double log_keys = (keysDone >= 1.0) ? log2(keysDone) : 0.0;
	double bkeys = keysDone / 1000000000.0;

	char run[64];
	formatDuration(run, sizeof(run), ttot);

	if (isRandom) {
		printf("%.1f MK/s - %.0f BKeys - 2^%.2f [%.2f%%] - RUN: %s - Found: %d     ",
			speed, bkeys, log_keys, perc, run, nbFoundKey);
	}
	else {
		char end[64];
		double left = taskSize - keysDone;
		if (left < 0.0) left = 0.0;
		formatDuration(end, sizeof(end), (speed > 0.0) ? left / (speed * 1000000.0) : -1.0);
		printf("%.1f MK/s - %.0f BKeys - 2^%.2f [%.2f%%] - RUN: %s|END: %s - Found: %d     ",
			speed, bkeys, log_keys, perc, run, end, nbFoundKey);
	}

	printf("\r");
	fflush(stdout);
}


// ----------------------------------------------------------------------------
// Backup file

static const char BACKUP_MAGIC[8] = { 'O','R','I','O','N','B','2','\n' };

std::string backupFileName(int gpuId) {
	return "orion_backup_gpu" + std::to_string(gpuId) + ".dat";
}

static void writeKey(std::ofstream& f, const Int& k) {
	for (int i = 0; i < 4; i++)
		f.write(reinterpret_cast<const char*>(&k.bits64[i]), sizeof(uint64_t));
}

static void readKey(std::ifstream& f, Int& k) {
	k.SetInt32(0);
	for (int i = 0; i < 4; i++)
		f.read(reinterpret_cast<char*>(&k.bits64[i]), sizeof(uint64_t));
}

bool readBackup(const std::string& fileName, BACKUP_STATE* st) {

	std::ifstream f(fileName, std::ios::binary);
	if (!f)
		return false;

	f.seekg(0, std::ios::end);
	std::streamoff size = f.tellg();
	f.seekg(0, std::ios::beg);

	st->legacy = false;
	st->nbThread = 0;
	st->stepSize = 0;
	st->start.SetInt32(0);
	st->finish.SetInt32(0);

	if (size == (std::streamoff)(sizeof(int32_t) + sizeof(double))) {
		// Legacy 12-byte backup format: int idxcount, double time
		int32_t idx = 0;
		f.read(reinterpret_cast<char*>(&idx), sizeof(idx));
		f.read(reinterpret_cast<char*>(&st->elapsed), sizeof(st->elapsed));
		if (!f || idx < 0)
			return false;
		st->idxcount = (uint64_t)idx;
		st->legacy = true;
		return true;
	}

	char magic[8];
	f.read(magic, sizeof(magic));
	if (!f || memcmp(magic, BACKUP_MAGIC, sizeof(magic)) != 0)
		return false;
	f.read(reinterpret_cast<char*>(&st->idxcount), sizeof(st->idxcount));
	f.read(reinterpret_cast<char*>(&st->elapsed), sizeof(st->elapsed));
	readKey(f, st->start);
	readKey(f, st->finish);
	f.read(reinterpret_cast<char*>(&st->nbThread), sizeof(st->nbThread));
	f.read(reinterpret_cast<char*>(&st->stepSize), sizeof(st->stepSize));
	return (bool)f;
}

bool writeBackup(const std::string& fileName, const BACKUP_STATE* st) {

	// Written next to the target and renamed over it, so an interrupted write never leaves a
	// truncated backup behind
	std::string tmp = fileName + ".tmp";
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f)
			return false;
		f.write(BACKUP_MAGIC, sizeof(BACKUP_MAGIC));
		f.write(reinterpret_cast<const char*>(&st->idxcount), sizeof(st->idxcount));
		f.write(reinterpret_cast<const char*>(&st->elapsed), sizeof(st->elapsed));
		writeKey(f, st->start);
		writeKey(f, st->finish);
		f.write(reinterpret_cast<const char*>(&st->nbThread), sizeof(st->nbThread));
		f.write(reinterpret_cast<const char*>(&st->stepSize), sizeof(st->stepSize));
		f.flush();
		if (!f)
			return false;
	}

#ifdef WIN64
	return MoveFileExA(tmp.c_str(), fileName.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(tmp.c_str(), fileName.c_str()) == 0;
#endif
}

void OrionSearch::saveBackup(int nbThread, int stepSize, double elapsed, int gpuid) {

	BACKUP_STATE st;
	st.idxcount = idxcount;
	st.elapsed = elapsed;
	st.start.Set(&bc->ksStart);
	st.finish.Set(&bc->ksFinish);
	st.nbThread = (uint32_t)nbThread;
	st.stepSize = (uint32_t)stepSize;
	st.legacy = false;

	std::string filename = backupFileName(gpuid);
	if (!writeBackup(filename, &st))
		std::cerr << "\nError writing backup file: " << filename << "\n";
}

int OrionSearch::Search(int gpuId) {

	endOfSearch = false;
	searchStatus = SEARCH_RUNNING;

	// One GPU per process: run directly on this thread without spawning a dummy 1-element thread pool
	FindKeyGPU(gpuId);

	return searchStatus;
}
