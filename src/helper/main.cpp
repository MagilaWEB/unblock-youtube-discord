#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <curl/curl.h>
#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <chrono>
#include <format>
#include <cstring>
#include <cstdlib>
#pragma comment(lib, "ws2_32.lib")

struct Context
{
	std::mutex mtx;
	std::queue<std::string> queue;
	std::unordered_set<std::string> active;
	SOCKET sock{ INVALID_SOCKET };
	bool running{ true };
};

static void udp_send(SOCKET sock, const std::string& msg, int port)
{
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sendto(sock, msg.c_str(), static_cast<int>(msg.size()), 0, (sockaddr*)&addr, sizeof(addr));
}

static void log(Context& ctx, const std::string& text)
{
	udp_send(ctx.sock, std::format("LOG:INFO:helper:{}", text), 9999);
}

static size_t write_cb(void*, size_t size, size_t nmemb, void*) { return size * nmemb; }

static void check_host(Context& ctx, const std::string& host)
{
	log(ctx, std::format("check {}", host));
	udp_send(ctx.sock, "CHECK:" + host, 10000);

	CURL* curl = curl_easy_init();
	if (!curl) return;

	std::string url = "https://" + host;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
	headers = curl_slist_append(headers, "Accept-Language: ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7");
	headers = curl_slist_append(headers, "Sec-Fetch-Dest: document");
	headers = curl_slist_append(headers, "Sec-Fetch-Mode: navigate");
	headers = curl_slist_append(headers, "Sec-Fetch-Site: none");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		log(ctx, std::format("fail {} curl={}", host, static_cast<int>(res)));
		udp_send(ctx.sock, "FAIL:" + host, 10000);
	}
	else
	{
		long code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		log(ctx, std::format("ok {} http={}", host, code));
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
}

static void worker_thread(Context& ctx)
{
	while (ctx.running)
	{
		std::string host;
		{
			std::lock_guard lock(ctx.mtx);
			if (!ctx.queue.empty())
			{
				host = ctx.queue.front();
				ctx.queue.pop();
				ctx.active.insert(host);
			}
		}

		if (!host.empty())
		{
			check_host(ctx, host);
			{
				std::lock_guard lock(ctx.mtx);
				ctx.active.erase(host);
			}
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}
}

int main()
{
	WSADATA wsa{};
	WSAStartup(MAKEWORD(2, 2), &wsa);

	Context ctx;

	ctx.sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctx.sock == INVALID_SOCKET) { WSACleanup(); return 1; }

	sockaddr_in server{};
	server.sin_family = AF_INET;
	server.sin_port = htons(10000);
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(ctx.sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) { closesocket(ctx.sock); WSACleanup(); return 1; }

	u_long nonblock = 1;
	ioctlsocket(ctx.sock, FIONBIO, &nonblock);

	std::thread worker(worker_thread, std::ref(ctx));

	char buf[2048];
	sockaddr_in from{};
	int fromlen = sizeof(from);

	while (ctx.running)
	{
		int n = recvfrom(ctx.sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromlen);
		if (n <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
		buf[n] = 0;
		std::string msg(buf);

		if (msg.find("LIST:") == 0)
		{
			auto rest = msg.substr(5);
			std::lock_guard lock(ctx.mtx);
			size_t pos = 0;
			while ((pos = rest.find(':')) != std::string::npos)
			{
				auto host = rest.substr(0, pos);
				if (!host.empty() && !ctx.active.contains(host)) ctx.queue.push(host);
				rest.erase(0, pos + 1);
			}
			if (!rest.empty() && !ctx.active.contains(rest)) ctx.queue.push(rest);
			log(ctx, std::format("list added {} hosts", ctx.queue.size()));
		}
		else if (msg.find("CHECK:") == 0)
		{
			auto host = msg.substr(6);
			std::lock_guard lock(ctx.mtx);
			if (!ctx.active.contains(host))
				ctx.queue.push(host);
		}
	}

	ctx.running = false;
	worker.join();
	closesocket(ctx.sock);
	WSACleanup();
	return 0;
}
