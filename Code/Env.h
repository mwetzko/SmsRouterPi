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

#include <SDKDDKVer.h>

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

#endif