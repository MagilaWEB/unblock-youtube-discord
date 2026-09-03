#include <catch2/catch_session.hpp>
#include <iostream>
#include <string>
#include <string_view>

#include "../pch.h"
#include "../debug.h"

class ColorFilter : public std::streambuf
{
	std::streambuf* saved;
	std::string		buf;

	static constexpr std::string_view green = "\x1b[32m";
	static constexpr std::string_view red	= "\x1b[31m";
	static constexpr std::string_view reset = "\x1b[0m";

public:
	explicit ColorFilter() : saved(std::cout.rdbuf(this)) {}
	~ColorFilter() override { std::cout.rdbuf(saved); }

	void emit(std::string_view s) { saved->sputn(s.data(), static_cast<std::streamsize>(s.size())); }

	void flush()
	{
		if (buf.empty())
			return;
		if (buf.starts_with("ok "))
		{
			emit(green);
			emit("[OK] ");
			emit(std::string_view(buf).substr(3));
			emit(reset);
		}
		else if (buf.starts_with("not ok "))
		{
			emit(red);
			emit("[ERROR] ");
			emit(std::string_view(buf).substr(7));
			emit(reset);
		}
		else
		{
			emit(buf);
		}
		buf.clear();
	}

	int overflow(int c) override
	{
		if (c == '\n')
		{
			flush();
			saved->sputc('\n');
		}
		else if (c != '\r')
		{
			buf += static_cast<char>(c);
		}
		return c;
	}

	int sync() override
	{
		if (!buf.empty())
			flush();
		return saved->pubsync();
	}

	std::streamsize xsputn(const char* s, std::streamsize n) override
	{
		for (std::streamsize i = 0; i < n; ++i)
		{
			if (s[i] == '\n')
			{
				flush();
				saved->sputc('\n');
			}
			else if (s[i] != '\r')
			{
				buf += s[i];
			}
		}
		return n;
	}
};

int main(int argc, char* argv[])
{
#ifdef CORE_TESTS
	// Tests must report crashes to the test runner, not show the crash dialog.
	Debug::setCrashHandlerEnabled(false);
#endif

	ColorFilter	   filter;
	Catch::Session session;
	return session.run(argc, argv);
}
