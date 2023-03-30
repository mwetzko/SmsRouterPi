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
#include "nlohmann/json.hpp"
#include <filesystem>

#define BUFFER_LENGTH 48

VOID CALLBACK TimerCallback(HWND, UINT, UINT_PTR, DWORD);

std::map<PlatformString, std::shared_ptr<std::thread>> Ports;
std::mutex PortsLock;
HANDLE ExitApp;

void RemoveCommPort(const PlatformString&);
void GetRemainingThreads(std::vector<std::shared_ptr<std::thread>>*);
void ProcessCommPort(const PlatformString&);
void ProcessCommLoop(OverlappedComm&);
bool ProcessMessages(const PlatformString&, OverlappedComm&);

PlatformString smtpusername;
PlatformString smtppassword;
PlatformString smtpserver;
PlatformString smtpfromto;

int MainLoop(int argc, PlatformChar** argv)
{
	ExitApp = CreateEventW(NULL, TRUE, FALSE, NULL);

	if (!ExitApp)
	{
		return 3;
	}

	std::map<PlatformString, PlatformString, PlatformCIComparer> parsed;
	ParseArguments(argc, argv, parsed);

	if (!ValidateArguments(parsed, { PLATFORMSTR("username"), PLATFORMSTR("password"), PLATFORMSTR("serverurl"), PLATFORMSTR("fromto") }))
	{
		auto path = std::filesystem::path(argv[0]);

		path = path.parent_path();

		path /= "arguments.json";

		std::ifstream argsfile(path);

		if (argsfile)
		{
			auto data = nlohmann::json::parse(argsfile);

			for (const auto& [key, value] : data.items())
			{
				parsed[Utf8ToPlatformString(key)] = Utf8ToPlatformString(value.get<Utf8String>());
			}
		}

		if (!ValidateArguments(parsed, { PLATFORMSTR("username"), PLATFORMSTR("password"), PLATFORMSTR("serverurl"), PLATFORMSTR("fromto") }))
		{
			PLATFORMCERR << PLATFORMSTR("Usage: ") << argv[0] << PLATFORMSTR(" -username <username> -password <password> -serverurl <serverurl> -fromto <fromto>") << std::endl;
			return 1;
		}
	}

	smtpusername = parsed[PLATFORMSTR("username")];
	smtppassword = parsed[PLATFORMSTR("password")];
	smtpserver = parsed[PLATFORMSTR("serverurl")];
	smtpfromto = parsed[PLATFORMSTR("fromto")];

	if (!SendEmail(PLATFORMSTR("[TEST]"), PLATFORMSTR("[TEST]"), PLATFORMSTR("[TEST]"), PLATFORMSTR("[TEST]"), smtpusername, smtppassword, smtpserver, smtpfromto))
	{
		PLATFORMCERR << PLATFORMSTR("Failed to send test mail!") << std::endl;
		return 2;
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

	std::vector<std::shared_ptr<std::thread>> threads;
	GetRemainingThreads(&threads);

	for (auto it : threads)
	{
		it->join();
	}

	return 0;
}

void EnsureCommPort(const PlatformString& port)
{
	const std::lock_guard<std::mutex> lock(PortsLock);

	auto it = Ports.find(port);

	if (it == Ports.end())
	{
		Ports.insert_or_assign(port, std::make_shared<std::thread>(ProcessCommPort, port));
	}
}

void RemoveCommPort(const PlatformString& port)
{
	const std::lock_guard<std::mutex> lock(PortsLock);

	auto it = Ports.find(port);

	if (it != Ports.end())
	{
		it->second->detach();
		Ports.erase(it);
	}
}

void GetRemainingThreads(std::vector<std::shared_ptr<std::thread>>* threads)
{
	*threads = std::vector<std::shared_ptr<std::thread>>();

	const std::lock_guard<std::mutex> lock(PortsLock);

	for (auto it : Ports)
	{
		threads->push_back(it.second);
	}
}

void ProcessCommPort(const PlatformString& port)
{
	PLATFORMCOUT << PLATFORMSTR("Processing device at ") << port << std::endl;

	HANDLE com = CreateFileW(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	if (com != NULL && com != INVALID_HANDLE_VALUE)
	{
		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout = -1;
		timeouts.ReadTotalTimeoutConstant = -2;
		timeouts.ReadTotalTimeoutMultiplier = -1;
		timeouts.WriteTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;

		SetCommTimeouts(com, &timeouts);

		OverlappedComm ofm(port, com);

		ProcessCommLoop(ofm);
	}

	RemoveCommPort(port);
}

void ProcessCommLoop(OverlappedComm& ofm)
{
	if (!ofm.ExecuteATCommand(ExitApp, PLATFORMSTR("AT")))
	{
		ofm.OutputConsole(PLATFORMSTR("AT start command failed!"));
		return;
	}

	PlatformString line;
	if (!ofm.ExecuteATCommandResult(ExitApp, PLATFORMSTR("AT+CPIN?"), &line))
	{
		ofm.OutputConsole(PLATFORMSTR("PIN command failed!"));
		return;
	}

	if (line.find(PLATFORMSTR("READY")) < 0)
	{
		ofm.OutputConsole(PLATFORMSTR("SIM card requires a PIN. Remove the PIN and try again!"));
		return;
	}

	if (!ofm.ExecuteATCommand(ExitApp, PLATFORMSTR("AT+CMGF=0")))
	{
		ofm.OutputConsole(PLATFORMSTR("Set PDU mode command failed!"));
		return;
	}

	PlatformString number;
	if (!ofm.ExecuteATCommandResult(ExitApp, PLATFORMSTR("AT+CNUM"), &number))
	{
		ofm.OutputConsole(PLATFORMSTR("Get own number command failed!"));
		return;
	}

	PlatformString pnumber;
	ParseSubscriberNumber(number, pnumber);

	ofm.OutputConsole(PLATFORMSTR("Phone number is "), pnumber);

	if (!ProcessMessages(pnumber, ofm))
	{
		return;
	}

	if (!ofm.ExecuteATCommand(ExitApp, PLATFORMSTR("AT+CNMI=2")))
	{
		ofm.OutputConsole(PLATFORMSTR("CNMI (Notify SMS received) command failed!"));
		return;
	}

	while (ofm.ReadLine(ExitApp, &line))
	{
		if (line.starts_with(PLATFORMSTR("+CMTI")))
		{
			ofm.OutputConsole(PLATFORMSTR("Processing new SMS..."));

			if (!ProcessMessages(pnumber, ofm))
			{
				return;
			}
		}
	}
}

bool ProcessMessages(const PlatformString& number, OverlappedComm& ofm)
{
	std::vector<PlatformString> lines;
	if (!ofm.ExecuteATCommandResults(ExitApp, PLATFORMSTR("AT+CMGL=4"), &lines))
	{
		ofm.OutputConsole(PLATFORMSTR("CMGR (Receive SMS) command failed!"));
		return false;
	}

	auto it = lines.begin();

	while (it != lines.end() && it->starts_with(PLATFORMSTR("+CMGL")))
	{
		int index;
		if (!GetMessageIndexFromListing(*it, &index))
		{
			return false;
		}

		if (++it != lines.end())
		{
			PlatformString from;
			PlatformString datetime;
			PlatformString message;
			if (ParseGsmPDU(*(it++), &from, &datetime, &message))
			{
				while (!SendEmail(from, number, datetime, message, smtpusername, smtppassword, smtpserver, smtpfromto))
				{
					ofm.OutputConsole(PLATFORMSTR("Failed to send email. Trying again in 30 minutes."));
					Sleep(1800000);
				}
			}
		}

		if (!ofm.ExecuteATCommand(ExitApp, FormatStr(PLATFORMSTR("AT+CMGD=%i"), index).c_str()))
		{
			ofm.OutputConsole(PLATFORMSTR("CMGD (Delete SMS) command failed!"));
			return false;
		}
	}

	return true;
}