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

std::map<PlatformString, std::shared_ptr<std::thread>> Ports;
std::mutex PortsLock;

void LoopUntilExit();
bool GetCommDevice(const PlatformString&, OverlappedComm*);
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

void ProcessCommLoop(OverlappedComm& ofm)
{
	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT")))
	{
		ofm.ExecuteATCommand(PLATFORMSTR("ATE1"));

		if (!ofm.ExecuteATCommand(PLATFORMSTR("AT")))
		{
			ofm.OutputConsole(PLATFORMSTR("AT start command failed!"));
			return;
		}
	}

	PlatformString line;
	if (!ofm.ExecuteATCommandResult(PLATFORMSTR("AT+CPIN?"), &line) || !line.starts_with(PLATFORMSTR("+CPIN:")))
	{
		ofm.OutputConsole(PLATFORMSTR("PIN command failed!"));
		return;
	}

	if (!Equal(PlatformString(PLATFORMSTR("READY")), ofm.ParseResult(line)))
	{
		ofm.OutputConsole(PLATFORMSTR("SIM card requires a PIN. Remove the PIN and try again!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CMGF=0")))
	{
		ofm.OutputConsole(PLATFORMSTR("Set PDU mode command failed!"));
		return;
	}

	if (!ofm.ExecuteATCommandResult(PLATFORMSTR("AT+CNUM"), &line) || !line.starts_with(PLATFORMSTR("+CNUM:")))
	{
		ofm.OutputConsole(PLATFORMSTR("Get own number command failed!"));
		return;
	}

	PlatformString number;
	ParseSubscriberNumber(ofm.ParseResult(line), number);

	ofm.OutputConsole(PLATFORMSTR("Phone number is "), number);

	// check if we have unprocessed messages in memory already
	if (!ProcessMessages(number, ofm))
	{
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CREG=1")))
	{
		ofm.OutputConsole(PLATFORMSTR("Enable network registration failed!"));
		return;
	}

	if (!ofm.ExecuteATCommand(PLATFORMSTR("AT+CNMI=2")))
	{
		ofm.OutputConsole(PLATFORMSTR("CNMI (Notify SMS received) command failed!"));
		return;
	}

	while (ofm.ReadLine(&line))
	{
		ofm.OutputConsole(line);

		if (line.starts_with(PLATFORMSTR("+CMTI")))
		{
			ofm.OutputConsole(PLATFORMSTR("Processing new SMS..."));

			if (!ProcessMessages(number, ofm))
			{
				return;
			}
		}
	}
}

bool ProcessMessages(const PlatformString& number, OverlappedComm& ofm)
{
	std::vector<PlatformString> lines;
	if (!ofm.ExecuteATCommandResults(PLATFORMSTR("AT+CMGL=4"), &lines))
	{
		ofm.OutputConsole(PLATFORMSTR("CMGL (List SMS) command failed!"));
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
					std::this_thread::sleep_for(std::chrono::minutes(30));
				}
			}
		}

		if (!ofm.ExecuteATCommand(FormatStr(PLATFORMSTR("AT+CMGD=%i"), index)))
		{
			ofm.OutputConsole(PLATFORMSTR("CMGD (Delete SMS) command failed!"));
			return false;
		}
	}

	return true;
}