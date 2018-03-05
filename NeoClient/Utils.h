#pragma once
#include <Windows.h>
#include <algorithm>
#include <codecvt>
#include <string>

using namespace std;

string StringToHex(string data);
string HexToString(string hex);
string WstringToString(wstring wstr);
wstring StringToWstring(string str);
bool FileExists(wstring name);
bool DirectoryExists(wstring path);
void FatalError(wstring msg);