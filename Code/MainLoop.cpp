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
#include "nlohmann/json.hpp"
#include <chrono>
#include <thread>

std::filesystem::path RootPath;

std::map<PlatformString, std::shared_ptr<std::thread>> Ports;
std::mutex PortsLock;

void HandleTimer();
bool GetCommDevice(const std::filesystem::path&, const PlatformString&, SIM800C*);
void RemoveCommPort(const PlatformString&);
void ProcessCommPort(const PlatformString&);
void ProcessCommLoop(SIM800C&);
void OnNewSms(SIM800C&, const PlatformString&, const PlatformString&, const PlatformString&);
void OnNewCaller(SIM800C&, const PlatformString&, const PlatformString&);

PlatformString smtpusername;
PlatformString smtppassword;
PlatformString smtpserver;
PlatformString smtpfromto;

struct EmailData
{
	PlatformString Subject;
	PlatformString Message;
};

void DoEmailProcessingIfNecessary();
void AddProcessEmail(const EmailData&);
void ProcessSendEmail();

std::vector<EmailData> Emails;
std::mutex EmailsLock;

std::thread EmailThread;
std::mutex EmailThreadLock;

WaitResetEvent ExitReset;

size_t GetRemainingThreads()
{
	const std::lock_guard<std::mutex> lock(PortsLock);

	return Ports.size();
}

void PrintUsage(const std::vector<PlatformString>& args)
{
	ConsoleErr(PLATFORMSTR("Usage: "), args[0], PLATFORMSTR(" -username <username> -password <password> -serverurl <serverurl> -fromto <fromto>"));
}

int MainLoop(const std::vector<PlatformString>& args)
{
	auto exe = std::filesystem::path(args[0]);

	if (!CheckExclusiveProcess(exe))
	{
		return -1;
	}

	RootPath = exe.parent_path();

	std::map<PlatformString, PlatformString, PlatformCIComparer> parsed;
	ParseArguments(args, parsed);

	if (!ValidateArguments(parsed, { PLATFORMSTR("username"), PLATFORMSTR("password"), PLATFORMSTR("serverurl"), PLATFORMSTR("fromto") }))
	{
		auto path = RootPath / PLATFORMSTR("arguments.json");

		PlatformString json;
		if (ReadAllText(path, json))
		{
			auto data = nlohmann::json::parse(json);

			for (const auto& [key, value] : data.items())
			{
				parsed[Utf8ToPlatformString(key)] = Utf8ToPlatformString(value.get<Utf8String>());
			}

			if (!ValidateArguments(parsed, { PLATFORMSTR("username"), PLATFORMSTR("password"), PLATFORMSTR("serverurl"), PLATFORMSTR("fromto") }))
			{
				PrintUsage(args);
				return 1;
			}
		}
		else
		{
			PrintUsage(args);
			return 1;
		}
	}

	smtpusername = parsed[PLATFORMSTR("username")];
	smtppassword = parsed[PLATFORMSTR("password")];
	smtpserver = parsed[PLATFORMSTR("serverurl")];
	smtpfromto = parsed[PLATFORMSTR("fromto")];

#if !_DEBUG
	if (!SendEmail(PLATFORMSTR("[TEST]"), PLATFORMSTR("[TEST]"), smtpusername, smtppassword, smtpserver, smtpfromto))
	{
		ConsoleErr(PLATFORMSTR("Failed to send test mail!"));
		return 2;
	}
#endif

	HandleTimer();

	while (!WaitExitOrTimeout(10s))
	{
		HandleTimer();
	}

	while (GetRemainingThreads() > 0)
	{
		std::this_thread::sleep_for(100ms);
	}

	if (EmailThread.joinable())
	{
		try
		{
			EmailThread.join();
		}
		catch (const std::exception&)
		{
			// nothing
		}
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

void ProcessCommPort(const PlatformString& port)
{
	ConsoleOut(PLATFORMSTR("Processing device at "), port);

	SIM800C sim;
	if (GetCommDevice(RootPath, port, &sim))
	{
		ProcessCommLoop(sim);
	}

	RemoveCommPort(port);
}

void ProcessCommLoop(SIM800C& sim)
{
	sim.OnNewSms = OnNewSms;
	sim.OnNewCaller = OnNewCaller;

	if (!sim.Init())
	{
		return;
	}

	sim.OutputConsole(PLATFORMSTR("Ready. Waiting for event..."));

	while (sim.PerformLoop())
	{
		DoEmailProcessingIfNecessary();
	}
}

void OnNewSms(SIM800C& sim, const PlatformString& from, const PlatformString& date, const PlatformString& message)
{
	auto msg = PlatformString(PLATFORMSTR("Sender: ")).append(from)
		.append(PLATFORMSTR("\r\n"))
		.append(PLATFORMSTR("Receiver: ")).append(sim.GetSubscriberNumber())
		.append(PLATFORMSTR("\r\n"))
		.append(PLATFORMSTR("Date: ")).append(date)
		.append(PLATFORMSTR("\r\n\r\n"))
		.append(message);

	EmailData ed = { PLATFORMSTR("SMS received"), msg };

	AddProcessEmail(ed);
}

void OnNewCaller(SIM800C& sim, const PlatformString& caller, const PlatformString& date)
{
	auto msg = PlatformString(PLATFORMSTR("Caller: ")).append(caller)
		.append(PLATFORMSTR("\r\n"))
		.append(PLATFORMSTR("Callee: ")).append(sim.GetSubscriberNumber())
		.append(PLATFORMSTR("\r\n"))
		.append(PLATFORMSTR("Date: ")).append(date);

	EmailData ed = { PLATFORMSTR("Call received"), msg };

	AddProcessEmail(ed);
}

void FireEmailThread()
{
	const std::lock_guard<std::mutex> lock(EmailThreadLock);

	if (!EmailThread.joinable())
	{
		EmailThread = std::thread(ProcessSendEmail);
	}
}

void DoEmailProcessingIfNecessary()
{
	const std::lock_guard<std::mutex> lock(EmailsLock);

	if (Emails.size() > 0)
	{
		FireEmailThread();
	}
}

void AddProcessEmail(const EmailData& data)
{
	const std::lock_guard<std::mutex> lock(EmailsLock);

	Emails.push_back(data);

	FireEmailThread();
}

bool GetNextEmailData(EmailData* data)
{
	const std::lock_guard<std::mutex> lock(EmailsLock);

	auto it = Emails.begin();

	if (it == Emails.end())
	{
		return false;
	}

	*data = *it;

	Emails.erase(it);

	return true;
}

void ProcessSendEmail()
{
	int num = 0;

	EmailData data;
	while (GetNextEmailData(&data))
	{
		if (SendEmail(data.Subject, data.Message, smtpusername, smtppassword, smtpserver, smtpfromto))
		{
			num = 0;
		}
		else
		{
			if ((num + 1) < 5)
			{
				num += 1;
			}

			if (WaitExitOrTimeout(num * 1min))
			{
				break;
			}
		}
	}

	const std::lock_guard<std::mutex> lock(EmailThreadLock);

	EmailThread.detach();
}