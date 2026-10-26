import std;
import helper.curl_client;
import helper.net;
import helper.zapret_helper;

// NOLINTNEXTLINE(bugprone-exception-escape) - helper crash is handled by the service manager restart
int main()
{
	Winsock	   winsock;
	CurlGlobal curl_global;

	ZapretHelper helper;
	return helper.run();
}
