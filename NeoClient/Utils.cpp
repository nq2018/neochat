#include "Utils.h"

string StringToHex(string data)
{
	static const char* const hex = "0123456789abcdef";
	size_t len = data.length();

	std::string output;
	for (size_t i = 0; i < len; ++i)
	{
		const unsigned char c = data[i];
		output.push_back(hex[c >> 4]);
		output.push_back(hex[c & 15]);
	}
	return output;
}

string HexToString(string data)
{
	static const char* const hex = "0123456789abcdef";
	size_t len = data.length();
	if (len & 1)
	{
		data = "0" + data;
		len++;
	}
	transform(data.begin(), data.end(), data.begin(), ::tolower);

	std::string output;
	for (size_t i = 0; i < len; i += 2)
	{
		char a = data[i];
		const char* p = std::lower_bound(hex, hex + 16, a);
		if (*p != a)
			return "";

		char b = data[i + 1];
		const char* q = std::lower_bound(hex, hex + 16, b);
		if (*q != b)
			return "";

		output.push_back(((p - hex) << 4) | (q - hex));
	}
	return output;
}

string WstringToString(wstring wstr)
{
	wstring_convert<codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(wstr);
}

wstring StringToWstring(string str)
{
	wstring_convert<codecvt_utf8<wchar_t>> myconv;
	return myconv.from_bytes(str);
}

bool FileExists(wstring name)
{
	struct _stat buffer;
	return (_wstat(name.c_str(), &buffer) == 0);
}

bool DirectoryExists(wstring path)
{
	DWORD dwAttrib = GetFileAttributes(path.c_str());

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void FatalError(wstring msg)
{
	MessageBox(NULL, msg.c_str(), L"Error", MB_OK | MB_ICONERROR);
	exit(0);
}