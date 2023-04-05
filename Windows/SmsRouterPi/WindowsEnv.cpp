// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma comment (lib, "SetupAPI.lib")
#pragma comment (lib, "Crypt32.lib")

#include "Shared.h"
#include "OverlappedComm.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <Ntddser.h>

SafeHandle<HANDLE> ExitApp;

int wmain(int argc, wchar_t* argv[])
{
	ExitApp = SafeHandle(CreateEventW(NULL, TRUE, FALSE, NULL), CloseHandle);

	if (!ExitApp)
	{
		return 3;
	}

	return MainLoop(argc, argv);
}

void CheckHardwareID(std::uint32_t vid, std::uint32_t pid)
{
	std::wstring format = FormatStr(L"USB\\VID_%X&PID_%X", vid, pid);

	LPWSTR str = new WCHAR[MAX_PATH];

	HDEVINFO devInfo = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR, NULL, NULL, DIGCF_PRESENT);

	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (DWORD dwDeviceIndex = 0; SetupDiEnumDeviceInfo(devInfo, dwDeviceIndex, &devInfoData); dwDeviceIndex++)
	{
		DWORD size = MAX_PATH;
		if (SetupDiGetDeviceInstanceIdW(devInfo, &devInfoData, str, size, &size))
		{
			if (std::wstring(str).find(format) == 0)
			{
				HKEY reg = SetupDiOpenDevRegKey(devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

				size = MAX_PATH * sizeof(WCHAR);
				if (RegGetValueW(reg, NULL, L"PortName", RRF_RT_REG_SZ, NULL, str, &size) == ERROR_SUCCESS)
				{
					EnsureCommPort(str);
				}

				RegCloseKey(reg);
			}
		}
	}

	if (devInfo)
	{
		SetupDiDestroyDeviceInfoList(devInfo);
	}

	delete[] str;
}

void CALLBACK TimerCallback(HWND, UINT, UINT_PTR, DWORD)
{
	CheckHardwareID(0x1A86, 0x7523);
}

void LoopUntilExit()
{
	UINT_PTR timer = SetTimer(NULL, NULL, 10000, TimerCallback);

	MSG msg = { 0 };
	while (GetMessageW(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	KillTimer(NULL, timer);

	SetEvent(ExitApp);
}

Utf8String PlatformStringToUtf8(const PlatformString& str)
{
	auto needed = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0, NULL, NULL);

	Utf8String cvt(needed, 0);

	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), (LPSTR)cvt.c_str(), (int)cvt.size(), NULL, NULL);

	return cvt;
}

PlatformString Utf8ToPlatformString(const Utf8String& str)
{
	auto needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);

	PlatformString cvt(needed, 0);

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), (PlatformChar*)cvt.c_str(), (int)cvt.size());

	return cvt;
}

PlatformString UCS2ToPlatformString(const std::u16string& str)
{
	// on windows wstring is u16string
	return PlatformString(str.begin(), str.end());
}

class PlatformSerialWindows :public PlatformSerial
{
private:
	SafeHandle<HANDLE> mCom;
	SafeHandle<HANDLE> mCancel; // ExitApp
	SafeHandle<HANDLE> mWriteReset;
	SafeHandle<HANDLE> mReadReset;
private:
	bool WaitCancelOverlapped(HANDLE overlapped)
	{
		HANDLE waits[] = { mCancel, overlapped };
		DWORD num = WaitForMultipleObjects(sizeof(waits) / sizeof(waits[0]), (const HANDLE*)(&waits), FALSE, INFINITE);

		if (num == MAXDWORD)
		{
			return false;
		}

		if ((num - WAIT_OBJECT_0) == 0)
		{
			return false;
		}

		return true;
	}
public:
	PlatformSerialWindows(const SafeHandle<HANDLE>& com, const SafeHandle<HANDLE>& writeReset, const SafeHandle<HANDLE>& readReset, const SafeHandle<HANDLE>& cancel) :PlatformSerial()
	{
		mCom = com;
		mWriteReset = writeReset;
		mReadReset = readReset;
		mCancel = cancel;
	}

	bool WriteLine(const Utf8String& cmd)
	{
		auto str = cmd;
		str.append("\r\n");

		DWORD written;
		OVERLAPPED op = { 0 };
		op.hEvent = mWriteReset;
		if (!WriteFile(mCom, str.c_str(), (DWORD)str.size() * sizeof(Utf8Char), &written, &op))
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				return false;
			}

			if (!this->WaitCancelOverlapped(mWriteReset))
			{
				return false;
			}

			if (!GetOverlappedResult(mCom, &op, &written, TRUE))
			{
				return false;
			}
		}

		return true;
	}

	bool ReadLine(Utf8String* str)
	{
		while (WaitForSingleObject(mCancel, 0))
		{
			Utf8String::iterator end;
			Utf8String::iterator it = this->GetNewLinePos(&end);

			if (it != mReadLineBuffer.end())
			{
				auto stra = Utf8String(mReadLineBuffer.begin(), it);

				mReadLineBuffer.erase(mReadLineBuffer.begin(), end);

				if (stra.begin() == stra.end())
				{
					continue;
				}

				*str = stra;

				return true;
			}

			DWORD read;
			OVERLAPPED op = { 0 };
			op.hEvent = mReadReset;
			if (!ReadFile(mCom, mReadBuffer, mReadBufferNum, &read, &op))
			{
				if (GetLastError() != ERROR_IO_PENDING)
				{
					return false;
				}

				if (!this->WaitCancelOverlapped(mReadReset))
				{
					return false;
				}

				if (!GetOverlappedResult(mCom, &op, &read, TRUE))
				{
					return false;
				}
			}

			mReadLineBuffer.append(mReadBuffer, read);
		}

		return false;
	}
};

bool GetCommDevice(const PlatformString& port, OverlappedComm* ofm)
{
	SafeHandle com = SafeHandle(CreateFileW(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0), CloseHandle);

	if (com)
	{
		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout = -1;
		timeouts.ReadTotalTimeoutConstant = -2;
		timeouts.ReadTotalTimeoutMultiplier = -1;
		timeouts.WriteTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;

		SetCommTimeouts(com, &timeouts);

		auto writeReset = SafeHandle(CreateEventW(NULL, TRUE, FALSE, NULL), CloseHandle);

		if (writeReset)
		{
			auto readReset = SafeHandle(CreateEventW(NULL, TRUE, FALSE, NULL), CloseHandle);

			if (readReset)
			{
				*ofm = OverlappedComm(port, std::make_shared<PlatformSerialWindows>(com, writeReset, readReset, ExitApp));

				return true;
			}
		}
	}

	return false;
}