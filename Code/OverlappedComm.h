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

class OverlappedComm
{
private:
	PlatformString mPort;
	std::shared_ptr<PlatformSerial> mSerial;
	std::wregex mRegATResult = std::wregex(PLATFORMSTR("^(\\+[0-9a-z]+):[\t ]+"), std::wregex::icase);

public:

	std::map<PlatformString, PlatformString> Store;
	void (*OnCommand)(OverlappedComm&, const PlatformString&, const PlatformString&) = 0;

	OverlappedComm()
	{

	}

	OverlappedComm(const PlatformString& port, const std::shared_ptr<PlatformSerial>& serial)
	{
		mPort = port;
		mSerial = serial;
	}

	bool WriteLine(const PlatformString& cmd)
	{
		Utf8String str = PlatformStringToUtf8(cmd);

		return mSerial->WriteLine(str);
	}

	bool ReadLine(PlatformString* line)
	{
		Utf8String str;
		if (mSerial->ReadLine(&str))
		{
			line->assign(Utf8ToPlatformString(str));
			return true;
		}
		return false;
	}

	bool IsOKCommand(const PlatformString& line)
	{
		return Equal(PlatformString(PLATFORMSTR("OK")), line);
	}

	bool IsErrorCommand(const PlatformString& line)
	{
		return Equal(PlatformString(PLATFORMSTR("ERROR")), line);
	}

	bool IsOKOrErrorCommand(const PlatformString& line)
	{
		return this->IsOKCommand(line) || this->IsErrorCommand(line);
	}

	bool ExecuteATCommand(const PlatformString& cmd)
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
			if (std::regex_search(line, match, this->mRegATResult))
			{
				if (this->OnCommand)
				{
					this->OnCommand(*this, match.str(1), match.suffix());
				}
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

	void PerformLoop()
	{
		while (true)
		{
			PlatformString line;
			if (!this->ReadLine(&line))
			{
				return;
			}

			if (line == PLATFORMSTR(""))
			{
				continue;
			}

			std::wsmatch match;
			if (std::regex_search(line, match, this->mRegATResult))
			{
				if (this->OnCommand)
				{
					this->OnCommand(*this, match.str(1), match.suffix());
				}
			}
			else
			{
				this->OutputConsole(PLATFORMSTR("Unhandled unsolicited event: "), line);
			}
		}
	}

	template<typename... Args>
	void OutputConsole(Args&&... args)
	{
		PLATFORMCOUT << PLATFORMSTR("Device at ") << mPort << PLATFORMSTR(": ");

		(PLATFORMCOUT << ... << args);

		PLATFORMCOUT << std::endl;
	}
};