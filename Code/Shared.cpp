// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Shared.h"

Utf8String PlatformStringToUtf8(const PlatformString& str)
{
	return ConvertMultiByte<PlatformString, Utf8String>(str, std::wcsrtombs);
}

PlatformString Utf8ToPlatformString(const Utf8String& str)
{
	return ConvertMultiByte<Utf8String, PlatformString>(str, std::mbsrtowcs);
}

void ParseArguments(const std::vector<PlatformString>& args, std::map<PlatformString, PlatformString, PlatformCIComparer>& parsed)
{
	auto it = parsed.end();

	for (auto item : args)
	{
		if (item.size() > 0)
		{
			if (item[0] == PLATFORMSTR('-') || item[0] == PLATFORMSTR('/'))
			{
				it = parsed.insert_or_assign(item.substr(1), PlatformString()).first;
			}
			else if (it != parsed.end())
			{
				it->second = item;
			}
		}
	}
}

bool ValidateArguments(const std::map<PlatformString, PlatformString, PlatformCIComparer>& parsed, const std::vector<PlatformString>& required)
{
	for (auto it : required)
	{
		auto find = parsed.find(it);

		if (find == parsed.end())
		{
			return false;
		}

		if (find->second.size() == 0)
		{
			return false;
		}
	}

	return true;
}

struct upload_status {
	Utf8String* data;
	size_t bytes_read;
};

#define CANCELEMAILIFNECESSARY if (res != CURLE_OK) goto CLEANUP

bool SendEmail(const PlatformString& subject, const PlatformString& message, const PlatformString& smtpusername, const PlatformString& smtppassword, const PlatformString& smtpserver, const PlatformString& smtpfromto)
{
	CURL* curl;
	CURLcode res = CURLE_FAILED_INIT;
	curl_slist* recipients = NULL;
	upload_status upload_ctx = { 0 };
	PlatformStream strm;
	std::time_t t = std::time(nullptr);
	std::tm* tx = localtime(&t);

	strm << PLATFORMSTR("Date: ") << std::put_time(tx, L"%a, %d %b %Y %T %z") << PLATFORMSTR("\r\n")
		<< PLATFORMSTR("To: ") << smtpfromto << PLATFORMSTR("\r\n")
		<< PLATFORMSTR("From: ") << smtpfromto << PLATFORMSTR("\r\n")
		<< PLATFORMSTR("Subject: ") << subject << PLATFORMSTR("\r\n")
		<< PLATFORMSTR("Content-Type: text/plain; charset=utf-8\r\n")
		<< PLATFORMSTR("\r\n") << message << PLATFORMSTR("\r\n");

	Utf8String msg(PlatformStringToUtf8(strm.str()));

	upload_ctx.data = &msg;

	curl = curl_easy_init();

	if (!curl)
	{
		return false;
	}
		
	res = curl_easy_setopt(curl, CURLOPT_USERNAME, PlatformStringToUtf8(smtpusername).c_str());

	CANCELEMAILIFNECESSARY;

	res = curl_easy_setopt(curl, CURLOPT_PASSWORD, PlatformStringToUtf8(smtppassword).c_str());

	CANCELEMAILIFNECESSARY;

	res = curl_easy_setopt(curl, CURLOPT_URL, PlatformStringToUtf8(PlatformString(PLATFORMSTR("smtp://")).append(smtpserver).append(PLATFORMSTR(":587"))).c_str());

	CANCELEMAILIFNECESSARY;

	res = curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

	CANCELEMAILIFNECESSARY;

	res = curl_easy_setopt(curl, CURLOPT_MAIL_FROM, PlatformStringToUtf8(smtpfromto).c_str());

	CANCELEMAILIFNECESSARY;

	recipients = curl_slist_append(recipients, PlatformStringToUtf8(smtpfromto).c_str());

	res = curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

	CANCELEMAILIFNECESSARY;

	res = curl_easy_setopt(curl, CURLOPT_READFUNCTION, +[](char* ptr, size_t size, size_t nmemb, void* userp)->size_t
		{ {
				struct upload_status* upload_ctx = (struct upload_status*)userp;

				size_t room = size * nmemb;

				if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1))
				{
					return 0;
				}

				auto data = upload_ctx->data->c_str() + upload_ctx->bytes_read;

				if (data)
				{
					size_t len = upload_ctx->data->size() - upload_ctx->bytes_read;

					if (len > 0)
					{
						if (room < len)
						{
							len = room;
						}

						std::memcpy(ptr, data, len);

						upload_ctx->bytes_read += len;

						return len;
					}
				}

				return 0;
			}});

	CANCELEMAILIFNECESSARY;

	res = curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);

	CANCELEMAILIFNECESSARY;

	res = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	CANCELEMAILIFNECESSARY;

	res = curl_easy_perform(curl);

CLEANUP:;

	if (recipients)
	{
		curl_slist_free_all(recipients);
	}

	curl_easy_cleanup(curl);

	if (res != CURLE_OK)
	{
		std::cout << curl_easy_strerror(res) << std::endl;
	}

	return res == CURLE_OK;
}