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
#include <fstream>
#include <iostream>
#include <vector>
#include <mutex>
#include <map>
#include <string>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <filesystem>
#include <condition_variable>
#include <curl/curl.h>

using namespace std::chrono_literals;

template<typename ...Args>
PlatformString FormatStr(const PlatformString& format, Args... args)
{
	PlatformString str(format.size() + 64, '0');

	int ans = std::swprintf((PlatformChar*)str.c_str(), str.size(), format.c_str(), args...);

	while (ans < 0)
	{
		str.resize(str.size() + 1024);

		ans = std::swprintf((PlatformChar*)str.c_str(), str.size(), format.c_str(), args...);
	}

	return PlatformString(str.c_str());
}

template<typename From, typename To>
To ConvertMultiByte(const From& str, auto cvt)
{
	auto astr = str.c_str();
	std::mbstate_t state = std::mbstate_t();
	std::size_t len = cvt(nullptr, &astr, 0, &state);

	if (len != static_cast<std::size_t>(-1))
	{
		std::vector<typename To::value_type> mbstr(len);
		len = cvt(mbstr.data(), &astr, mbstr.size(), &state);

		if (len != static_cast<std::size_t>(-1))
		{
			return To(mbstr.data(), len);
		}
	}

	return To();
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
			auto aa = ::towlower(*ait);
			auto bb = ::towlower(*bit);

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

class WaitResetEvent
{
protected:
	std::condition_variable mCV;
	std::mutex mMutex;
	bool mSignaled;
public:
	void Set()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mSignaled = true;
		mCV.notify_all();
	}
	void Reset()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mSignaled = false;
	}
	template <class _Rep, class _Period>
	bool WaitOrTimeout(const std::chrono::duration<_Rep, _Period>& _Rel_time)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		return mCV.wait_for(lock, _Rel_time, [this] { return this->mSignaled; });
	}
};

extern WaitResetEvent ExitReset;

template <class _Rep, class _Period>
bool WaitExitOrTimeout(const std::chrono::duration<_Rep, _Period>& _Rel_time)
{
	return ExitReset.WaitOrTimeout(_Rel_time);
}

template<typename T, typename S>
bool ReadAll(const T& filename, S& content, std::ios_base::openmode mode = std::ios_base::in)
{
	std::basic_ifstream<typename S::value_type, std::char_traits<typename S::value_type>> strm(filename, mode);

	if (!strm)
	{
		return false;
	}

	content = S(std::istreambuf_iterator<typename S::value_type>(strm), std::istreambuf_iterator<typename S::value_type>());

	return true;
}

template<typename T, typename S>
bool ReadAllText(const T& filename, S& content)
{
	return ReadAll(filename, content);
}