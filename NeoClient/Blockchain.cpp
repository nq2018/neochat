#include "Blockchain.h"

Blockchain* Blockchain::instance = nullptr;
mutex Blockchain::inst;

Blockchain* Blockchain::GetInstance()
{
	lock_guard<mutex> get_instance(inst);
	if (instance == nullptr)
		instance = new Blockchain();
	return instance;
}

bool Blockchain::AddBlock(Block block)
{
	if (!block.Check())
		return false;
	if (BlockExists(block.GetHash()))
		return false;
	return SaveBlock(block);
}

bool Blockchain::BlockExists(string hash)
{
	if (hash.length() != 32)
		return false;
	if (hash == string(32, 0))
		return true;
	return CheckBlock(hash);
}

u64 Blockchain::GetHeight()
{
	return current_height;
}

bool Blockchain::GetBlock(u64 index, Block &block)
{
	string target_block_hash;
	if (!GetBlockHash(index, target_block_hash))
		return false;

	u64 height;
	if (!LoadBlock(target_block_hash, block, height))
		return false;
	return true;
}

bool Blockchain::GetBlock(string hash, Block & block)
{
	if (hash.length() != 32)
		return false;
	u64 height;
	return LoadBlock(hash, block, height);
}

bool Blockchain::GetBlockData(u64 index, string &data)
{
	string target_block_hash;
	if (!GetBlockHash(index, target_block_hash))
		return false;

	Block block;
	u64 height;
	if (!LoadBlock(target_block_hash, block, height))
		return false;
	data = block.GetData();
	return true;
}

bool Blockchain::GetBlockHash(u64 index, string &hash)
{
	if (index == 0)
	{
		hash = string(32, 0);
		return true;
	}
	if (index > current_height)
		return false;
	u64 steps = current_height - index;
	string target_block_hash = current_block_hash;
	for (u64 i = 0; i < steps; i++)
	{
		if (block_parents.count(target_block_hash) != 0)
			target_block_hash = block_parents[target_block_hash];
		else
		{
			Block block;
			u64 height;
			if (!LoadBlock(target_block_hash, block, height))
				return false;
			target_block_hash = block.GetPrevHash();
		}
	}
	hash = target_block_hash;
	return true;
}

Blockchain::Blockchain()
	: current_block_hash(string(32, 0)), current_height(0)
{
	SetCurrentDirectory(NodeConfig::current_dir.c_str());
	if (sqlite3_open("blockchain.db", &db) != SQLITE_OK)
		FatalError(L"Error opening blockchain database");

	if (sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS Blockchain(hash BLOB PRIMARY KEY NOT NULL, prev_hash BLOB NOT NULL, nonce BLOB NOT NULL, data BLOB NOT NULL, height INT NOT NULL); CREATE INDEX IF NOT EXISTS prev_hash_index ON Blockchain(prev_hash);", NULL, NULL, NULL) != SQLITE_OK)
		FatalError(L"Error creating table Blockchain");

	sqlite3_stmt *stmt;
	if (sqlite3_prepare(db, "SELECT hash, height FROM Blockchain ORDER BY height DESC LIMIT 1;", -1, &stmt, 0) != SQLITE_OK)
		FatalError(L"Error preparing SELECT statement");

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char *buf = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0));
		copy(buf, buf + current_block_hash.length(), &current_block_hash[0]);
		current_height = sqlite3_column_int(stmt, 1);
	}

	sqlite3_finalize(stmt);
}

bool Blockchain::CheckBlock(string hash)
{
	sqlite3_stmt *stmt;
	if (sqlite3_prepare(db, "SELECT height FROM Blockchain WHERE hash = ?;", -1, &stmt, 0) != SQLITE_OK)
		FatalError(L"Error preparing SELECT statement");

	if (sqlite3_bind_blob(stmt, 1, &hash[0], hash.length(), SQLITE_STATIC) != SQLITE_OK)
		FatalError(L"Error binding BLOB value");

	bool success = (sqlite3_step(stmt) == SQLITE_ROW);

	sqlite3_finalize(stmt);

	return success;
}

bool Blockchain::LoadBlock(string hash, Block &block, u64 &height)
{
	sqlite3_stmt *stmt;
	if (sqlite3_prepare(db, "SELECT prev_hash, nonce, data, height FROM Blockchain WHERE hash = ?;", -1, &stmt, 0) != SQLITE_OK)
		FatalError(L"Error preparing SELECT statement");

	if (sqlite3_bind_blob(stmt, 1, &hash[0], hash.length(), SQLITE_STATIC) != SQLITE_OK)
		FatalError(L"Error binding BLOB value");

	string prev_hash, nonce, data;
	bool success = false;
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char *buf = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0));
		prev_hash.resize(sqlite3_column_bytes(stmt, 0));
		copy(buf, buf + prev_hash.length(), &prev_hash[0]);
		buf = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1));
		nonce.resize(sqlite3_column_bytes(stmt, 1));
		copy(buf, buf + nonce.length(), &nonce[0]);
		buf = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 2));
		data.resize(sqlite3_column_bytes(stmt, 2));
		copy(buf, buf + data.length(), &data[0]);
		height = sqlite3_column_int(stmt, 3);
		success = ((prev_hash.length() == 32) && (nonce.length() == 32));
	}

	sqlite3_finalize(stmt);

	if (success)
		block = Block(prev_hash, data, nonce);
	return success;
}

bool Blockchain::SaveBlock(Block block)
{
	Block b_tmp;
	u64 height;
	u64 initial_height = current_height;

	if (block.GetPrevHash() == string(32, 0))
		height = 1;
	else
	{
		if (LoadBlock(block.GetPrevHash(), b_tmp, height))
		{
			if (height != 0)
				height++;
		}
		else
		{
			Net::GetInstance()->DownloadBlock(block.GetPrevHash());
			height = 0;
		}
	}

	sqlite3_stmt *stmt;
	if (sqlite3_prepare(db, "INSERT INTO Blockchain(hash, prev_hash, nonce, data, height) VALUES (?, ?, ?, ?, ?);", -1, &stmt, 0) != SQLITE_OK)
		FatalError(L"Error preparing SELECT statement");

	string hash = block.GetHash();
	if (sqlite3_bind_blob(stmt, 1, &hash[0], hash.length(), SQLITE_STATIC) != SQLITE_OK)
		FatalError(L"Error binding BLOB value");

	string prev_hash = block.GetPrevHash();
	if (sqlite3_bind_blob(stmt, 2, &prev_hash[0], prev_hash.length(), SQLITE_STATIC) != SQLITE_OK)
		FatalError(L"Error binding BLOB value");

	string nonce = block.GetNonce();
	if (sqlite3_bind_blob(stmt, 3, &nonce[0], nonce.length(), SQLITE_STATIC) != SQLITE_OK)
		FatalError(L"Error binding BLOB value");

	string data = block.GetData();
	if (sqlite3_bind_blob(stmt, 4, &data[0], data.length(), SQLITE_STATIC) != SQLITE_OK)
		FatalError(L"Error binding BLOB value");

	if (sqlite3_bind_int(stmt, 5, height) != SQLITE_OK)
		FatalError(L"Error binding INT value");

	if (sqlite3_step(stmt) != SQLITE_DONE)
	{
		sqlite3_finalize(stmt);
		return false;
	}

	sqlite3_finalize(stmt);

	if (height > current_height)
	{
		current_height = height;
		current_block_hash = block.GetHash();
	}

	if (height != 0)
	{
		vector<string> blocks_to_update = { block.GetHash() };
		vector<u64> heights = { height };
		while (blocks_to_update.size() != 0)
		{
			sqlite3_stmt *stmt;
			if (sqlite3_prepare(db, "SELECT hash FROM Blockchain WHERE prev_hash = ?;", -1, &stmt, 0) != SQLITE_OK)
				FatalError(L"Error preparing SELECT statement");

			if (sqlite3_bind_blob(stmt, 1, &blocks_to_update[0][0], blocks_to_update[0].length(), SQLITE_STATIC) != SQLITE_OK)
				FatalError(L"Error binding BLOB value");

			bool found = false;
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				const char *buf = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0));
				string updated_hash(sqlite3_column_bytes(stmt, 0), 0);
				copy(buf, buf + updated_hash.length(), &updated_hash[0]);

				blocks_to_update.push_back(updated_hash);
				heights.push_back(heights[0] + 1);

				if (heights[0] + 1 > current_height)
				{
					current_height = heights[0] + 1;
					current_block_hash = updated_hash;
				}

				found = true;
			}
			sqlite3_finalize(stmt);

			if (found)
			{
				if (sqlite3_prepare(db, "UPDATE Blockchain SET height = ? WHERE prev_hash = ?;", -1, &stmt, 0) != SQLITE_OK)
					FatalError(L"Error preparing SELECT statement");

				if (sqlite3_bind_int(stmt, 1, heights[0] + 1) != SQLITE_OK)
					FatalError(L"Error binding INT value");

				if (sqlite3_bind_blob(stmt, 2, &blocks_to_update[0][0], blocks_to_update[0].length(), SQLITE_STATIC) != SQLITE_OK)
					FatalError(L"Error binding BLOB value");

				sqlite3_step(stmt);

				sqlite3_finalize(stmt);
			}

			blocks_to_update.erase(blocks_to_update.begin());
			height = max(height, heights[0]);
			heights.erase(heights.begin());
		}
	}

	return true;
}
