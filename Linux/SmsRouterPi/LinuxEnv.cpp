// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Shared.h"
#include "OverlappedComm.h"
#include <codecvt>
#include <filesystem>

int main(int argc, char** argv)
{
	std::setlocale(LC_ALL, "iv.utf8");

	std::vector<PlatformString> vec;

	for (int i = 0; i < argc; i++)
	{
		vec.push_back(Utf8ToPlatformString(argv[i]));
	}

	return MainLoop(vec);
}

std::vector<PlatformString> GetPorts(int vendor, int product)
{
	std::vector<PlatformString> names;

	std::filesystem::path p("/sys/bus/usb-serial/devices");

	try
	{
		if (exists(p))
		{
			for (auto it : std::filesystem::directory_iterator(p))
			{
				try
				{
					if (is_symlink(it.symlink_status()))
					{
						std::filesystem::path symlink_points_at = read_symlink(it);
						std::filesystem::path canonical_path = std::filesystem::canonical(p / symlink_points_at);

						std::filesystem::path idVendorPath = canonical_path / ".." / ".." / "idVendor";
						std::filesystem::path idProductPath = canonical_path / ".." / ".." / "idProduct";

						Utf8String strVendor;
						Utf8String strProduct;

						if (ReadAllText(idVendorPath, strVendor) && ReadAllText(idProductPath, strProduct))
						{
							if (std::stoi(strVendor, nullptr, 16) == vendor && std::stoi(strProduct, nullptr, 16) == product)
							{
								names.push_back(Utf8ToPlatformString("/dev" / it.path().filename()));
							}
						}
					}
				}
				catch (const std::exception&)
				{
					// nothing
				}
			}
		}
	}
	catch (const std::exception&)
	{
		// nothing
	}

	return names;
}

void HandleTimer()
{
	auto ports = GetPorts(0x1A86, 0x7523);

	for (auto it : ports)
	{
		EnsureCommPort(it);
	}
}

PlatformString UCS2ToPlatformString(const std::u16string& str)
{
	// on linux wstring is double u16string and big endian instead of little endian
	std::wstring_convert<std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>, wchar_t> conv;
	return conv.from_bytes((const char*)str.c_str(), (const char*)(str.c_str() + str.size()));
}

class PlatformSerialLinux :public PlatformSerial
{
private:

private:

public:
	PlatformSerialLinux() :PlatformSerial()
	{

	}

	bool WriteLine(const Utf8String& cmd)
	{
		auto str = cmd;
		str.append("\n");

		// todo

		return true;
	}

	bool ReadLine(Utf8String* line)
	{
		// todo

		return false;
	}
};

bool GetCommDevice(const PlatformString& port, OverlappedComm* ofm)
{
	SafeFdPtr com = SafeFdPtr(open(PlatformStringToUtf8(port).c_str(), O_RDWR));

	if (com)
	{


	}

	return false;
}

uint32_t RtlEnlargedUnsignedDivide(ULARGE_INTEGER Dividend, uint32_t Divisor, uint32_t* Remainder)
{
	if (Remainder)
	{
		*Remainder = (uint32_t)(Dividend.QuadPart % Divisor);
	}

	return (uint32_t)(Dividend.QuadPart / Divisor);
}

int MulDiv(int nNumber, int nNumerator, int nDenominator)
{
	LARGE_INTEGER Result;
	int Negative;

	Negative = nNumber ^ nNumerator ^ nDenominator;

	if (nNumber < 0) nNumber *= -1;
	if (nNumerator < 0) nNumerator *= -1;
	if (nDenominator < 0) nDenominator *= -1;

	Result.QuadPart = Int32x32To64(nNumber, nNumerator) + (nDenominator / 2);

	if (nDenominator > Result.HighPart)
	{
		Result.LowPart = RtlEnlargedUnsignedDivide(*(PULARGE_INTEGER)&Result, (uint32_t)nDenominator, (uint32_t*)&Result.HighPart);

		if ((int)Result.LowPart >= 0)
		{
			return (Negative >= 0) ? (int)Result.LowPart : -(int)Result.LowPart;
		}
	}

	return -1;
}