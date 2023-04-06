// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

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

Utf8String PlatformStringToUtf8(const PlatformString& str);
PlatformString Utf8ToPlatformString(const Utf8String& str);
PlatformString UCS2ToPlatformString(const std::u16string& str);

template<typename... Args>
PlatformString FormatStr(const PlatformString& format, Args... args);

struct PlatformCIComparer
{
	bool operator()(const PlatformString& a, const PlatformString& b) const
	{
		auto ax = PlatformString(a);
		auto bx = PlatformString(b);

		std::transform(ax.begin(), ax.end(), ax.begin(), ::tolower);
		std::transform(bx.begin(), bx.end(), bx.begin(), ::tolower);

		return a < b;
	}
};

void ParseArguments(int argc, PlatformChar* argv[], std::map<PlatformString, PlatformString, PlatformCIComparer>& parsed);
bool ValidateArguments(const std::map<PlatformString, PlatformString, PlatformCIComparer>& parsed, const std::vector<PlatformString>& required);
bool SendEmail(const PlatformString& from, const PlatformString& to, const PlatformString& datetime, const PlatformString& message, const PlatformString& smtpusername, const PlatformString& smtppassword, const PlatformString& smtpserver, const PlatformString& smtpfromto);
void ParseSubscriberNumber(const PlatformString& number, PlatformString& pnumber);

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

template<typename T>
bool IsValidHandleValue(T arg)
{
	if constexpr (std::is_same_v<T, HANDLE>)
	{
		return arg != NULL && arg != INVALID_HANDLE_VALUE;
	}

	return arg != NULL;
}

template<typename T, typename D>
class SafeDeleter
{
private:
	D* mDeleter;
public:
	SafeDeleter(D* deleter) :mDeleter(deleter)
	{
		// nothing
	}

	void operator()(T* arg)
	{
		if (IsValidHandleValue(arg))
		{
			mDeleter(arg);
		}
	}
};

template<typename T>
class SafeHandle :public std::shared_ptr<std::remove_pointer_t<T>>
{
public:
	SafeHandle()
	{
		// nothing
	}
	template<typename D>
	SafeHandle(T value, D* deleter) : std::shared_ptr<std::remove_pointer_t<T>>(value, SafeDeleter<std::remove_pointer_t<T>, D>(deleter))
	{
		// nothing
	}

	operator T() const
	{
		return this->get();
	}

	operator bool() const
	{
		return IsValidHandleValue(this->get());
	}
};