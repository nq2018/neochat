#include "Network.h"

Net* Net::instance = nullptr;
mutex Net::inst;

Net* Net::GetInstance()
{
	lock_guard<mutex> get_instance(inst);
	if (instance == nullptr)
	{
		size_t pos = NodeConfig::bootstrap_node.find(':');
		if (pos == string::npos)
			FatalError(L"Bad bootstrap host address");
		string bootstrap_host = NodeConfig::bootstrap_node.substr(0, pos);
		string bootstrap_port = NodeConfig::bootstrap_node.substr(pos + 1);
		string my_port = to_string(NodeConfig::node_port);
		instance = new Net(bootstrap_host, bootstrap_port, my_port, false);
	}
	return instance;
}

Net::Net(string bootstrap_host, string bootstrap_port, string my_port, bool bootstrap_flag)
	: is_bootstrap(bootstrap_flag)
{
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		FatalError(L"Winsock initialisation failed");

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET)
		FatalError(L"Error creating socket");

	sockaddr_in bootstrap;
	bootstrap.sin_family = AF_INET;
	bootstrap.sin_addr.s_addr = inet_addr(bootstrap_host.c_str());
	bootstrap.sin_port = stoi(bootstrap_port);
	bootstrap_addr = GetAddr(bootstrap);

	sockaddr_in me;
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = stoi(my_port);
	if (::bind(s, (sockaddr*)&me, sizeof(me)) == SOCKET_ERROR)
		FatalError(L"Port binding failed. Please check if the specified port is not used by another program (or change the port in config file).");

	threads.push_back(thread(&Net::Listen, this));
	if (!is_bootstrap)
	{
		threads.push_back(thread(&Net::ContactPeers, this));
		threads.push_back(thread(&Net::PunchHoles, this));
		threads.push_back(thread(&Net::DownloadBlocks, this));
	}
}

void Net::DownloadBlock(string hash)
{
	lock_guard<mutex> lock3(requested_blocks_mtx);
	requested_blocks.push_back(hash);
}

void Net::PublishNewBlock(Block block)
{
	string block_data = block.ToBinaryData();
	string msg;
	if (!BuildMsg(BLOCK_MSG, { block_data }, msg))
		return;
	{
		lock_guard<mutex> lock(peers_mtx);
		for (auto p = peers.begin(); p != peers.end(); p++)
			SendUdpMessage(p->first, msg);
	}
}

string Net::DumpPeers()
{
	stringstream result;
	{
		lock_guard<mutex> lock(peers_mtx);
		result << "Active peers: " << peers.size() << '\n';
		for (auto p = peers.begin(); p != peers.end(); p++)
		{
			string addr = p->first;
			double last_con = duration<float>(high_resolution_clock::now() - p->second).count();
			result << "    " << (int)(BYTE)addr[0] << "." << (int)(BYTE)addr[1] << "." << (int)(BYTE)addr[2] << "." << (int)(BYTE)addr[3] << ":" << *(USHORT*)&addr[4] << " (" << last_con << " sec)\n";
		}
	}
	{
		lock_guard<mutex> lock2(hole_punching_peers_mtx);
		result << "Potential peers: " << hole_punching_peers.size() << '\n';
		for (auto p = hole_punching_peers.begin(); p != hole_punching_peers.end(); p++)
		{
			string addr = p->first;
			result << "    " << (int)(BYTE)addr[0] << "." << (int)(BYTE)addr[1] << "." << (int)(BYTE)addr[2] << "." << (int)(BYTE)addr[3] << ":" << *(USHORT*)&addr[4] << '\n';
		}
	}
	return result.str();
}

string Net::GetAddr(sockaddr_in sock)
{
	return string((char*)&sock.sin_addr.s_addr, 4) + string((char*)&sock.sin_port, 2);
}

bool Net::ParseMsg(msg_type &type, vector<string> &data, string msg)
{
	if (msg.length() == 0)
		return false;

	switch (msg[0])
	{

	case 'a':
		if (msg.length() != 1)
			return false;
		type = I_AM_ALIVE_MSG;
		data.clear();
		return true;

	case 'r':
		if (msg.length() > 1 + 12 * 6)
			return false;
		type = REQUEST_PEERS_MSG;
		data.clear();
		for (int i = 1; i < msg.length(); i += 6)
			data.push_back(msg.substr(i, 6));
		return true;

	case 'c':
		if (msg.length() != 7)
			return false;
		type = CONNECT_MSG;
		data = { msg.substr(1, 6) };
		return true;

	case 'q':
		if (msg.length() != 1)
			return false;
		type = QUERY_LAST_BLOCK_MSG;
		data.clear();
		return true;

	case 'h':
		if (msg.length() != 33)
			return false;
		type = BLOCK_HASH_MSG;
		data = { msg.substr(1, 32) };
		return true;

	case 'g':
		if (msg.length() != 33)
			return false;
		type = GET_BLOCK_MSG;
		data = { msg.substr(1, 32) };
		return true;

	case 'b':
		if (msg.length() > 513)
			return false;
		type = BLOCK_MSG;
		data = { msg.substr(1) };
		return true;

	default:
		return false;
	}
}

void Net::ProcessMessage(string msg, string addr)
{
	msg_type type;
	vector<string> data;
	if (ParseMsg(type, data, msg))
	{
		switch (type)
		{

		case I_AM_ALIVE_MSG:
			if (addr != bootstrap_addr)
			{
				lock_guard<mutex> lock(peers_mtx);
				lock_guard<mutex> lock2(hole_punching_peers_mtx);
				if (peers.count(addr) == 0)
				{
					string msg;
					if (!BuildMsg(QUERY_LAST_BLOCK_MSG, {}, msg))
						return;
					SendUdpMessage(addr, msg);
				}
				peers[addr] = high_resolution_clock::now();
				hole_punching_peers.erase(addr);
			}
			return;

		case REQUEST_PEERS_MSG:
			if (is_bootstrap)
			{
				high_resolution_clock::time_point now = high_resolution_clock::now();
				vector<string> good_peers;
				{
					lock_guard<mutex> lock(peers_mtx);
					for (auto it = peers.begin(); it != peers.end(); it++)
						if ((duration_cast<milliseconds>(now - it->second).count() < 40000) && (find(data.begin(), data.end(), it->first) == data.end()) && (it->first != addr))
							good_peers.push_back(it->first);
				}
				if (good_peers.size() != 0)
				{
					auto it = good_peers.begin();
					advance(it, rand() % good_peers.size());
					string msg1, msg2;
					if (!BuildMsg(CONNECT_MSG, { *it }, msg1))
						return;
					if (!BuildMsg(CONNECT_MSG, { addr }, msg2))
						return;
					SendUdpMessage(addr, msg1);
					SendUdpMessage(*it, msg2);
				}
			}
			return;

		case CONNECT_MSG:
			if ((!is_bootstrap) && (addr == bootstrap_addr))
			{
				lock_guard<mutex> lock(peers_mtx);
				lock_guard<mutex> lock2(hole_punching_peers_mtx);
				if (peers.count(data[0]) == 0)
					hole_punching_peers[data[0]] = 20;
			}
			return;

		case QUERY_LAST_BLOCK_MSG:
			if (!is_bootstrap)
			{
				Blockchain *bc = Blockchain::GetInstance();
				string last_block_hash;
				if (!bc->GetBlockHash(bc->GetHeight(), last_block_hash))
					return;
				string msg;
				if (!BuildMsg(BLOCK_HASH_MSG, { last_block_hash }, msg))
					return;
				SendUdpMessage(addr, msg);
			}
			return;

		case BLOCK_HASH_MSG:
			if (!is_bootstrap)
			{
				Blockchain *bc = Blockchain::GetInstance();
				if (!bc->BlockExists(data[0]))
					DownloadBlock(data[0]);
			}
			return;

		case GET_BLOCK_MSG:
			if (!is_bootstrap)
			{
				Blockchain *bc = Blockchain::GetInstance();
				Block block;
				if (!bc->GetBlock(data[0], block))
					return;
				string msg;
				if (!BuildMsg(BLOCK_MSG, { block.ToBinaryData() }, msg))
					return;
				SendUdpMessage(addr, msg);
			}
			return;

		case BLOCK_MSG:
			if (!is_bootstrap)
			{
				Block block;
				try
				{
					block = Block(data[0]);
				}
				catch (int)
				{
					return;
				}
				if (!block.Check())
					return;
				string hash = block.GetHash();
				{
					lock_guard<mutex> new_block(new_block_mtx);
					Blockchain *bc = Blockchain::GetInstance();
					if (bc->BlockExists(hash))
						return;
					{
						lock_guard<mutex> lock3(requested_blocks_mtx);

						auto it = find(requested_blocks.begin(), requested_blocks.end(), hash);
						if (it != requested_blocks.end())
						{
							requested_blocks.erase(it);
						}
						else
						{
							lock_guard<mutex> lock(peers_mtx);
							for (auto p = peers.begin(); p != peers.end(); p++)
								if (p->first != addr)
									SendUdpMessage(p->first, msg);
						}
					}
					bc->AddBlock(block);
				}
			}
			return;

		default:
			return;
		}
	}

}

void Net::Listen()
{
	char buf[upd_len];
	sockaddr_in src;
	int src_size = sizeof(src);
	while (true)
	{
		int recv_len = recvfrom(s, buf, upd_len, 0, (sockaddr *)&src, &src_size);
		if (recv_len == SOCKET_ERROR)
			continue;

		string msg(buf, recv_len);
		string addr = GetAddr(src);
		ProcessMessage(msg, addr);
	}
}

bool Net::BuildMsg(msg_type type, vector<string> data, string &msg)
{
	switch (type)
	{

	case I_AM_ALIVE_MSG:
		if (!data.empty())
			return false;
		msg = "a";
		return true;

	case REQUEST_PEERS_MSG:
		if (data.size() > 12)
			return false;
		msg = "r" + string(1, data.size());
		for (string s : data)
		{
			if (s.length() != 6)
				return false;
			msg += s;
		}
		return true;

	case CONNECT_MSG:
		if ((data.size() != 1) || (data[0].length() != 6))
			return false;
		msg = "c" + data[0];
		return true;

	case QUERY_LAST_BLOCK_MSG:
		if (!data.empty())
			return false;
		msg = "q";
		return true;

	case BLOCK_HASH_MSG:
		if ((data.size() != 1) || (data[0].length() != 32))
			return false;
		msg = "h" + data[0];
		return true;

	case GET_BLOCK_MSG:
		if ((data.size() != 1) || (data[0].length() != 32))
			return false;
		msg = "g" + data[0];
		return true;

	case BLOCK_MSG:
		if ((data.size() != 1) || (data[0].length() > 512))
			return false;
		msg = "b" + data[0];
		return true;

	default:
		return false;
	}
}

void Net::SendUdpMessage(string addr, string msg)
{
	sockaddr_in dst;
	dst.sin_family = AF_INET;
	dst.sin_addr.s_addr = *(ULONG*)&addr[0];
	dst.sin_port = *(USHORT*)&addr[4];

	sendto(s, &msg[0], msg.length(), 0, (sockaddr *)&dst, sizeof(dst));
}

void Net::ContactPeers()
{
	while (true)
	{
		high_resolution_clock::time_point now = high_resolution_clock::now();

		string msg;
		BuildMsg(I_AM_ALIVE_MSG, {}, msg);
		SendUdpMessage(bootstrap_addr, msg);

		{
			lock_guard<mutex> lock(peers_mtx);
			auto it = peers.begin();
			while (it != peers.end())
			{
				if (duration_cast<milliseconds>(now - it->second).count() < 40000)
				{
					string msg;
					BuildMsg(I_AM_ALIVE_MSG, {}, msg);
					SendUdpMessage(it->first, msg);
					it++;
				}
				else
				{
					it = peers.erase(it);
				}
			}
		}

		{
			lock_guard<mutex> lock(peers_mtx);
			lock_guard<mutex> lock2(hole_punching_peers_mtx);
			if (peers.size() + hole_punching_peers.size() < 3)
			{
				vector<string> known_peers;
				for (auto it = peers.begin(); it != peers.end(); it++)
					known_peers.push_back(it->first);
				for (auto it = hole_punching_peers.begin(); it != hole_punching_peers.end(); it++)
					known_peers.push_back(it->first);

				// TODO! Hole punching via NATed hosts
				string msg;
				if (BuildMsg(REQUEST_PEERS_MSG, known_peers, msg))
					SendUdpMessage(bootstrap_addr, msg);
			}
		}

		this_thread::sleep_for(chrono::milliseconds(10000));
	}
}

void Net::PunchHoles()
{
	while (true)
	{
		{
			lock_guard<mutex> lock2(hole_punching_peers_mtx);
			auto it = hole_punching_peers.begin();
			while (it != hole_punching_peers.end())
			{
				if (it->second > 0)
				{
					it->second--;
				}
				else
				{
					it = hole_punching_peers.erase(it);
					continue;
				}
				string msg;
				BuildMsg(I_AM_ALIVE_MSG, {}, msg);
				SendUdpMessage(it->first, msg);
				it++;
			}
		}
		this_thread::sleep_for(chrono::milliseconds(1000));
	}
}

void Net::DownloadBlocks()
{
	while (true)
	{
		{
			lock_guard<mutex> lock(peers_mtx);
			lock_guard<mutex> lock3(requested_blocks_mtx);
			if ((peers.size() != 0) && (requested_blocks.size() != 0))
			{
				for (auto it = requested_blocks.begin(); it != requested_blocks.end(); it++)
				{
					int r = rand() % peers.size();
					string msg;
					if (!BuildMsg(GET_BLOCK_MSG, { *it }, msg))
						continue;
					auto peer = peers.begin();
					advance(peer, r);
					SendUdpMessage(peer->first, msg);
				}
			}
		}
		this_thread::sleep_for(chrono::milliseconds(100));
	}
}
