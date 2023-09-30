/*
   Copyright 2022-2023 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

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