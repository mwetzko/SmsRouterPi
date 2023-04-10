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
#include <vector>
#include <functional>
#include <filesystem>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PLATFORMSTR(x) L##x
#define PLATFORMCOUT std::wcout
#define PLATFORMCERR std::wcerr

using PlatformString = std::wstring;
using PlatformChar = PlatformString::value_type;
using PlatformStream = std::basic_stringstream<PlatformChar, std::char_traits<PlatformChar>, std::allocator<PlatformChar>>;

using Utf8String = std::string;
using Utf8Char = Utf8String::value_type;

using byte = unsigned char;

int MainLoop(const std::vector<PlatformString>&);
bool CheckExclusiveProcess(const std::filesystem::path&);
void EnsureCommPort(const PlatformString&);

#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)

#pragma warning(push)
#pragma warning(disable: 4302)
#pragma warning(disable: 4311)
#pragma warning(disable: 4312)

#endif

template<typename T>
class SafeHandlePtr :public std::shared_ptr<T>
{
private:
	std::function<bool(T)> mFuncValidValue;
protected:
	SafeHandlePtr()
	{
		// nothing
	}
public:
	template<typename D>
	SafeHandlePtr(T value, std::function<bool(T)> funcValidValue, D* deleter) : std::shared_ptr<T>((T*)value, [funcValidValue, deleter](T* v) { if (funcValidValue((T)v)) deleter((T)v); })
	{
		mFuncValidValue = funcValidValue;
	}

	operator T() const
	{
		return (T)this->get();
	}

	operator bool() const
	{
		return mFuncValidValue((T)this->get());
	}
};

#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)

#pragma warning(pop)

#include <sdkddkver.h>
#include <boost/asio.hpp>
#include <Windows.h>
#include <io.h>

class SafeHANDLE :public SafeHandlePtr<HANDLE>
{
public:
	SafeHANDLE()
	{
		// nothing
	}

	SafeHANDLE(HANDLE handle) :SafeHandlePtr<HANDLE>(handle, [](HANDLE v) { return v != NULL && v != INVALID_HANDLE_VALUE; }, CloseHandle)
	{
		// nothing
	}
};

#define PLATFORMCLOSE _close

#else

#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <sys/file.h>

typedef union _LARGE_INTEGER {
	struct {
		uint32_t LowPart;
		int32_t HighPart;
	};
	struct {
		uint32_t LowPart;
		int32_t HighPart;
	} u;
	int64_t QuadPart;
} LARGE_INTEGER, * PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
	struct {
		uint32_t LowPart;
		uint32_t HighPart;
	};
	struct {
		uint32_t LowPart;
		uint32_t HighPart;
	} u;
	uint64_t QuadPart;
} ULARGE_INTEGER, * PULARGE_INTEGER;

#if defined(_M_IX86) && !defined(_M_ARM) && !defined(_M_ARM64) && !defined(MIDL_PASS)&& !defined(RC_INVOKED) && !defined(_M_CEE_PURE)
#define Int32x32To64(a,b) __emul(a,b)
#define UInt32x32To64(a,b) __emulu(a,b)
#else
#define Int32x32To64(a,b) (((int64_t)(int32_t)(a))*((int64_t)(int32_t)(b)))
#define UInt32x32To64(a,b) ((uint64_t)(uint32_t)(a)*(uint64_t)(uint32_t)(b))
#endif

uint32_t RtlEnlargedUnsignedDivide(ULARGE_INTEGER, uint32_t, uint32_t*);
int MulDiv(int, int, int);

template<typename T>
Utf8String EnsurePathString(const T& path)
{
	if constexpr (std::is_same_v<T, std::filesystem::path>)
	{
		return path;
	}
	else
	{
		return PlatformStringToUtf8(path);
	}
}

#define PLATFORMCLOSE close

#endif

class SafeFdPtr :public SafeHandlePtr<int>
{
public:
	SafeFdPtr()
	{
		// nothing
	}

	SafeFdPtr(int fd) :SafeHandlePtr<int>(fd, [](int v) { return !(v < 0); }, PLATFORMCLOSE)
	{
		// nothing
	}
};