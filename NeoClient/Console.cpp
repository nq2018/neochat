#include <Windows.h>
#include <iostream>
#include <string>
#include "Config.h"
#include "Network.h"
#include "Blockchain.h"

using namespace std;

int main()
{
	set_terminate([]() { ExitProcess(1); });
	srand(time(0));

	NodeConfig::Load();
	Net* net = Net::GetInstance();
	Blockchain *bc = Blockchain::GetInstance();
	string cmd;

	cout << "NeoClient v1.0\n";

	while (true)
	{
		cout << "> ";
		getline(cin, cmd);
		if ((cmd.length() > 5) && (cmd.substr(0, 5) == "mine "))
		{
			cout << "Mining...\n";
			string msg = cmd.substr(5);
			string hash;
			if (!bc->GetBlockHash(bc->GetHeight(), hash))
			{
				cout << "Error getting current block hash\n";
				continue;
			}
			Block b(hash, msg);
			b.Mine();
			if (bc->AddBlock(b))
			{
				net->PublishNewBlock(b);
				cout << "Success!\n";
			}
			else
				cout << "Something went wrong... Try again\n";
		}
		else if ((cmd.length() > 5) && (cmd.substr(0, 5) == "show "))
		{
			u64 count = stoi(cmd.substr(5));
			if (count == 0)
			{
				cout << "Bad number of blocks\n";
			}
			else
			{
				u64 cur_height = bc->GetHeight();
				while ((count > 0) && (cur_height > 0))
				{
					Block b;
					if (!bc->GetBlock(cur_height, b))
					{
						cout << "Error loading block\n";
						break;
					}
					cout << "Block #" << cur_height << '\n';
					cout << "Block hash: " << StringToHex(b.GetHash()) << '\n';
					cout << "Prev block: " << StringToHex(b.GetPrevHash()) << '\n';
					cout << "Data: " << b.GetData() << '\n';
					cout << '\n';
					count--; cur_height--;
				}
				if ((count > 0) && (cur_height == 0))
				{
					cout << "Block #0 (genesis block)\n\n";
				}
			}
		}
		else if ((cmd.length() == 5) && (cmd == "peers"))
		{
			cout << net->DumpPeers();
		}
		else if ((cmd.length() == 4) && (cmd == "help"))
		{
			cout << "Available commands:\n"
				<< "mine DATA - create a new block, put DATA in it and add it to the blockchain\n"
				<< "show N - display last N blocks\n"
				<< "peers - display all peers\n"
				<< "help - display this message\n";
		}
		else
		{
			cout << "Unknown command. Type \"help\" to see the list of available commands.\n";
		}
	}
}