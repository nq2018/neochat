#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include "Block.h"
#include "Config.h"
#include "Network.h"
#include "Utils.h"
#include "sqlite3.h"

using namespace std;

class Blockchain
{
public:
	static Blockchain* GetInstance();
	bool AddBlock(Block block);
	bool BlockExists(string hash);
	u64 GetHeight();
	bool GetBlock(u64 index, Block &block);
	bool GetBlock(string hash, Block &block);
	bool GetBlockData(u64 index, string &data);
	bool GetBlockHash(u64 index, string &hash);

private:
	static Blockchain* instance;
	static mutex inst;
	sqlite3 *db;
	string current_block_hash;
	u64 current_height;
	map<string, string> block_parents;

	Blockchain();
	bool CheckBlock(string hash);
	bool LoadBlock(string hash, Block &block, u64 &height);
	bool SaveBlock(Block block);

};