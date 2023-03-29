// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma comment (lib, "SetupAPI.lib")
#pragma comment (lib, "Crypt32.lib")

#include "Shared.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <Ntddser.h>

int wmain(int argc, wchar_t* argv[])
{
	return MainLoop(argc, argv);
}

void CALLBACK TimerCallback(HWND, UINT, UINT_PTR, DWORD)
{
	CheckHardwareID(0x1A86, 0x7523);
}

void CheckHardwareID(std::uint32_t vid, std::uint32_t pid)
{
	std::wstring format = FormatStr(L"USB\\VID_%X&PID_%X", vid, pid);

	LPWSTR str = new WCHAR[MAX_PATH];

	HDEVINFO devInfo = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR, NULL, NULL, DIGCF_PRESENT);

	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (DWORD dwDeviceIndex = 0; SetupDiEnumDeviceInfo(devInfo, dwDeviceIndex, &devInfoData); dwDeviceIndex++)
	{
		DWORD size = MAX_PATH;
		if (SetupDiGetDeviceInstanceIdW(devInfo, &devInfoData, str, size, &size))
		{
			if (std::wstring(str).find(format) == 0)
			{
				HKEY reg = SetupDiOpenDevRegKey(devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

				size = MAX_PATH * sizeof(WCHAR);
				if (RegGetValueW(reg, NULL, L"PortName", RRF_RT_REG_SZ, NULL, str, &size) == ERROR_SUCCESS)
				{
					EnsureCommPort(str);
				}

				RegCloseKey(reg);
			}
		}
	}

	if (devInfo)
	{
		SetupDiDestroyDeviceInfoList(devInfo);
	}

	delete[] str;
}

Utf8String PlatformStringToUtf8(const PlatformString& str)
{
	auto needed = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0, NULL, NULL);

	Utf8String cvt(needed, 0);

	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), (Utf8Char*)cvt.c_str(), (int)cvt.size(), NULL, NULL);

	return cvt;
}

PlatformString Utf8ToPlatformString(const Utf8String& str)
{
	auto needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);

	PlatformString cvt(needed, 0);

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), (PlatformChar*)cvt.c_str(), (int)cvt.size());

	return cvt;
}