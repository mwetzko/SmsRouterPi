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
#include <vector>
#include "Shared.h"

// DO NOT CHANGE ============================================================================================================================================
WCHAR GsmPage0[] = L"@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞ\x1bÆæßÉ !\"#¤%&'()*+,-./0123456789:;<=>?¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§¿abcdefghijklmnopqrstuvwxyzäöñüà";
// DO NOT CHANGE ============================================================================================================================================
WCHAR GsmPage1[] = L"??????????\n??\r??????^??????\x1b????????????{}?????\\????????????[~]?|????????????????????????????????????€??????????????????????????";
// DO NOT CHANGE ============================================================================================================================================

void DecodeGsmChar(BYTE code, LPWSTR* page, std::wstring* decoded)
{
	if (code == 0x1B)
	{
		*page = GsmPage1;
		return;
	}

	if (*page != GsmPage0)
	{
		*page = GsmPage0;
	}

	decoded->push_back((*page)[code]);
}

void DecodeGsm(std::vector<BYTE>& data, std::wstring* decoded)
{
	*decoded = std::wstring();

	auto it = data.begin();

	auto num = *it++;

	BYTE c = 0;

	LPWSTR page = GsmPage0;

	for (int i = 0; i < num; i++)
	{
		DecodeGsmChar((c | ((*it) << (i % 7))) & 0x7F, &page, decoded);

		c = (*it) >> (7 - (i % 7));

		if (((i + 1) % 7) == 0)
		{
			DecodeGsmChar(c & 0x7F, &page, decoded);
			c = 0;
		}

		it++;

		if (it == data.end())
		{
			break;
		}
	}
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

#define ENDIFNECESSARY if (it == end) return FALSE
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