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

#define PLATFORMSTR(x) L##x
#define PLATFORMCOUT std::wcout
#define PLATFORMCERR std::wcerr

using PlatformString = std::wstring;
using PlatformChar = PlatformString::value_type;
using PlatformStream = std::basic_stringstream<PlatformChar, std::char_traits<PlatformChar>, std::allocator<PlatformChar>>;

using Utf8String = std::string;
using Utf8Char = Utf8String::value_type;

int MainLoop(const std::vector<PlatformString>&);
void EnsureCommPort(const PlatformString&);

#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)

#include <sdkddkver.h>
#include <boost/asio.hpp>
#include <Windows.h>

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

#else

#include <sys/time.h>

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

uint32_t RtlEnlargedUnsignedDivide(ULARGE_INTEGER Dividend, uint32_t  Divisor, uint32_t* Remainder)
{
	if (Remainder)
	{
		*Remainder = (uint32_t)(Dividend.QuadPart % Divisor);
	}

	return (uint32_t)(Dividend.QuadPart / Divisor);
}

int MulDiv(int nNumber, int nNumerator, int nDenominator)
{
	LARGE_INTEGER Result;
	int Negative;

	Negative = nNumber ^ nNumerator ^ nDenominator;

	if (nNumber < 0) nNumber *= -1;
	if (nNumerator < 0) nNumerator *= -1;
	if (nDenominator < 0) nDenominator *= -1;

	Result.QuadPart = Int32x32To64(nNumber, nNumerator) + (nDenominator / 2);

	if (nDenominator > Result.HighPart)
	{
		Result.LowPart = RtlEnlargedUnsignedDivide(*(PULARGE_INTEGER)&Result, (uint32_t)nDenominator, (uint32_t*)&Result.HighPart);

		if ((int)Result.LowPart >= 0)
		{
			return (Negative >= 0) ? (int)Result.LowPart : -(int)Result.LowPart;
		}
	}

	return -1;
}

#endif