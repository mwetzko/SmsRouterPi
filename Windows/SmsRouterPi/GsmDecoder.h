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

// DO NOT CHANGE ============================================================================================================================================
WCHAR GsmPage0[] = L"@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞ\x1bÆæßÉ !\"#¤%&'()*+,-./0123456789:;<=>?¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§¿abcdefghijklmnopqrstuvwxyzäöñüà";
// DO NOT CHANGE ============================================================================================================================================
WCHAR GsmPage1[] = L"??????????\n??\r??????^??????\x1b????????????{}?????\\????????????[~]?|????????????????????????????????????€??????????????????????????";
// DO NOT CHANGE ============================================================================================================================================

void DecodeGsmSeptet(BYTE code, LPWSTR* page, std::wstring* decoded)
{
	if (code == 0x1B)
	{
		*page = GsmPage1;
		return;
	}

	decoded->push_back((*page)[code]);

	*page = GsmPage0;
}

#define ENDIFNECESSARY if (it == end) return FALSE

BOOL DecodeGsmSeptetData(std::vector<BYTE>::iterator it, std::vector<BYTE>::iterator end, int chars, int skip, std::wstring* decoded)
{
	*decoded = std::wstring();

	BYTE c = 0;

	LPWSTR page = GsmPage0;

	int num = 0;

	int cchar = 0;

	while (cchar < skip)
	{
		if ((num % 7) == 0 && c > 0)
		{
			c = 0;
			cchar++;
			continue;
		}

		ENDIFNECESSARY;

		c = (*it++) >> (7 - (num++ % 7));

		cchar++;
	}

	while (cchar < chars)
	{
		if ((num % 7) == 0 && c > 0)
		{
			DecodeGsmSeptet(c & 0x7F, &page, decoded);
			c = 0;
			cchar++;
			continue;
		}

		ENDIFNECESSARY;

		DecodeGsmSeptet((c | ((*it) << (num % 7))) & 0x7F, &page, decoded);

		c = (*it++) >> (7 - (num++ % 7));

		cchar++;
	}

	return TRUE;
}

void DecodeHexToBin(std::wstring& data, std::vector<BYTE>* decoded)
{
	*decoded = std::vector<BYTE>();

	WCHAR buff[] = L"\0\0";

	for (auto it = data.begin(); it != data.end(); it++)
	{
		buff[0] = *(it++);

		if (it == data.end())
		{
			buff[1] = L'\0';
			decoded->push_back((BYTE)wcstoul(buff, NULL, 16));
			break;
		}
		else
		{
			buff[1] = *it;
		}

		decoded->push_back((BYTE)wcstoul(buff, NULL, 16));
	}
}

#define ENDIFNECESSARY2 if (it == value.end()) return FALSE

BOOL ParseDateTimeNumber(std::wstring::iterator& it, std::wstring::iterator end, PINT value)
{
	ENDIFNECESSARY;

	if (!(bool)iswdigit(*it))
	{
		return FALSE;
	}

	auto itx = it++;

	ENDIFNECESSARY;

	if (!(bool)iswdigit(*it))
	{
		return FALSE;
	}

	*value = ((*itx - L'0') * 10) + (*it - L'0');

	return TRUE;
}

BOOL ParseGsmDateTime(std::wstring& value, std::wstring* datetime)
{
	std::time_t t = std::time(NULL);
	std::tm tx;
	if (localtime_s(&tx, &t) != 0)
	{
		return FALSE;
	}

	int currentyear = 1900 + tx.tm_year;

	auto it = value.begin();

	int year;
	if (!ParseDateTimeNumber(it, value.end(), &year))
	{
		return FALSE;
	}

	year += (currentyear / 100) * 100;

	if (year > currentyear)
	{
		year -= 100;
	}

	it++;

	int month;
	if (!ParseDateTimeNumber(it, value.end(), &month))
	{
		return FALSE;
	}

	if (month < 1 || month > 12)
	{
		return FALSE;
	}

	it++;

	int day;
	if (!ParseDateTimeNumber(it, value.end(), &day))
	{
		return FALSE;
	}

	if (day < 1 || day > 31)
	{
		return FALSE;
	}

	it++;

	int hour;
	if (!ParseDateTimeNumber(it, value.end(), &hour))
	{
		return FALSE;
	}

	if (hour < 0 || hour > 23)
	{
		return FALSE;
	}

	it++;

	int minute;
	if (!ParseDateTimeNumber(it, value.end(), &minute))
	{
		return FALSE;
	}

	if (minute < 0 || minute > 59)
	{
		return FALSE;
	}

	it++;

	int second;
	if (!ParseDateTimeNumber(it, value.end(), &second))
	{
		return FALSE;
	}

	if (second < 0 || second > 59)
	{
		return FALSE;
	}

	it++;

	ENDIFNECESSARY2;

	int tz = *it - L'0';

	it++;

	ENDIFNECESSARY2;

	int tz1 = *it - L'0';

	tz = (((tz1 & 0x7F) + tz) * 15) / 60;

	if (tz1 & 0x80)
	{
		tz *= -1;
	}

	*datetime = FormatStr(L"%i-%02i-%02iT%02i:%02i:%02i%+02i:00", year, month, day, hour, minute, second, tz);

	return TRUE;
}

BOOL GetMessageIndexFromListing(std::wstring& value, PINT index)
{
	auto it = value.begin();

	ENDIFNECESSARY2;

	while (*it++ != ' ')
	{
		ENDIFNECESSARY2;
	}

	ENDIFNECESSARY2;

	int num = 0;

	while (*it >= L'0' && *it <= L'9')
	{
		num *= 10;
		num += ((*it++) - L'0');

		ENDIFNECESSARY2;
	}

	*index = num;

	return TRUE;
}

#define ENDIFNECESSARY3 if (it == buffer.end()) return FALSE

BOOL ParseGsmPDU(std::wstring& pdu, std::wstring* from, std::wstring* datetime, std::wstring* message)
{
	std::vector<BYTE> buffer;
	DecodeHexToBin(pdu, &buffer);

	auto it = buffer.begin();

	ENDIFNECESSARY3;

	int num = *it++;

	while (num-- > 0)
	{
		ENDIFNECESSARY3;

		it++;
	}

	ENDIFNECESSARY3;

	auto flags = *it++;

	ENDIFNECESSARY3;

	int senderNum = *it++;

	ENDIFNECESSARY3;

	int numberType = *it++;

	num = senderNum + (senderNum % 2);

	std::wstring number;

	for (int i = 0; i < num; i += 2, it++)
	{
		ENDIFNECESSARY3;

		number.push_back(L'0' + ((*it) & 0xF));
		number.push_back(L'0' + (((*it) >> 4) & 0xF));
	}

	if (number.size() < senderNum)
	{
		return FALSE;
	}

	// assume from is empty
	from->append(number.c_str(), senderNum);

	ENDIFNECESSARY3;

	auto proto = *it++;

	ENDIFNECESSARY3;

	auto scheme = *it++;

	ENDIFNECESSARY3;

	std::wstring timestamp;

	for (int i = 0; i < 7; i++, it++)
	{
		ENDIFNECESSARY3;

		timestamp.push_back(L'0' + ((*it) & 0xF));
		timestamp.push_back(L'0' + (((*it) >> 4) & 0xF));
	}

	if (!ParseGsmDateTime(timestamp, datetime))
	{
		return FALSE;
	}

	// VALIDITY INFO IF PRESENT
	if (flags & 0x10)
	{
		if (flags & 0x08)
		{
			num = 7;
		}
		else
		{
			num = 1;
		}

		while (num-- > 0)
		{
			ENDIFNECESSARY3;

			it++;
		}
	}

	ENDIFNECESSARY3;

	int len = *it++;

	num = 0;

	if (scheme & 0x8)
	{
		// UCS-2
		// HAS USER DATA HEADER
		if (flags & 0x40)
		{
			ENDIFNECESSARY3;

			// LENGTH OF HEADER
			num = *it++;

			while (num-- > 0)
			{
				ENDIFNECESSARY3;

				it++;
			}
		}

		std::string temp;

		for (int i = 0; i < len; i++)
		{
			ENDIFNECESSARY3;

			temp.push_back(*it++);
		}

		*message = std::wstring((LPWSTR)temp.c_str(), (LPWSTR)temp.c_str() + (temp.length() / sizeof(wchar_t)));
	}
	else if (scheme & 0x4)
	{
		// Binary Message
		return FALSE;
	}
	else
	{
		// 7 Bit
		// HAS USER DATA HEADER
		if (flags & 0x40)
		{
			// LENGTH OF HEADER
			num = *it;

			num = MulDiv(num + 1, 8, 7);
		}

		DecodeGsmSeptetData(it, buffer.end(), len, num, message);
	}

	return TRUE;
}