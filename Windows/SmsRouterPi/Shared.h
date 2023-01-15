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

std::wstring FormatStr(LPCWSTR format, ...)
{
	std::wstring str(wcslen(format) + 64, '0');

	va_list args;

	va_start(args, format);

	int ans = _vsnwprintf_s((LPWSTR)str.c_str(), str.size(), str.size(), format, args);

	while (ans < 0)
	{
		str.resize(str.size() + 1024);

		ans = _vsnwprintf_s((LPWSTR)str.c_str(), str.size(), str.size(), format, args);
	}

	va_end(args);

	return std::wstring(str.c_str());
}