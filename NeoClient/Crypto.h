#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <ctime>

using namespace std;

typedef unsigned __int64 u64;

class Crypto
{
public:
	static string Hash(string data);
	static string Rand(u64 length);

private:
	static time_t prev_rand;

	static const u64 hash_block_size = 32;
	static string sbox(string data);
	static string pbox(string data);
	static string xor_buffers(string s1, string s2);
	static string laimassey(string data, string key);
	static string feistel(string data, string key);
	static string encrypt_block(string data, string key);
	static string compress(string data, string key);

};