#pragma once
#include <Windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <ratio>
#include <mutex>
#include <map>
#include "Blockchain.h"
#include "Config.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace chrono;

class Net
{
public:
	static Net* GetInstance();
	void DownloadBlock(string hash);
	void PublishNewBlock(Block block);
	string DumpPeers();

private:
	static const int upd_len = 576;
	static Net* instance;
	WSADATA wsa;
	SOCKET s;
	string bootstrap_addr;
	bool is_bootstrap;
	map<string, high_resolution_clock::time_point> peers;
	map<string, int> hole_punching_peers;
	vector<string> requested_blocks;
	mutex peers_mtx, hole_punching_peers_mtx, requested_blocks_mtx, new_block_mtx;
	static mutex inst;
	vector<thread> threads;

	enum msg_type
	{
		I_AM_ALIVE_MSG = 'a',
		REQUEST_PEERS_MSG = 'r',
		CONNECT_MSG = 'c',
		QUERY_LAST_BLOCK_MSG = 'q',
		BLOCK_HASH_MSG = 'h',
		GET_BLOCK_MSG = 'g',
		BLOCK_MSG = 'b'
	};

	Net(string bootstrap_host, string bootstrap_port, string my_port, bool bootstrap_flag);
	string GetAddr(sockaddr_in sock);
	bool ParseMsg(msg_type &type, vector<string> &data, string msg);
	void ProcessMessage(string msg, string addr);
	void Listen();
	bool BuildMsg(msg_type type, vector<string> data, string &msg);
	void SendUdpMessage(string addr, string msg);
	void ContactPeers();
	void PunchHoles();
	void DownloadBlocks();
};