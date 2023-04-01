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
PlatformChar GsmPage0[] = PLATFORMSTR("@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞ\x1bÆæßÉ !\"#¤%&'()*+,-./0123456789:;<=>?¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§¿abcdefghijklmnopqrstuvwxyzäöñüà");
// DO NOT CHANGE ============================================================================================================================================
PlatformChar GsmPage1[] = PLATFORMSTR("??????????\n??\r??????^??????\x1b????????????{}?????\\????????????[~]?|????????????????????????????????????€??????????????????????????");
// DO NOT CHANGE ============================================================================================================================================

void DecodeGsmSeptet(BYTE code, PlatformChar** page, PlatformString* decoded)
{
	if (code == 0x1B)
	{
		*page = GsmPage1;
		return;
	}

	decoded->push_back((*page)[code]);

	*page = GsmPage0;
}

#define ENDIFNECESSARY if (it == end) return false

bool DecodeGsmSeptetData(std::vector<BYTE>::iterator it, std::vector<BYTE>::iterator end, int chars, int skip, PlatformString* decoded)
{
	*decoded = PlatformString();

	BYTE c = 0;

	PlatformChar* page = GsmPage0;

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

	return true;
}

void DecodeHexToBin(PlatformString& data, std::vector<BYTE>* decoded)
{
	*decoded = std::vector<BYTE>();

	PlatformChar buff[] = PLATFORMSTR("\0\0");

	for (auto it = data.begin(); it != data.end(); it++)
	{
		buff[0] = *(it++);

		if (it == data.end())
		{
			buff[1] = PLATFORMSTR('\0');
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

#define ENDIFNECESSARY2 if (it == value.end()) return false

bool ParseDateTimeNumber(PlatformString::iterator& it, PlatformString::iterator end, PINT value)
{
	ENDIFNECESSARY;

	if (!(bool)iswdigit(*it))
	{
		return false;
	}

	auto itx = it++;

	ENDIFNECESSARY;

	if (!(bool)iswdigit(*it))
	{
		return false;
	}

	*value = ((*itx - PLATFORMSTR('0')) * 10) + (*it - PLATFORMSTR('0'));

	return true;
}

bool ParseGsmDateTime(PlatformString& value, PlatformString* datetime)
{
	std::time_t t = std::time(NULL);
	std::tm tx;
	if (localtime_s(&tx, &t) != 0)
	{
		return false;
	}

	int currentyear = 1900 + tx.tm_year;

	auto it = value.begin();

	int year;
	if (!ParseDateTimeNumber(it, value.end(), &year))
	{
		return false;
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
		return false;
	}

	if (month < 1 || month > 12)
	{
		return false;
	}

	it++;

	int day;
	if (!ParseDateTimeNumber(it, value.end(), &day))
	{
		return false;
	}

	if (day < 1 || day > 31)
	{
		return false;
	}

	it++;

	int hour;
	if (!ParseDateTimeNumber(it, value.end(), &hour))
	{
		return false;
	}

	if (hour < 0 || hour > 23)
	{
		return false;
	}

	it++;

	int minute;
	if (!ParseDateTimeNumber(it, value.end(), &minute))
	{
		return false;
	}

	if (minute < 0 || minute > 59)
	{
		return false;
	}

	it++;

	int second;
	if (!ParseDateTimeNumber(it, value.end(), &second))
	{
		return false;
	}

	if (second < 0 || second > 59)
	{
		return false;
	}

	it++;

	ENDIFNECESSARY2;

	int tz = *it - PLATFORMSTR('0');

	it++;

	ENDIFNECESSARY2;

	int tz1 = *it - PLATFORMSTR('0');

	tz = (((tz1 & 0x7F) + tz) * 15) / 60;

	if (tz1 & 0x80)
	{
		tz *= -1;
	}

	*datetime = FormatStr(PLATFORMSTR("%i-%02i-%02iT%02i:%02i:%02i%+02i:00"), year, month, day, hour, minute, second, tz);

	return true;
}

bool GetMessageIndexFromListing(PlatformString& value, PINT index)
{
	auto it = value.begin();

	ENDIFNECESSARY2;

	while (*it++ != ' ')
	{
		ENDIFNECESSARY2;
	}

	ENDIFNECESSARY2;

	int num = 0;

	while (*it >= PLATFORMSTR('0') && *it <= PLATFORMSTR('9'))
	{
		num *= 10;
		num += ((*it++) - PLATFORMSTR('0'));

		ENDIFNECESSARY2;
	}

	*index = num;

	return true;
}

#define ENDIFNECESSARY3 if (it == buffer.end()) return false

bool ParseGsmPDU(PlatformString& pdu, PlatformString* from, PlatformString* datetime, PlatformString* message)
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

	PlatformString number;

	for (int i = 0; i < num; i += 2, it++)
	{
		ENDIFNECESSARY3;

		number.push_back(PLATFORMSTR('0') + ((*it) & 0xF));
		number.push_back(PLATFORMSTR('0') + (((*it) >> 4) & 0xF));
	}

	if (number.size() < senderNum)
	{
		return false;
	}

	// assume from is empty
	from->append(number.c_str(), senderNum);

	ENDIFNECESSARY3;

	auto proto = *it++;

	ENDIFNECESSARY3;

	auto scheme = *it++;

	ENDIFNECESSARY3;

	PlatformString timestamp;

	for (int i = 0; i < 7; i++, it++)
	{
		ENDIFNECESSARY3;

		timestamp.push_back(PLATFORMSTR('0') + ((*it) & 0xF));
		timestamp.push_back(PLATFORMSTR('0') + (((*it) >> 4) & 0xF));
	}

	if (!ParseGsmDateTime(timestamp, datetime))
	{
		return false;
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

		*message = UCS2ToPlatformString(std::u16string((std::u16string::value_type*)temp.c_str(), (std::u16string::value_type*)temp.c_str() + (temp.length() / sizeof(std::u16string::value_type))));
	}
	else if (scheme & 0x4)
	{
		// Binary Message
		return false;
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

	return true;
}