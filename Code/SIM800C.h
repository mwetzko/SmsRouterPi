// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "Shared.h"

class SIM800C
{
private:
	struct SmsCacheItem
	{
		PlatformString Command;
		PlatformString PDU;
	};
	struct CallerCacheItem
	{
		PlatformString Caller;
		PlatformString Date;
	};

	std::filesystem::path mRoot;
	PlatformString mPort;
	std::shared_ptr<PlatformSerial> mSerial;
	std::wregex mRegMatchATResult = std::wregex(PLATFORMSTR("^(\\+[0-9a-z]+):[\t ]+"), std::wregex::icase);
	std::wregex mRegMatchSmsIndex = std::wregex(PLATFORMSTR("^([0-9]+),"), std::wregex::icase);
	std::wregex mRegMatchCallerId = std::wregex(PLATFORMSTR("^['\"]?([^,'\"]+)['\"]?,"), std::wregex::icase);
	std::wregex mRegMatchSubscriberNumber = std::wregex(PLATFORMSTR("^(?:(['\"]).*?\\1)?,(['\"])(.*?)\\2,"), std::wregex::icase);
	std::map<PlatformString, PlatformString> mStore;
	std::vector<SmsCacheItem> mSmsCache;
	bool mNeedCheckSms = false;
	std::vector<CallerCacheItem> mCallerCache;
	PlatformString mRecentCaller;
	std::chrono::steady_clock::time_point mRecentCallerTime;

	bool WriteLine(const PlatformString&);
	bool ReadLine(PlatformString*);
	bool ProcessCache();
	bool IsOKCommand(const PlatformString&);
	bool IsErrorCommand(const PlatformString&);
	bool IsOKOrErrorCommand(const PlatformString&);
	bool ExecuteATCommand(const PlatformString&);
	bool ExecuteATCommand(const PlatformString&, PlatformString*);
	void PrintNetworkState(const PlatformString&);
	void OnCommand(const PlatformString&, const PlatformString&);
	bool ProcessSms(const PlatformString&, const PlatformString&);

public:

	void (*OnNewSms)(SIM800C&, const PlatformString&, const PlatformString&, const PlatformString&) = 0;
	void (*OnNewCaller)(SIM800C&, const PlatformString&, const PlatformString&) = 0;

	SIM800C();
	SIM800C(const std::filesystem::path&, const PlatformString&, const std::shared_ptr<PlatformSerial>&);

	bool Init();
	bool PerformLoop();

	template<typename... Args>
	void OutputConsole(Args&&... args)
	{
		const std::lock_guard<std::mutex> lock(ConsoleLock);

		PLATFORMCOUT << PLATFORMSTR("Device at ") << mPort << PLATFORMSTR(": ");

		(PLATFORMCOUT << ... << args);

		PLATFORMCOUT << std::endl;
	}

	PlatformString GetSubscriberNumber();
};