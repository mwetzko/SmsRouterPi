// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "Env.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <map>
#include <string>
#include <algorithm>
#include <mailio/message.hpp>
#include <mailio/smtp.hpp>
#include <regex>

template<typename ...Args>
PlatformString FormatStr(const PlatformString& format, Args... args)
{
	PlatformString str(format.size() + 64, '0');

	int ans = _snwprintf_s((PlatformChar*)str.c_str(), str.size(), str.size(), format.c_str(), args...);

	while (ans < 0)
	{
		str.resize(str.size() + 1024);

		ans = _snwprintf_s((PlatformChar*)str.c_str(), str.size(), str.size(), format.c_str(), args...);
	}

	return PlatformString(str.c_str());
}

template<typename From, typename To>
To ConvertMultiByte(const From& str, auto cvt)
{
	auto astr = str.c_str();
	std::mbstate_t state = std::mbstate_t();
	std::size_t len = 1 + cvt(nullptr, &astr, 0, &state);
	std::vector<typename To::value_type> mbstr(len);
	cvt(mbstr.data(), &astr, mbstr.size(), &state);
	return To(mbstr.data());
}

Utf8String PlatformStringToUtf8(const PlatformString&);
PlatformString Utf8ToPlatformString(const Utf8String&);
PlatformString UCS2ToPlatformString(const std::u16string&);

struct PlatformCIComparer
{
	bool operator()(const PlatformString& a, const PlatformString& b) const
	{
		for (auto ait = a.begin(), bit = b.begin(); ait != a.end() && bit != b.end(); ait++, bit++)
		{
			auto aa = std::towlower(*ait);
			auto bb = std::towlower(*bit);

			if (aa < bb)
			{
				return true;
			}
			else if (bb < aa)
			{
				return false;
			}
		}

		return false;
	}
};

void ParseArguments(const std::vector<PlatformString>& args, std::map<PlatformString, PlatformString, PlatformCIComparer>& parsed);
bool ValidateArguments(const std::map<PlatformString, PlatformString, PlatformCIComparer>& parsed, const std::vector<PlatformString>& required);
bool WaitOrExitApp();
bool SendEmail(const PlatformString& subject, const PlatformString& message, const PlatformString& smtpusername, const PlatformString& smtppassword, const PlatformString& smtpserver, const PlatformString& smtpfromto);

template<typename T>
bool Equal(const T& a, const T& b)
{
	return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

class PlatformSerial
{
private:
	std::regex mRegMatchNL = std::regex("^([^\r\n]*)[\r\n]+", std::regex::icase);
protected:
	Utf8String mReadLineBuffer;
	Utf8Char* mReadBuffer;
	std::uint32_t mReadBufferNum;

	PlatformSerial()
	{
		mReadBufferNum = 256;
		mReadBuffer = new Utf8Char[mReadBufferNum];
	}

	~PlatformSerial()
	{
		delete[] mReadBuffer;
	}

	bool CanReadLine(Utf8String* line)
	{
		std::smatch match;
		if (!std::regex_search(mReadLineBuffer, match, this->mRegMatchNL))
		{
			return false;
		}

		line->assign(match.str(1));

		mReadLineBuffer = match.suffix();

		return true;
	}

public:
	virtual bool WriteLine(const Utf8String& cmd) = 0;
	virtual bool ReadLine(Utf8String* line) = 0;
};