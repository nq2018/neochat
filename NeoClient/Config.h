#pragma once
#include <Windows.h>
#include <fstream>
#include <string>
#include <vector>
#include "json.hpp"
#include "Utils.h"

using namespace std;
using json = nlohmann::json;

class NodeConfig
{
public:
	static wstring current_dir;
	static string bootstrap_node;
	static WORD node_port;
	static const WORD default_node = 6285;

	static void Load();
	static void Save();

private:

	static void Create(wstring path);

};