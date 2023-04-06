// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <string>
#include <memory>

#define PLATFORMSTR(x) L##x
#define PLATFORMCOUT std::wcout
#define PLATFORMCERR std::wcerr

using PlatformString = std::wstring;
using PlatformChar = PlatformString::value_type;
using PlatformStream = std::basic_stringstream<PlatformString::value_type, std::char_traits<PlatformString::value_type>, std::allocator<PlatformString::value_type>>;

using Utf8String = std::string;
using Utf8Char = Utf8String::value_type;

int MainLoop(int, PlatformChar**);
void EnsureCommPort(const PlatformString&);

#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)

#include <sdkddkver.h>
#include <boost/asio.hpp>
#include <Windows.h>

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

#endif