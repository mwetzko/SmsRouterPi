// Author: Martin Wetzko
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Shared.h"

void ParseArguments(int argc, PlatformChar* argv[], std::map<PlatformString, PlatformString, PlatformCIComparer>& parsed)
{
	auto it = parsed.end();

	for (int i = 1; i < argc; i++)
	{
		auto item = PlatformString(argv[i]);

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

bool SendEmail(const PlatformString& subject, const PlatformString& message, const PlatformString& smtpusername, const PlatformString& smtppassword, const PlatformString& smtpserver, const PlatformString& smtpfromto)
{
	try
	{
		mailio::message msg;
		msg.header_codec(mailio::message::header_codec_t::BASE64);
		msg.content_transfer_encoding(mailio::mime::content_transfer_encoding_t::BASE_64);
		msg.from(mailio::mail_address(PlatformStringToUtf8(smtpfromto), PlatformStringToUtf8(smtpfromto)));
		msg.add_recipient(mailio::mail_address(PlatformStringToUtf8(smtpfromto), PlatformStringToUtf8(smtpfromto)));
		msg.subject(PlatformStringToUtf8(subject));
		msg.content_type(mailio::mime::media_type_t::TEXT, "PLAIN", "UTF-8");
		msg.content(PlatformStringToUtf8(message));

		mailio::smtps conn(PlatformStringToUtf8(smtpserver), 587);
		conn.authenticate(PlatformStringToUtf8(smtpusername), PlatformStringToUtf8(smtppassword), mailio::smtps::auth_method_t::START_TLS);
		conn.submit(msg);

		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

void ParseSubscriberNumber(const PlatformString& number, PlatformString& pnumber)
{
	bool st = false;
	bool qt = false;

	for (auto it : number)
	{
		if (it == PLATFORMSTR(','))
		{
			if (qt)
			{
				pnumber.push_back(it);
			}
			else if (st)
			{
				return;
			}
			else
			{
				st = true;
			}
		}
		else if (it == PLATFORMSTR('"'))
		{
			qt = !qt;
		}
		else if (st)
		{
			pnumber.push_back(it);
		}
	}
}