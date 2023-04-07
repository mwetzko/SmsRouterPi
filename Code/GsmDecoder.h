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

std::wregex RegMatchGsmDate = std::wregex(PLATFORMSTR("^([0-9]{2})([0-9]{2})([0-9]{2})([0-9]{2})([0-9]{2})([0-9]{2})([0-9])([0-9])"), std::wregex::icase);

void DecodeGsmSeptet(std::byte code, PlatformChar** page, PlatformString* decoded)
{
	if (code == std::byte(0x1B))
	{
		*page = GsmPage1;
		return;
	}

	decoded->push_back((*page)[(int)code]);

	*page = GsmPage0;
}

#define ENDIFNECESSARY if (it == end) return false

bool DecodeGsmSeptetData(std::vector<std::byte>::iterator it, std::vector<std::byte>::iterator end, int chars, int skip, PlatformString* decoded)
{
	*decoded = PlatformString();

	std::byte c = std::byte(0);

	PlatformChar* page = GsmPage0;

	int num = 0;

	int cchar = 0;

	while (cchar < skip)
	{
		if ((num % 7) == 0 && c > std::byte(0))
		{
			c = std::byte(0);
			cchar++;
			continue;
		}

		ENDIFNECESSARY;

		c = (*it++) >> (7 - (num++ % 7));

		cchar++;
	}

	while (cchar < chars)
	{
		if ((num % 7) == 0 && c > std::byte(0))
		{
			DecodeGsmSeptet(c & std::byte(0x7F), &page, decoded);
			c = std::byte(0);
			cchar++;
			continue;
		}

		ENDIFNECESSARY;

		DecodeGsmSeptet((c | ((*it) << (num % 7))) & std::byte(0x7F), &page, decoded);

		c = (*it++) >> (7 - (num++ % 7));

		cchar++;
	}

	return true;
}

void DecodeHexToBin(const PlatformString& data, std::vector<std::byte>* decoded)
{
	*decoded = std::vector<std::byte>();

	PlatformChar buff[] = PLATFORMSTR("\0\0");

	for (auto it = data.begin(); it != data.end(); it++)
	{
		buff[0] = *(it++);

		if (it == data.end())
		{
			buff[1] = PLATFORMSTR('\0');
			decoded->push_back(std::byte(std::stoul(buff, nullptr, 16)));
			break;
		}
		else
		{
			buff[1] = *it;
		}

		decoded->push_back(std::byte(std::stoul(buff, nullptr, 16)));
	}
}

bool ParseGsmDateTime(PlatformString& value, PlatformString* datetime)
{
	std::wsmatch match;
	if (!std::regex_search(value, match, RegMatchGsmDate))
	{
		return false;
	}

	std::time_t t = std::time(nullptr);
	std::tm* tx = localtime(&t);

	int currentyear = 1900 + tx->tm_year;

	int year = std::stoi(match.str(1));

	year += (currentyear / 100) * 100;

	if (year > currentyear)
	{
		year -= 100;
	}

	int month = std::stoi(match.str(2));

	if (month < 1 || month > 12)
	{
		return false;
	}

	int day = std::stoi(match.str(3));

	if (day < 1 || day > 31)
	{
		return false;
	}

	int hour = std::stoi(match.str(4));

	if (hour < 0 || hour > 23)
	{
		return false;
	}

	int minute = std::stoi(match.str(5));

	if (minute < 0 || minute > 59)
	{
		return false;
	}

	int second = std::stoi(match.str(6));

	if (second < 0 || second > 59)
	{
		return false;
	}

	int tz = std::stoi(match.str(7));
	int tz1 = std::stoi(match.str(8));

	tz = (((tz1 & 0x7F) + tz) * 15) / 60;

	if (tz1 & 0x80)
	{
		tz *= -1;
	}

	*datetime = FormatStr(PLATFORMSTR("%i-%02i-%02iT%02i:%02i:%02i%+02i:00"), year, month, day, hour, minute, second, tz);

	return true;
}

#define ENDIFNECESSARY3 if (it == buffer.end()) return false

bool ParseGsmPDU(const PlatformString& pdu, PlatformString* from, PlatformString* datetime, PlatformString* message)
{
	std::vector<std::byte> buffer;
	DecodeHexToBin(pdu, &buffer);

	auto it = buffer.begin();

	ENDIFNECESSARY3;

	int num = (int)(*it++);

	while (num-- > 0)
	{
		ENDIFNECESSARY3;

		it++;
	}

	ENDIFNECESSARY3;

	int flags = (int)(*it++);

	ENDIFNECESSARY3;

	int senderNum = (int)(*it++);

	ENDIFNECESSARY3;

	/* int numberType = * */ it++;

	num = senderNum + (senderNum % 2);

	PlatformString number;

	for (int i = 0; i < num; i += 2, it++)
	{
		ENDIFNECESSARY3;

		number.push_back(PLATFORMSTR('0') + (PlatformChar)((*it) & std::byte(0xF)));
		number.push_back(PLATFORMSTR('0') + (PlatformChar)(((*it) >> 4) & std::byte(0xF)));
	}

	if ((int)number.size() < senderNum)
	{
		return false;
	}

	// assume from is empty
	from->append(number.c_str(), senderNum);

	ENDIFNECESSARY3;

	/* int proto = * */ it++;

	ENDIFNECESSARY3;

	int scheme = (int)(*it++);

	ENDIFNECESSARY3;

	PlatformString timestamp;

	for (int i = 0; i < 7; i++, it++)
	{
		ENDIFNECESSARY3;

		timestamp.push_back(PLATFORMSTR('0') + (PlatformChar)((*it) & std::byte(0xF)));
		timestamp.push_back(PLATFORMSTR('0') + (PlatformChar)(((*it) >> 4) & std::byte(0xF)));
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

	int len = (int)(*it++);

	num = 0;

	if (scheme & 0x8)
	{
		// UCS-2
		// HAS USER DATA HEADER
		if (flags & 0x40)
		{
			ENDIFNECESSARY3;

			// LENGTH OF HEADER
			num = (int)(*it++);

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

			temp.push_back((std::string::value_type)(*it++));
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
			num = (int)(*it);

			num = MulDiv(num + 1, 8, 7);
		}

		DecodeGsmSeptetData(it, buffer.end(), len, num, message);
	}

	return true;
}