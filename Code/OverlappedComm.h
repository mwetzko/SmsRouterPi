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
public:
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
			*line = Utf8ToPlatformString(str);
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

		PlatformString line;
		if (!this->ReadLine(&line))
		{
			return false;
		}

		if (Equal(cmd, line))
		{
			if (!this->ReadLine(&line))
			{
				return false;
			}
		}

		return this->IsOKCommand(line);
	}

	bool ExecuteATCommandResults(const PlatformString& cmd, std::vector<PlatformString>* lines)
	{
		*lines = std::vector<PlatformString>();

		if (!this->WriteLine(cmd))
		{
			return false;
		}

		PlatformString line;
		if (!this->ReadLine(&line))
		{
			return false;
		}

		if (Equal(cmd, line))
		{
			if (!this->ReadLine(&line))
			{
				return false;
			}
		}

		while (true)
		{
			if (this->IsOKOrErrorCommand(line))
			{
				break;
			}

			lines->push_back(line);

			if (!this->ReadLine(&line))
			{
				return false;
			}
		}

		return true;
	}

	bool ExecuteATCommandResult(const PlatformString& cmd, PlatformString* line)
	{
		std::vector<PlatformString>lines;
		if (!this->ExecuteATCommandResults(cmd, &lines))
		{
			return false;
		}

		auto it = lines.begin();

		if (it != lines.end())
		{
			*line = *it;
		}

		return true;
	}

	template<typename... Args>
	void OutputConsole(Args&&... args)
	{
		PLATFORMCOUT << PLATFORMSTR("Device at ") << mPort << PLATFORMSTR(": ");

		(PLATFORMCOUT << ... << args);

		PLATFORMCOUT << std::endl;
	}

	PlatformString ParseResult(const PlatformString& result)
	{
		for (auto it = result.begin(); it != result.end(); it++)
		{
			if (*it == PLATFORMSTR(' '))
			{
				break;
			}
			else if (*it == PLATFORMSTR(':'))
			{
				it++;

				while (it != result.end() && *it == PLATFORMSTR(' '))
				{
					it++;
				}

				return PlatformString(it, result.end());
			}
		}

		return result;
	}
};