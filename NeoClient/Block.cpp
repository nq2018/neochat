#include "Block.h"

Block::Block()
	: m_nonce(string(32, 0)), m_prev_hash(string(32, 0)), m_data(string())
{

}

Block::Block(string prev_hash, string data, string nonce)
	: m_nonce(nonce), m_prev_hash(prev_hash), m_data(data)
{
	if (m_prev_hash.length() != 32)
		throw "Bad prev_hash value";
	if (m_data.length() > max_size - header_size)
		throw "Data too long";
}

Block::Block(string bin_data)
{
	if ((bin_data.length() < header_size) || (bin_data.length() > max_size))
		throw "Bad block size";
	m_nonce = bin_data.substr(0, 32);
	m_prev_hash = bin_data.substr(32, 32);
	u64 data_size = *(u64*)&bin_data[64];
	if (data_size + header_size != bin_data.length())
		throw "Bad data size";
	m_data = bin_data.substr(72);
}

Block & Block::operator=(const Block & other)
{
	m_nonce = other.m_nonce;
	m_prev_hash = other.m_prev_hash;
	m_data = other.m_data;
	return *this;
}

string Block::ToBinaryData()
{
	u64 data_size = m_data.length();
	return m_nonce + m_prev_hash + string((char*)&data_size, 8) + m_data;
}

string Block::GetNonce()
{
	return m_nonce;
}

string Block::GetHash()
{
	return Crypto::Hash(ToBinaryData());
}

string Block::GetPrevHash()
{
	return m_prev_hash;
}

string Block::GetData()
{
	return m_data;
}

bool Block::Check()
{
	string sHash = GetHash();
	for (u64 i = 0; i < difficulty; i++)
		if (sHash[i] != 0)
			return false;
	return true;
}

void Block::Mine()
{
	do
	{
		m_nonce = Crypto::Rand(32);
	} while (!Check());
}