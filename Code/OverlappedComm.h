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
	HANDLE mCom;
	Utf8String mReadLineBuffer;
	HANDLE mWriteReset;
	Utf8Char* mReadBuffer;
	std::uint32_t mReadBufferNum;
	HANDLE mReadReset;

public:
	OverlappedComm(const PlatformString& port, HANDLE com)
	{
		mPort = port;

		mCom = com;

		mWriteReset = CreateEventW(NULL, TRUE, FALSE, NULL);

		mReadBufferNum = 256;
		mReadBuffer = new Utf8Char[mReadBufferNum];

		mReadReset = CreateEventW(NULL, TRUE, FALSE, NULL);
	}

	~OverlappedComm()
	{
		CloseHandle(mCom);
		CloseHandle(mWriteReset);
		CloseHandle(mReadReset);
		delete[] mReadBuffer;
	}

	bool WaitCancelOverlapped(HANDLE cancel, HANDLE overlapped)
	{
		HANDLE waits[] = { cancel, overlapped };
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

	bool WriteLine(HANDLE cancel, const PlatformString& cmd)
	{
		Utf8String str = PlatformStringToUtf8(cmd).append("\r\n");

		DWORD written;
		OVERLAPPED op = { 0 };
		op.hEvent = mWriteReset;
		if (!WriteFile(mCom, str.c_str(), (DWORD)str.size() * sizeof(Utf8Char), &written, &op))
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				return false;
			}

			if (!WaitCancelOverlapped(cancel, mWriteReset))
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

	bool IsEndLine(Utf8String::iterator& it)
	{
		return *it == '\r' || *it == '\n';
	}

	Utf8String::iterator GetNewLinePos(Utf8String::iterator* end)
	{
		for (auto it = mReadLineBuffer.begin(); it != mReadLineBuffer.end(); it++)
		{
			if (IsEndLine(it))
			{
				*end = it;

				while (++(*end) != mReadLineBuffer.end() && IsEndLine(*end));

				return it;
			}
		}

		return mReadLineBuffer.end();
	}

	bool ReadLine(HANDLE cancel, PlatformString* line)
	{
		while (WaitForSingleObject(cancel, 0))
		{
			Utf8String::iterator end;
			Utf8String::iterator it = GetNewLinePos(&end);

			if (it != mReadLineBuffer.end())
			{
				auto str = Utf8String(mReadLineBuffer.begin(), it);

				mReadLineBuffer.erase(mReadLineBuffer.begin(), end);

				if (str.begin() == str.end())
				{
					continue;
				}

				*line = Utf8ToPlatformString(str);

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

				if (!WaitCancelOverlapped(cancel, mReadReset))
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

	bool IsOKCommand(PlatformString& line)
	{
		return wcscmp(PLATFORMSTR("OK"), line.c_str()) == 0;
	}

	bool IsErrorCommand(PlatformString& line)
	{
		return wcscmp(PLATFORMSTR("ERROR"), line.c_str()) == 0;
	}

	bool IsOKOrErrorCommand(PlatformString& line)
	{
		return IsOKCommand(line) || IsErrorCommand(line);
	}

	bool ExecuteATCommand(HANDLE cancel, const PlatformString& cmd)
	{
		if (!this->WriteLine(cancel, cmd))
		{
			return false;
		}

		PlatformString line;
		if (!this->ReadLine(cancel, &line))
		{
			return false;
		}

		if (!Equal(cmd, line))
		{
			return false;
		}

		if (!this->ReadLine(cancel, &line))
		{
			return false;
		}

		return IsOKCommand(line);
	}

	bool ExecuteATCommandResults(HANDLE cancel, const PlatformString& cmd, std::vector<PlatformString>* lines)
	{
		*lines = std::vector<PlatformString>();

		if (!this->WriteLine(cancel, cmd))
		{
			return false;
		}

		PlatformString line;
		if (!this->ReadLine(cancel, &line))
		{
			return false;
		}

		if (!Equal(cmd, line))
		{
			return false;
		}

		while (true)
		{
			if (!this->ReadLine(cancel, &line))
			{
				return false;
			}

			if (IsOKOrErrorCommand(line))
			{
				break;
			}

			lines->push_back(line);
		}

		return true;
	}

	bool ExecuteATCommandResult(HANDLE cancel, const PlatformString& cmd, PlatformString* line)
	{
		std::vector<PlatformString>lines;
		if (!ExecuteATCommandResults(cancel, cmd, &lines))
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

	void OutputConsole(const PlatformString& msg)
	{
		PLATFORMCOUT << PLATFORMSTR("Device at ") << mPort << PLATFORMSTR(": ") << msg << std::endl;
	}
};