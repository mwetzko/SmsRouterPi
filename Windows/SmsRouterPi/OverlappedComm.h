// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <Windows.h>
#include <string>
#include <codecvt>

class OverlappedComm
{
private:
	LPCWSTR mPort;
	HANDLE mCom;
	std::string mReadLineBuffer;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> mConvert;
	HANDLE mWriteReset;
	LPSTR mReadBuffer;
	DWORD mReadBufferNum;
	HANDLE mReadReset;

public:
	OverlappedComm(LPCWSTR port, HANDLE com)
	{
		mPort = port;

		mCom = com;

		mWriteReset = CreateEventW(NULL, TRUE, FALSE, NULL);

		mReadBufferNum = 256;
		mReadBuffer = new CHAR[mReadBufferNum];

		mReadReset = CreateEventW(NULL, TRUE, FALSE, NULL);
	}

	~OverlappedComm()
	{
		CloseHandle(mCom);
		CloseHandle(mWriteReset);
		CloseHandle(mReadReset);
		delete[] mReadBuffer;
	}

	BOOL WaitCancelOverlapped(HANDLE cancel, HANDLE overlapped)
	{
		HANDLE waits[] = { cancel, overlapped };
		DWORD num = WaitForMultipleObjects(sizeof(waits) / sizeof(waits[0]), (const HANDLE*)(&waits), FALSE, INFINITE);

		if (num == MAXDWORD)
		{
			return FALSE;
		}

		if ((num - WAIT_OBJECT_0) == 0)
		{
			return FALSE;
		}

		return TRUE;
	}

	BOOL WriteLine(HANDLE cancel, LPCWSTR cmd)
	{
		std::string str = mConvert.to_bytes(std::wstring(cmd)).append("\r\n");

		DWORD written;
		OVERLAPPED op = { 0 };
		op.hEvent = mWriteReset;
		if (!WriteFile(mCom, str.c_str(), (DWORD)str.size() * sizeof(CHAR), &written, &op))
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				return FALSE;
			}

			if (!WaitCancelOverlapped(cancel, mWriteReset))
			{
				return FALSE;
			}

			if (!GetOverlappedResult(mCom, &op, &written, TRUE))
			{
				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL IsEndLine(std::string::iterator& it)
	{
		return *it == '\r' || *it == '\n';
	}

	std::string::iterator GetNewLinePos(std::string::iterator* end)
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

	BOOL ReadLine(HANDLE cancel, std::wstring* line)
	{
		while (WaitForSingleObject(cancel, 0))
		{
			std::string::iterator end;
			std::string::iterator it = GetNewLinePos(&end);

			if (it != mReadLineBuffer.end())
			{
				auto str = std::string(mReadLineBuffer.begin(), it);

				mReadLineBuffer.erase(mReadLineBuffer.begin(), end);

				if (str.begin() == str.end())
				{
					continue;
				}

				*line = mConvert.from_bytes(str);

				return TRUE;
			}

			DWORD read;
			OVERLAPPED op = { 0 };
			op.hEvent = mReadReset;
			if (!ReadFile(mCom, mReadBuffer, mReadBufferNum, &read, &op))
			{
				if (GetLastError() != ERROR_IO_PENDING)
				{
					return FALSE;
				}

				if (!WaitCancelOverlapped(cancel, mReadReset))
				{
					return FALSE;
				}

				if (!GetOverlappedResult(mCom, &op, &read, TRUE))
				{
					return FALSE;
				}
			}

			mReadLineBuffer.append(mReadBuffer, read);
		}

		return FALSE;
	}

	BOOL IsOKCommand(std::wstring& line)
	{
		return wcscmp(L"OK", line.c_str()) == 0;
	}

	BOOL IsErrorCommand(std::wstring& line)
	{
		return wcscmp(L"ERROR", line.c_str()) == 0;
	}

	BOOL IsOKOrErrorCommand(std::wstring& line)
	{
		return IsOKCommand(line) || IsErrorCommand(line);
	}

	BOOL ExecuteATCommand(HANDLE cancel, LPCWSTR cmd)
	{
		if (!this->WriteLine(cancel, cmd))
		{
			return FALSE;
		}

		std::wstring line;
		if (!this->ReadLine(cancel, &line))
		{
			return FALSE;
		}

		if (wcscmp(cmd, line.c_str()) != 0)
		{
			return FALSE;
		}

		if (!this->ReadLine(cancel, &line))
		{
			return FALSE;
		}

		return IsOKCommand(line);
	}

	BOOL ExecuteATCommandResults(HANDLE cancel, LPCWSTR cmd, std::vector<std::wstring>* lines)
	{
		*lines = std::vector<std::wstring>();

		if (!this->WriteLine(cancel, cmd))
		{
			return FALSE;
		}

		std::wstring line;
		if (!this->ReadLine(cancel, &line))
		{
			return FALSE;
		}

		if (wcscmp(cmd, line.c_str()) != 0)
		{
			return FALSE;
		}

		while (true)
		{
			if (!this->ReadLine(cancel, &line))
			{
				return FALSE;
			}

			if (IsOKOrErrorCommand(line))
			{
				break;
			}

			lines->push_back(line);
		}

		return TRUE;
	}

	BOOL ExecuteATCommandResult(HANDLE cancel, LPCWSTR cmd, std::wstring* line)
	{
		std::vector<std::wstring>lines;
		if (!ExecuteATCommandResults(cancel, cmd, &lines))
		{
			return FALSE;
		}

		auto it = lines.begin();

		if (it != lines.end())
		{
			*line = *it;
		}

		return TRUE;
	}

	void OutputConsole(LPCWSTR msg)
	{
		std::wcout << L"Device at " << mPort << L": " << msg << std::endl;
	}
};