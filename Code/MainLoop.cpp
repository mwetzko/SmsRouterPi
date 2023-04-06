// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Shared.h"
#include "OverlappedComm.h"
#include "GsmDecoder.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <chrono>
#include <thread>

std::wregex RegMatchSmsIndex = std::wregex(PLATFORMSTR("^([0-9]+),"), std::wregex::icase);
std::wregex RegMatchCallerId = std::wregex(PLATFORMSTR("^([^,]+),"), std::wregex::icase);

std::map<PlatformString, std::shared_ptr<std::thread>> Ports;
std::mutex PortsLock;

void LoopUntilExit();
bool GetCommDevice(const PlatformString&, OverlappedComm*);
void RemoveCommPort(const PlatformString&);
void GetRemainingThreads(std::vector<std::shared_ptr<std::thread>>*);
void ProcessCommPort(const PlatformString&);
void ProcessCommLoop(OverlappedComm&);
void ProcessMessage(const PlatformString&, const PlatformString&, const PlatformString&, OverlappedComm&);
void PrintNetworkState(const PlatformString&, OverlappedComm&);

PlatformString smtpusername;
PlatformString smtppassword;
PlatformString smtpserver;
PlatformString smtpfromto;

int MainLoop(int argc, PlatformChar** argv)
{
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

#if !_DEBUG
	if (!SendEmail(PLATFORMSTR("[TEST]"), PLATFORMSTR("[TEST]"), PLATFORMSTR("[TEST]"), PLATFORMSTR("[TEST]"), smtpusername, smtppassword, smtpserver, smtpfromto))
	{
		PLATFORMCERR << PLATFORMSTR("Failed to send test mail!") << std::endl;
		return 2;
	}
#endif

	LoopUntilExit();

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

	OverlappedComm ofm;
	if (GetCommDevice(port, &ofm))
	{
		ProcessCommLoop(ofm);
	}

	RemoveCommPort(port);
}

void OnCommand(OverlappedComm& ofm, const PlatformString& cmd, const PlatformString& value)
{
	if (cmd == PLATFORMSTR("+CPIN"))
	{
		ofm.Store[cmd] = value;
	}
	else if (cmd == PLATFORMSTR("+CNUM"))
	{
		ofm.Store[cmd] = value;
	}
	else if (cmd == PLATFORMSTR("+CMGL"))
	{
		ofm.OutputConsole(PLATFORMSTR("New SMS!"));

		PlatformString line;
		if (!ofm.ReadLine(&line))
		{
			return;
		}

		ProcessMessage(ofm.Store[PLATFORMSTR("+CNUM")], value, line, ofm);
	}
	else if (cmd == PLATFORMSTR("+CREG"))
	{
		PrintNetworkState(value, ofm);
	}
	else if (cmd == PLATFORMSTR("+CMTI"))
	{
		if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CMGL=4")))
		{
			ofm.OutputConsole(PLATFORMSTR("CMGL (List SMS) command failed!"));
			return;
		}
	}
	else if (cmd == PLATFORMSTR("+CRING"))
	{
		ofm.OutputConsole(PLATFORMSTR("Incoming call: "), value);
	}
	else if (cmd == PLATFORMSTR("+CLIP"))
	{
		std::wsmatch match;
		if (std::regex_search(value, match, RegMatchCallerId))
		{
			ofm.OutputConsole(PLATFORMSTR("Caller ID: "), match.str(1));
		}
		else
		{
			ofm.OutputConsole(PLATFORMSTR("Caller ID: "), value);
		}
	}
	else
	{
		ofm.OutputConsole(PLATFORMSTR("Unhandled command: "), cmd, PLATFORMSTR(" with value: "), value);
	}
}

void ProcessCommLoop(OverlappedComm& ofm)
{
	ofm.OnCommand = OnCommand;

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT")))
	{
		ofm.OutputConsole(PLATFORMSTR("AT start command failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("ATE0")))
	{
		ofm.OutputConsole(PLATFORMSTR("ATE start command failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CPIN?")))
	{
		ofm.OutputConsole(PLATFORMSTR("PIN command failed!"));
		return;
	}

	if (ofm.Store[PLATFORMSTR("+CPIN")] != PLATFORMSTR("READY"))
	{
		ofm.OutputConsole(PLATFORMSTR("SIM card requires a PIN. Remove the PIN and try again!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CMGF=0")))
	{
		ofm.OutputConsole(PLATFORMSTR("Set PDU mode command failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CRC=1")))
	{
		ofm.OutputConsole(PLATFORMSTR("Set extended ring mode command failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CNUM")))
	{
		ofm.OutputConsole(PLATFORMSTR("Get own number command failed!"));
		return;
	}

	if (ofm.Store[PLATFORMSTR("+CNUM")] == PLATFORMSTR(""))
	{
		ofm.OutputConsole(PLATFORMSTR("Own number is empty!"));
		return;
	}

	PlatformString number;
	ParseSubscriberNumber(ofm.Store[PLATFORMSTR("+CNUM")], number);

	ofm.Store[PLATFORMSTR("+CNUM")] = number;

	ofm.OutputConsole(PLATFORMSTR("Phone number is "), number);

	ofm.OutputConsole(PLATFORMSTR("Processing SMS on SIM card..."));

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CMGL=4")))
	{
		ofm.OutputConsole(PLATFORMSTR("CMGL (List SMS) command failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CREG=1")))
	{
		ofm.OutputConsole(PLATFORMSTR("Enable network registration status notification failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CLIP=1")))
	{
		ofm.OutputConsole(PLATFORMSTR("Enable caller identification notification failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CNMI=2")))
	{
		ofm.OutputConsole(PLATFORMSTR("Enable SMS notification failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CREG?")))
	{
		ofm.OutputConsole(PLATFORMSTR("Get network registration status failed!"));
		return;
	}

	ofm.OutputConsole(PLATFORMSTR("Ready. Waiting for event..."));

	while (ofm.ExecuteATCommand(PLATFORMSTR("AT")))
	{
		ofm.PerformLoop();
	}
}

void ProcessMessage(const PlatformString& number, const PlatformString& cmd, const PlatformString& value, OverlappedComm& ofm)
{
	std::wsmatch match;
	if (!std::regex_search(cmd, match, RegMatchSmsIndex))
	{
		return;
	}

	int index = std::stoi(match.str(1));

	PlatformString from;
	PlatformString datetime;
	PlatformString message;
	if (ParseGsmPDU(value, &from, &datetime, &message))
	{
		while (!SendEmail(from, number, datetime, message, smtpusername, smtppassword, smtpserver, smtpfromto))
		{
			ofm.OutputConsole(PLATFORMSTR("Failed to send email. Trying again in 30 minutes."));
			std::this_thread::sleep_for(std::chrono::minutes(1));
		}
	}

	if (!ofm.ExecuteATCommand(FormatStr(PLATFORMSTR("AT+CMGD=%i"), index)))
	{
		ofm.OutputConsole(PLATFORMSTR("CMGD (Delete SMS) command failed!"));
		return;
	}
}

void PrintNetworkState(const PlatformString& value, OverlappedComm& ofm)
{
	auto state = value;

	auto pos = state.find(PLATFORMSTR(','));

	if (pos != PlatformString::npos)
	{
		state = state.substr(pos + 1);
	}

	if (Equal(PlatformString(PLATFORMSTR("0")), state))
	{
		ofm.OutputConsole(PLATFORMSTR("Network state change: Disconnected"));
	}
	else if (Equal(PlatformString(PLATFORMSTR("1")), state))
	{
		ofm.OutputConsole(PLATFORMSTR("Network state change: Connected"));
	}
	else if (Equal(PlatformString(PLATFORMSTR("2")), state))
	{
		ofm.OutputConsole(PLATFORMSTR("Network state change: Searching..."));
	}
	else
	{
		// 3, 4, 5, etc.
		ofm.OutputConsole(PLATFORMSTR("Network state change: "), state);
	}
}