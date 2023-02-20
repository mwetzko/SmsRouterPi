// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "OverlappedComm.h"
#include "GsmDecoder.h"
#include "Shared.h"

#pragma comment (lib, "SetupAPI.lib")
#pragma comment (lib, "Crypt32.lib")

#define BUFFER_LENGTH 48

VOID CALLBACK TimerCallback(HWND, UINT, UINT_PTR, DWORD);

std::map<std::wstring, HANDLE> Ports;
std::mutex PortsLock;

HANDLE ExitApp;

void CheckHardwareID(DWORD, DWORD);
void EnsureCommPort(LPCWSTR);
void RemoveCommPort(LPCWSTR);
void GetRemainingThreads(std::vector<HANDLE>*);
DWORD WINAPI ProcessCommPort(LPVOID);
void ProcessCommLoop(OverlappedComm&);
BOOL ProcessMessages(const std::wstring&, OverlappedComm&);

std::wstring smtpusername;
std::wstring smtppassword;
std::wstring smtpserver;
std::wstring smtpfromto;

int wmain(int argc, wchar_t* argv[])
{
	std::map<std::wstring, std::wstring, WideStringCIComparer> parsed;
	ParseArguments(argc, argv, parsed);

	if (!ValidateArguments(parsed, { L"username", L"password", L"serverurl", L"fromto" }))
	{
		std::wcerr << L"Usage: " << argv[0] << L" -username <username> -password <password> -serverurl <serverurl> -fromto <fromto>" << std::endl;
		return 1;
	}

	smtpusername = parsed[L"username"];
	smtppassword = parsed[L"password"];
	smtpserver = parsed[L"serverurl"];
	smtpfromto = parsed[L"fromto"];

	if (!SendEmail(L"[TEST]", L"[TEST]", L"[TEST]", L"[TEST]", smtpusername, smtppassword, smtpserver, smtpfromto))
	{
		std::wcerr << L"Failed to send test mail!" << std::endl;
		return 2;
	}

	ExitApp = CreateEventW(NULL, TRUE, FALSE, NULL);

	if (!ExitApp)
	{
		return 3;
	}

	UINT_PTR timer = SetTimer(NULL, NULL, 10000, TimerCallback);

	MSG msg = { 0 };
	while (GetMessageW(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	KillTimer(NULL, timer);

	SetEvent(ExitApp);

	std::vector<HANDLE> threads;
	GetRemainingThreads(&threads);

	for (auto it : threads)
	{
		WaitForSingleObject(it, INFINITE);
	}

	return 0;
}

VOID CALLBACK TimerCallback(HWND, UINT, UINT_PTR, DWORD)
{
	CheckHardwareID(0x1A86, 0x7523);
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

void EnsureCommPort(LPCWSTR port)
{
	const std::lock_guard<std::mutex> lock(PortsLock);

	auto it = Ports.find(port);

	if (it == Ports.end())
	{
		Ports.insert_or_assign(port, CreateThread(NULL, 0, ProcessCommPort, new std::wstring(port), 0, NULL));
	}
}

void RemoveCommPort(LPCWSTR port)
{
	const std::lock_guard<std::mutex> lock(PortsLock);

	auto it = Ports.find(port);

	if (it != Ports.end())
	{
		Ports.erase(it);
	}
}

void GetRemainingThreads(std::vector<HANDLE>* threads)
{
	*threads = std::vector<HANDLE>();

	const std::lock_guard<std::mutex> lock(PortsLock);

	for (auto it : Ports)
	{
		threads->push_back(it.second);
	}
}

DWORD WINAPI ProcessCommPort(LPVOID state)
{
	std::wstring* port = (std::wstring*)state;

	std::wcout << L"Processing device at " << *port << std::endl;

	HANDLE com = CreateFileW(port->c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	if (com != NULL && com != INVALID_HANDLE_VALUE)
	{
		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout = -1;
		timeouts.ReadTotalTimeoutConstant = -2;
		timeouts.ReadTotalTimeoutMultiplier = -1;
		timeouts.WriteTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;

		SetCommTimeouts(com, &timeouts);

		OverlappedComm ofm(port->c_str(), com);

		ProcessCommLoop(ofm);
	}

	RemoveCommPort(port->c_str());

	delete state;

	return 0;
}

void ProcessCommLoop(OverlappedComm& ofm)
{
	if (!ofm.ExecuteATCommand(ExitApp, L"AT"))
	{
		ofm.OutputConsole(L"AT start command failed!");
		return;
	}

	std::wstring line;
	if (!ofm.ExecuteATCommandResult(ExitApp, L"AT+CPIN?", &line))
	{
		ofm.OutputConsole(L"PIN command failed!");
		return;
	}

	if (!wcsstr(line.c_str(), L"READY"))
	{
		ofm.OutputConsole(L"SIM card requires a PIN. Remove the PIN and try again!");
		return;
	}

	if (!ofm.ExecuteATCommand(ExitApp, L"AT+CMGF=0"))
	{
		ofm.OutputConsole(L"Set PDU mode command failed!");
		return;
	}

	std::wstring number;
	if (!ofm.ExecuteATCommandResult(ExitApp, L"AT+CNUM", &number))
	{
		ofm.OutputConsole(L"Get own number command failed!");
		return;
	}

	std::wstring pnumber;
	ParseSubscriberNumber(number, pnumber);

	if (!ProcessMessages(pnumber, ofm))
	{
		return;
	}

	if (!ofm.ExecuteATCommand(ExitApp, L"AT+CNMI=2"))
	{
		ofm.OutputConsole(L"CNMI (Notify SMS received) command failed!");
		return;
	}

	while (ofm.ReadLine(ExitApp, &line))
	{
		if (wcsstr(line.c_str(), L"+CMTI") == line.c_str())
		{
			if (!ProcessMessages(pnumber, ofm))
			{
				return;
			}
		}
	}
}

BOOL ProcessMessages(const std::wstring& number, OverlappedComm& ofm)
{
	std::vector<std::wstring> lines;
	if (!ofm.ExecuteATCommandResults(ExitApp, L"AT+CMGL=4", &lines))
	{
		ofm.OutputConsole(L"CMGR (Receive SMS) command failed!");
		return FALSE;
	}

	auto it = lines.begin();

	while (it != lines.end() && wcsstr(it->c_str(), L"+CMGL") == it->c_str())
	{
		int index;
		if (!GetMessageIndexFromListing(*it, &index))
		{
			return FALSE;
		}

		if (++it != lines.end())
		{
			std::wstring from;
			std::wstring datetime;
			std::wstring message;
			if (ParseGsmPDU(*(it++), &from, &datetime, &message))
			{
				while (!SendEmail(from, number, datetime, message, smtpusername, smtppassword, smtpserver, smtpfromto))
				{
					ofm.OutputConsole(L"Failed to send email. Trying again in 30 minutes.");
					Sleep(1800000);
				}
			}
		}

		if (!ofm.ExecuteATCommand(ExitApp, FormatStr(L"AT+CMGD=%i", index).c_str()))
		{
			ofm.OutputConsole(L"CMGD (Delete SMS) command failed!");
			return FALSE;
		}
	}

	return TRUE;
}