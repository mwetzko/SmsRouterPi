// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "SIM800C.h"
#include "GsmDecoder.h"

SIM800C::SIM800C()
{
	// nothing
}

SIM800C::SIM800C(const PlatformString& port, const std::shared_ptr<PlatformSerial>& serial)
{
	mPort = port;
	mSerial = serial;
	mRecentCaller = PLATFORMSTR("");
	mRecentCallerTime = std::chrono::steady_clock::now();
}

bool SIM800C::WriteLine(const PlatformString& cmd)
{
	Utf8String str = PlatformStringToUtf8(cmd);

	return mSerial->WriteLine(str);
}

bool SIM800C::ReadLine(PlatformString* line)
{
	Utf8String str;
	if (mSerial->ReadLine(&str))
	{
		line->assign(Utf8ToPlatformString(str));
		return true;
	}
	return false;
}

bool SIM800C::ProcessCache()
{
	auto sc = mSmsCache.begin();

	while (sc != mSmsCache.end())
	{
		if (!ProcessSms(sc->Command, sc->PDU))
		{
			return false;
		}

		sc = mSmsCache.erase(sc);
	}

	auto cc = mCallerCache.begin();

	while (cc != mCallerCache.end())
	{
		if (this->OnNewCaller)
		{
			this->OnNewCaller(*this, cc->Caller, cc->Date);
		}

		cc = mCallerCache.erase(cc);
	}

	return true;
}

bool SIM800C::IsOKCommand(const PlatformString& line)
{
	return Equal(PlatformString(PLATFORMSTR("OK")), line);
}

bool SIM800C::IsErrorCommand(const PlatformString& line)
{
	return Equal(PlatformString(PLATFORMSTR("ERROR")), line);
}

bool SIM800C::IsOKOrErrorCommand(const PlatformString& line)
{
	return this->IsOKCommand(line) || this->IsErrorCommand(line);
}

bool SIM800C::ExecuteATCommand(const PlatformString& cmd)
{
	if (!this->WriteLine(cmd))
	{
		return false;
	}

	while (true)
	{
		PlatformString line;
		if (!this->ReadLine(&line))
		{
			return false;
		}

		if (line == PLATFORMSTR("") || line == cmd)
		{
			continue;
		}

		std::wsmatch match;
		if (std::regex_search(line, match, mRegMatchATResult))
		{
			this->OnCommand(match.str(1), match.suffix());
		}
		else if (line == PLATFORMSTR("OK"))
		{
			return true;
		}
		else if (line == PLATFORMSTR("ERROR"))
		{
			return false;
		}
		else
		{
			this->OutputConsole(PLATFORMSTR("Unhandled return: "), line);
		}
	}
}

void SIM800C::PrintNetworkState(const PlatformString& value)
{
	auto state = value;

	auto pos = state.find(PLATFORMSTR(','));

	if (pos != PlatformString::npos)
	{
		state = state.substr(pos + 1);
	}

	if (Equal(PlatformString(PLATFORMSTR("0")), state))
	{
		this->OutputConsole(PLATFORMSTR("Network state change: Disconnected"));
	}
	else if (Equal(PlatformString(PLATFORMSTR("1")), state))
	{
		this->OutputConsole(PLATFORMSTR("Network state change: Connected"));
	}
	else if (Equal(PlatformString(PLATFORMSTR("2")), state))
	{
		this->OutputConsole(PLATFORMSTR("Network state change: Searching..."));
	}
	else
	{
		// 3, 4, 5, etc.
		this->OutputConsole(PLATFORMSTR("Network state change: "), state);
	}
}

void SIM800C::OnCommand(const PlatformString& cmd, const PlatformString& value)
{
	if (cmd == PLATFORMSTR("+CPIN"))
	{
		mStore[cmd] = value;
	}
	else if (cmd == PLATFORMSTR("+CNUM"))
	{
		std::wsmatch match;
		if (std::regex_search(value, match, mRegMatchSubscriberNumber))
		{
			mStore[cmd] = match.str(3);
		}
	}
	else if (cmd == PLATFORMSTR("+CMGL"))
	{
		this->OutputConsole(PLATFORMSTR("New SMS!"));

		PlatformString line;
		if (!this->ReadLine(&line))
		{
			return;
		}

		mSmsCache.push_back({ value, line });
	}
	else if (cmd == PLATFORMSTR("+CREG"))
	{
		PrintNetworkState(value);
	}
	else if (cmd == PLATFORMSTR("+CMTI"))
	{
		mNeedCheckSms = true;
	}
	else if (cmd == PLATFORMSTR("+CRING"))
	{
		this->OutputConsole(PLATFORMSTR("Incoming call: "), value);
	}
	else if (cmd == PLATFORMSTR("+CLIP"))
	{
		std::wsmatch match;
		if (std::regex_search(value, match, mRegMatchCallerId))
		{
			auto caller = match.str(1);
			auto now = std::chrono::steady_clock::now();

			if (mRecentCaller != caller || std::chrono::duration_cast<std::chrono::seconds>(now - mRecentCallerTime).count() > 60)
			{
				std::time_t t = std::time(nullptr);
				std::tm* tx = localtime(&t);

				PlatformStream strm;

				strm << std::put_time(tx, PLATFORMSTR("%FT%T%z"));

				mCallerCache.push_back({ caller, strm.str() });
			}

			mRecentCaller = caller;
			mRecentCallerTime = now;

			this->OutputConsole(PLATFORMSTR("Caller ID: "), caller);
		}
		else
		{
			// must never happen
			this->OutputConsole(PLATFORMSTR("Caller ID: "), value);
		}
	}
	else
	{
		this->OutputConsole(PLATFORMSTR("Unhandled command: "), cmd, PLATFORMSTR(" with value: "), value);
	}
}

bool SIM800C::ProcessSms(const PlatformString& cmd, const PlatformString& pdu)
{
	std::wsmatch match;
	if (!std::regex_search(cmd, match, mRegMatchSmsIndex))
	{
		// ignore this one
		return true;
	}

	int index = std::stoi(match.str(1));

	PlatformString from;
	PlatformString datetime;
	PlatformString message;
	if (ParseGsmPDU(pdu, &from, &datetime, &message))
	{
		if (this->OnNewSms)
		{
			this->OnNewSms(*this, from, datetime, message);
		}
	}

	if (!this->ExecuteATCommand(FormatStr(PLATFORMSTR("AT+CMGD=%i"), index)))
	{
		this->OutputConsole(PLATFORMSTR("CMGD (Delete SMS) command failed!"));
		return false;
	}

	return true;
}

bool SIM800C::Init()
{
	if (!this->ExecuteATCommand(PLATFORMSTR("AT")))
	{
		this->OutputConsole(PLATFORMSTR("AT start command failed!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("ATE0")))
	{
		this->OutputConsole(PLATFORMSTR("ATE start command failed!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CPIN?")))
	{
		this->OutputConsole(PLATFORMSTR("PIN command failed!"));
		return false;
	}

	if (mStore[PLATFORMSTR("+CPIN")] != PLATFORMSTR("READY"))
	{
		this->OutputConsole(PLATFORMSTR("SIM card requires a PIN. Remove the PIN and try again!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CMGF=0")))
	{
		this->OutputConsole(PLATFORMSTR("Set PDU mode command failed!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CRC=1")))
	{
		this->OutputConsole(PLATFORMSTR("Set extended ring mode command failed!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CNUM")))
	{
		this->OutputConsole(PLATFORMSTR("Get own number command failed!"));
		return false;
	}

	auto number = this->GetSubscriberNumber();

	if (number == PLATFORMSTR(""))
	{
		this->OutputConsole(PLATFORMSTR("Subscriber number is empty!"));
		return false;
	}

	this->OutputConsole(PLATFORMSTR("Phone number is "), number);

	this->OutputConsole(PLATFORMSTR("Processing SMS on SIM card..."));

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CMGL=4")))
	{
		this->OutputConsole(PLATFORMSTR("CMGL (List SMS) command failed!"));
		return false;
	}

	if (!this->ProcessCache())
	{
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CREG=1")))
	{
		this->OutputConsole(PLATFORMSTR("Enable network registration status notification failed!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CLIP=1")))
	{
		this->OutputConsole(PLATFORMSTR("Enable caller identification notification failed!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CNMI=2")))
	{
		this->OutputConsole(PLATFORMSTR("Enable SMS notification failed!"));
		return false;
	}

	if (!this->ExecuteATCommand(PLATFORMSTR("AT+CREG?")))
	{
		this->OutputConsole(PLATFORMSTR("Get network registration status failed!"));
		return false;
	}

	return true;
}

bool SIM800C::PerformLoop()
{
	if (this->ExecuteATCommand(PLATFORMSTR("AT")))
	{
		while (true)
		{
			if (mNeedCheckSms)
			{
				mNeedCheckSms = false;

				if (!this->ExecuteATCommand(PLATFORMSTR("AT+CMGL=4")))
				{
					this->OutputConsole(PLATFORMSTR("CMGL (List SMS) command failed!"));
					return false;
				}
			}

			if (!ProcessCache())
			{
				return false;
			}

			PlatformString line;
			if (!this->ReadLine(&line))
			{
				return true;
			}

			if (line == PLATFORMSTR(""))
			{
				continue;
			}

			std::wsmatch match;
			if (std::regex_search(line, match, mRegMatchATResult))
			{
				this->OnCommand(match.str(1), match.suffix());
			}
			else
			{
				this->OutputConsole(PLATFORMSTR("Unhandled unsolicited event: "), line);
			}
		}
	}

	return false;
}

PlatformString SIM800C::GetSubscriberNumber()
{
	return mStore[PLATFORMSTR("+CNUM")];
}