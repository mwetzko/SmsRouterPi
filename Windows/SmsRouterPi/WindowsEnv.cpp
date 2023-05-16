// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Shared.h"
#include "SIM800C.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <Ntddser.h>

SafeFdPtr ExclusiveProcess;

BOOL WINAPI CtrlHandler(DWORD);

int wmain(int argc, wchar_t* argv[])
{
	std::setlocale(LC_ALL, "iv.utf8");

	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	std::vector<PlatformString> vec;

	for (int i = 0; i < argc; i++)
	{
		vec.push_back(argv[i]);
	}

	return MainLoop(vec);
}

bool CheckExclusiveProcess(const std::filesystem::path& exe)
{
	auto lockerFile = PlatformString(exe);

	lockerFile.append(PLATFORMSTR(".lock"));

	ExclusiveProcess = SafeFdPtr(_wopen(lockerFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, _S_IWRITE));

	return ExclusiveProcess;
}

void CheckHardwareID(DWORD vid, DWORD pid)
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

void HandleTimer()
{
	CheckHardwareID(0x1A86, 0x7523);
}

PlatformString UCS2ToPlatformString(const std::u16string& str)
{
	// on windows wstring is u16string
	return PlatformString(str.begin(), str.end());
}

class PlatformSerialWindows :public PlatformSerial
{
private:
	SafeHANDLE mCom;
	SafeHANDLE mWriteReset;
	SafeHANDLE mReadReset;
private:
	bool WaitCancelOverlapped(HANDLE overlapped)
	{
		for (int i = 0; i < 150; i++)
		{
			auto res = WaitForSingleObject(overlapped, 100);

			if (res == WAIT_OBJECT_0)
			{
				return true;
			}
			else if (res == WAIT_TIMEOUT)
			{
				if (WaitExitOrTimeout(100ms))
				{
					break;
				}
			}
			else
			{
				break;
			}
		}

		return false;
	}
public:
	PlatformSerialWindows(const SafeHANDLE& com, const SafeHANDLE& writeReset, const SafeHANDLE& readReset) :PlatformSerial()
	{
		mCom = com;
		mWriteReset = writeReset;
		mReadReset = readReset;
	}

	bool WriteLine(const Utf8String& cmd)
	{
		auto str = cmd;
		str.append("\n");

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

	bool ReadLine(Utf8String* line)
	{
		while (!WaitExitOrTimeout(0ms))
		{
			Utf8String str;
			if (CanReadLine(&str))
			{
				line->assign(str);
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

bool GetCommDevice(const PlatformString& port, SIM800C* sim)
{
	SafeHANDLE com = SafeHANDLE(CreateFileW(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0));

	if (com)
	{
		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout = -1;
		timeouts.ReadTotalTimeoutConstant = -2;
		timeouts.ReadTotalTimeoutMultiplier = -1;
		timeouts.WriteTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;

		SetCommTimeouts(com, &timeouts);

		DCB dcb = { 0 };
		dcb.DCBlength = sizeof(DCB);
		dcb.BaudRate = 9600;
		dcb.ByteSize = DATABITS_8;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;

		SetCommState(com, &dcb);

		PurgeComm(com, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

		auto writeReset = SafeHANDLE(CreateEventW(NULL, TRUE, FALSE, NULL));

		if (writeReset)
		{
			auto readReset = SafeHANDLE(CreateEventW(NULL, TRUE, FALSE, NULL));

			if (readReset)
			{
				*sim = SIM800C(port, std::make_shared<PlatformSerialWindows>(com, writeReset, readReset));

				return true;
			}
		}
	}

	return false;
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		PLATFORMCOUT << PLATFORMSTR("Received closing event...") << std::endl;
		ExitReset.Set();
		return TRUE;
	default:
		return FALSE;
	}
}