// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include <map>
#include <string>
#include <algorithm>
#include <mailio/message.hpp>
#include <mailio/smtp.hpp>
#include <Windows.h>
#include <SetupAPI.h>
#include <Ntddser.h>

std::wstring FormatStr(LPCWSTR format, ...)
{
	std::wstring str(wcslen(format) + 64, '0');

	va_list args;

	va_start(args, format);

	int ans = _vsnwprintf_s((LPWSTR)str.c_str(), str.size(), str.size(), format, args);

	while (ans < 0)
	{
		str.resize(str.size() + 1024);

		ans = _vsnwprintf_s((LPWSTR)str.c_str(), str.size(), str.size(), format, args);
	}

	va_end(args);

	return std::wstring(str.c_str());
}

struct WideStringCIComparer
{
	bool operator()(const std::wstring& a, const std::wstring& b) const
	{
		auto ax = std::wstring(a);
		auto bx = std::wstring(b);

		std::transform(ax.begin(), ax.end(), ax.begin(), ::tolower);
		std::transform(bx.begin(), bx.end(), bx.begin(), ::tolower);

		return a < b;
	}
};

void ParseArguments(int argc, wchar_t* argv[], std::map<std::wstring, std::wstring, WideStringCIComparer>& parsed)
{
	auto it = parsed.end();

	for (int i = 1; i < argc; i++)
	{
		if (wcslen(argv[i]) > 0)
		{
			if (argv[i][0] == L'-' || argv[i][0] == L'/')
			{
				it = parsed.insert_or_assign(&argv[i][1], std::wstring()).first;
			}
			else if (it != parsed.end())
			{
				it->second = argv[i];
			}
		}
	}
}

bool ValidateArguments(const std::map<std::wstring, std::wstring, WideStringCIComparer>& parsed, const std::vector<std::wstring>& required)
{
	for (auto it : required)
	{
		auto find = parsed.find(it);

		if (find == parsed.end())
		{
			return false;
		}

		if (find->second.size() == 0)
		{
			return false;
		}
	}

	return true;
}

std::string wstringToUtf8(const std::wstring& str)
{
	auto needed = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0, NULL, NULL);

	std::string cvt(needed, 0);

	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), (LPSTR)cvt.c_str(), (int)cvt.size(), NULL, NULL);

	return cvt;
}

std::wstring Utf8ToWstring(const std::string& str)
{
	auto needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);

	std::wstring cvt(needed, 0);

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), (LPWSTR)cvt.c_str(), (int)cvt.size());

	return cvt;
}

bool SendEmail(const std::wstring& from, const std::wstring& to, const std::wstring& datetime, const std::wstring& message, const std::wstring& smtpusername, const std::wstring& smtppassword, const std::wstring& smtpserver, const std::wstring& smtpfromto)
{
	try
	{
		mailio::message msg;
		msg.from(mailio::mail_address(wstringToUtf8(smtpfromto), wstringToUtf8(smtpfromto)));
		msg.add_recipient(mailio::mail_address(wstringToUtf8(smtpfromto), wstringToUtf8(smtpfromto)));
		msg.subject(wstringToUtf8(L"SMS received"));
		msg.content(wstringToUtf8(std::wstring(L"Sender: ").append(from).append(L"\r\n").
			append(L"Tel received: ").append(to).append(L"\r\n").
			append(L"Date received: ").append(datetime).append(L"\r\n\r\n").
			append(message)));

		mailio::smtps conn(wstringToUtf8(smtpserver), 587);
		conn.authenticate(wstringToUtf8(smtpusername), wstringToUtf8(smtppassword), mailio::smtps::auth_method_t::START_TLS);
		conn.submit(msg);

		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

void ParseSubscriberNumber(const std::wstring& number, std::wstring& pnumber)
{
	bool st = false;
	bool qt = false;

	for (auto it : number)
	{
		if (it == L',')
		{
			if (qt)
			{
				pnumber.push_back(it);
			}
			else if (st)
			{
				return;
			}
			else
			{
				st = true;
			}
		}
		else if (it == L'"')
		{
			qt = !qt;
		}
		else if (st)
		{
			pnumber.push_back(it);
		}
	}
}