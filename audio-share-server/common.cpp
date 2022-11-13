#include "common.hpp"


std::string wchars_to_mbs(const wchar_t* src)
{
    UINT cp = GetACP();
    int ccWideChar = (int)wcslen(src);
    int n = WideCharToMultiByte(cp, 0, src, ccWideChar, 0, 0, 0, 0);

    std::vector<char> buf(n);
    WideCharToMultiByte(cp, 0, src, ccWideChar, buf.data(), (int)buf.size(), 0, 0);
    std::string dst(buf.data(), buf.size());
    return dst;
}

std::string str_win_err(int err)
{
	LPSTR buf = nullptr;
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		nullptr,
		err,
		0,
		(LPSTR)&buf,
		0,
		nullptr
	);
	std::string msg;
	if (buf) {
		msg.assign(buf);
		LocalFree(buf);
	}
	return msg;
}

std::wstring wstr_win_err(int err)
{
	LPWSTR buf = nullptr;
	FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		nullptr,
		err,
		0,
		(LPWSTR)&buf,
		0,
		nullptr
	);
	std::wstring msg;
	if (buf) {
		msg.assign(buf);
		LocalFree(buf);
	}
	return msg;
}