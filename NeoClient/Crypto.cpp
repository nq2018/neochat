#include "Crypto.h"

time_t Crypto::prev_rand = 0;

string Crypto::sbox(string data)
{
	static const BYTE s[256] = { 185, 226, 243, 232, 46, 37, 100, 127, 108, 119, 38, 61, 251, 224, 177, 170, 186, 176, 225, 250, 60, 39, 118, 109, 126, 101, 52, 111, 233, 121, 163, 164, 215, 204, 157, 6, 64, 91, 138, 145, 2, 25, 72, 83, 149, 142, 223, 196, 197, 222, 143, 148, 82, 73, 24, 3, 16, 11, 90, 65, 135, 156, 205, 242, 159, 132, 213, 206, 8, 19, 66, 89, 74, 87, 0, 27, 221, 198, 151, 140, 141, 150, 199, 220, 26, 1, 80, 75,88, 67, 18, 9, 207, 212, 133, 158, 241, 59, 187, 160, 102, 125, 44, 55, 36, 63,110, 183, 179, 168, 249, 162, 227, 248, 169, 178, 116, 95, 62, 53, 54, 45, 124,47, 161, 152, 235, 190, 115, 105, 56, 35, 165, 254, 175, 180, 167, 188, 237, 246, 48, 43, 122, 97, 96, 123, 42, 49, 247, 236, 189, 166, 181, 174, 255, 228, 34,57, 104, 103, 28, 7, 86, 77, 139, 144, 193, 218, 201, 210, 131, 184, 94, 69, 20, 15, 14, 21, 68, 29, 153, 128, 211, 234, 219, 192, 17, 10, 76, 85, 134, 171, 84, 79, 30, 5, 195, 216, 136, 146, 129, 154, 203, 208, 22, 13, 92, 71, 70, 93, 12,23, 209, 202, 155, 130, 147, 137, 217, 194, 4, 31, 78, 200, 58, 33, 112, 107, 173, 182, 230, 252, 239, 50, 229, 172, 120, 99, 114, 41, 40, 51, 98, 113, 191, 214, 245, 244, 253, 231, 117, 32, 106, 81, 240, 238 };
	for (u64 i = 0; i < hash_block_size / 2; i++)
		data[i] = s[(BYTE)data[i]];
	return data;
}

string Crypto::pbox(string data)
{
	string tmp(hash_block_size / 2, 0);
	for (u64 i = 0; i < hash_block_size / 2; i++)
		for (u64 j = 0; j < 8; j++)
		{
			u64 shift = (35 * (i * 8 + j) + 57) % (8 * (hash_block_size / 2));
			tmp[i] |= (data[shift / 8] & (0x80 >> (shift % 8))) ? (0x80 >> j) : (0);
		}
	return tmp;
}

string Crypto::xor_buffers(string s1, string s2)
{
	if (s1.length() >= s2.length())
	{
		for (u64 i = 0; i < s2.length(); i++)
			s1[i] = s1[i] ^ s2[i];
		return s1;
	}
	else
	{
		for (u64 i = 0; i < s1.length(); i++)
			s2[i] = s1[i] ^ s2[i];
		return s2;
	}
}

string Crypto::laimassey(string data, string key)
{
	string tmp = xor_buffers(data.substr(0, hash_block_size / 2), data.substr(hash_block_size / 2, hash_block_size / 2));
	tmp = xor_buffers(tmp, key);
	tmp = sbox(tmp);
	tmp = pbox(tmp);
	return xor_buffers(tmp, data.substr(0, hash_block_size / 2)) + xor_buffers(tmp, data.substr(hash_block_size / 2, hash_block_size / 2));
}

string Crypto::feistel(string data, string key)
{
	string tmp = data.substr(hash_block_size / 2, hash_block_size / 2);
	tmp = xor_buffers(tmp, key);
	tmp = sbox(tmp);
	tmp = pbox(tmp);
	return data.substr(hash_block_size / 2, hash_block_size / 2) + xor_buffers(tmp, data.substr(0, hash_block_size / 2));
}

string Crypto::encrypt_block(string data, string key)
{
	data = feistel(data, key.substr(0, hash_block_size / 2));
	data = feistel(data, key.substr(hash_block_size / 2, hash_block_size / 2));
	data = laimassey(data, key.substr(0, hash_block_size / 2));
	data = laimassey(data, key.substr(hash_block_size / 2, hash_block_size / 2));
	return data;
}

string Crypto::compress(string data, string key)
{
	string enc = encrypt_block(data, key);
	data = xor_buffers(data, enc);
	return data;
}

string Crypto::Hash(string data)
{
	// Padding
	u64 size = data.length();
	if (hash_block_size - size % hash_block_size >= sizeof(size))
		data += string(hash_block_size - size % hash_block_size - sizeof(size), 0) + string((char*)&size, sizeof(size));
	else
		data += string(2 * hash_block_size - size % hash_block_size - sizeof(size), 0) + string((char*)&size, sizeof(size));

	// Break into blocks
	vector<string> blocks;
	for (u64 i = 0; i < data.length(); i += hash_block_size)
		blocks.push_back(data.substr(i, hash_block_size));

	// Initial hash value
	blocks.push_back("\xdd\x67\x64\x59\x4e\x0a\xa5\x5a\x86\x20\xc1\x89\xc4\x30\x64\xbc\x69\xa2\xc5\x8b\x83\x20\xe6\x52\x52\xf6\xed\xd2\xaf\xee\x54\x0d");

	// Main loop
	for (u64 i = blocks.size() - 1; i > 0; i--)
		blocks[i - 1] = compress(blocks[i], blocks[i - 1]);

	return blocks[0];
}

string Crypto::Rand(u64 length)
{
	srand(time(0) + prev_rand);
	string res(length, 0);
	for (u64 i = 0; i < length; i++)
		res[i] = rand();
	prev_rand = rand();
	return res;
}

