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

#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)

using PlatformString = std::wstring;
#define PLATFORMSTR(x) L##x
#define PLATFORMCOUT std::wcout
#define PLATFORMCERR std::wcerr

#endif

using PlatformChar = std::_Get_element_type<PlatformString>::type;

using Utf8String = std::string;
using Utf8Char = std::_Get_element_type<Utf8String>::type;

int MainLoop(int, PlatformChar**);
void EnsureCommPort(const PlatformString&);
void CheckHardwareID(std::uint32_t, std::uint32_t);

#if defined(WINDOWS) || defined(WIN32) || defined(_WIN32)

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