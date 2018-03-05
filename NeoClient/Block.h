#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "Crypto.h"

using namespace std;

class Block
{
public:
	static const u64 header_size = 72;
	static const u64 max_size = 512;
	static const u64 difficulty = 12;

	Block();
	Block(string prev_hash, string data, string nonce = string(32, 0));
	Block(string bin_data);
	Block& operator=(const Block &other);
	string ToBinaryData();
	string GetNonce();
	string GetHash();
	string GetPrevHash();
	string GetData();
	bool Check();
	void Mine();

private:
	string m_nonce;
	string m_prev_hash;
	string m_data;

};