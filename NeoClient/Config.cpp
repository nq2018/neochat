#include "Config.h"

wstring NodeConfig::current_dir;
string NodeConfig::bootstrap_node;
WORD NodeConfig::node_port;

void NodeConfig::Load()
{
	wstring path(1024, 0);
	DWORD path_length = GetModuleFileName(NULL, &path[0], 1024);
	current_dir = path.substr(0, path.find_last_of('\\') + 1);
	path = current_dir + L"config.json";

	ifstream fin(path);
	if (fin.good() && fin.is_open())
	{
		string config((istreambuf_iterator<char>(fin)), (istreambuf_iterator<char>()));
		fin.close();
		json json_config;
		try
		{
			json_config = json::parse(config);
		}
		catch (exception)
		{
			Create(path);
			return;
		}

		if ((json_config["bootstrap_node"] == nullptr) || (json_config["node_port"] == nullptr))
		{
			Create(path);
			return;
		}
		bootstrap_node = json_config["bootstrap_node"].get<string>();
		node_port = json_config["node_port"].get<WORD>();
	}
	else
		Create(path);
}

void NodeConfig::Save()
{
	wstring path = current_dir + L"config.json";

	ofstream fout(path);
	if (fout.good() && fout.is_open())
	{
		json json_config = {
			{ "bootstrap_node", bootstrap_node },
			{ "node_port", node_port }
		};
		fout << json_config.dump(4);
		fout.close();
	}
	else
		FatalError(L"Config file is inaccessible");
}

void NodeConfig::Create(wstring path)
{
	ofstream fout(path);
	if (fout.good() && fout.is_open())
	{
		bootstrap_node = "213.170.100.215:6285";
		node_port = default_node;
		json json_config = {
			{ "bootstrap_node", bootstrap_node },
			{ "node_port", node_port }
		};
		fout << json_config.dump(4);
		fout.close();
	}
	else
		FatalError(L"Config file is inaccessible");
}
