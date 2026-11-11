import std;
import helper.curl_client;
import helper.net;
import helper.zapret_helper;

// NOLINTNEXTLINE(bugprone-exception-escape) - helper crash is handled by the service manager restart
int main(int argc, char** argv)
{
	Winsock	   winsock;
	CurlGlobal curl_global;

	// --vpn-only: started next to Zapret1, where the helper protocol does not
	// exist at all. The helper then only watches for a VPN tunnel and pauses
	// the engines while it is up; no host pumping, no verdicts to unblock.
	bool vpn_only = false;
	for (int i = 1; i < argc; ++i)
		if (std::string_view{ argv[i] } == "--vpn-only")
			vpn_only = true;

	ZapretHelper helper;
	helper.setVpnOnly(vpn_only);

	return helper.run();
}
