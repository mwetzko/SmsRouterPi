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