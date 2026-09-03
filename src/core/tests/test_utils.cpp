#include <catch2/catch_test_macros.hpp>
#include "../pch.h"
#include "../utils.h"

TEST_CASE("utils::IsUTF8 empty string", "[utils][utf8]")
{
	CHECK(utils::IsUTF8(""));
}

TEST_CASE("utils::IsUTF8 plain ASCII", "[utils][utf8]")
{
	CHECK(utils::IsUTF8("Hello, World!"));
}

TEST_CASE("utils::IsUTF8 Russian text", "[utils][utf8]")
{
	CHECK(utils::IsUTF8("Привет, мир!"));
}

TEST_CASE("utils::IsUTF8 2-byte sequence", "[utils][utf8]")
{
	CHECK(utils::IsUTF8("\xC3\xA9"));
}

TEST_CASE("utils::IsUTF8 3-byte sequence", "[utils][utf8]")
{
	CHECK(utils::IsUTF8("\xE0\xA0\x80"));
}

TEST_CASE("utils::IsUTF8 4-byte sequence", "[utils][utf8]")
{
	CHECK(utils::IsUTF8("\xF0\x9F\x98\x80"));
}

TEST_CASE("utils::IsUTF8 invalid stray continuation byte", "[utils][utf8]")
{
	CHECK_FALSE(utils::IsUTF8("\x80"));
}

TEST_CASE("utils::IsUTF8 invalid missing continuation", "[utils][utf8]")
{
	CHECK_FALSE(utils::IsUTF8("\xC3"));
}

TEST_CASE("utils::IsUTF8 invalid 0xFE byte", "[utils][utf8]")
{
	CHECK_FALSE(utils::IsUTF8("\xFE"));
}

TEST_CASE("utils::IsUTF8 mixed valid and invalid", "[utils][utf8]")
{
	CHECK_FALSE(utils::IsUTF8("abc\x80xyz"));
}

TEST_CASE("utils::UTF8_to_CP1251 ASCII passthrough", "[utils][cp1251]")
{
	CHECK(utils::UTF8_to_CP1251("Hello") == "Hello");
}

TEST_CASE("utils::UTF8_to_CP1251 Russian", "[utils][cp1251]")
{
	auto result = utils::UTF8_to_CP1251("Привет");
	CHECK_FALSE(result.empty());
	CHECK_FALSE(result == "Привет");
}

TEST_CASE("utils::UTF8_to_CP1251 non-UTF8 returns input", "[utils][cp1251]")
{
	CHECK(utils::UTF8_to_CP1251("\x80\x81\x82") == "\x80\x81\x82");
}

TEST_CASE("utils::UTF8_to_UTF16 empty", "[utils][utf16]")
{
	CHECK(utils::UTF8_to_UTF16("").empty());
}

TEST_CASE("utils::UTF8_to_UTF16 ASCII", "[utils][utf16]")
{
	auto result = utils::UTF8_to_UTF16("Hello");
	CHECK(result == L"Hello");
}

TEST_CASE("utils::UTF8_to_UTF16 Russian", "[utils][utf16]")
{
	auto result = utils::UTF8_to_UTF16("Привет");
	CHECK_FALSE(result.empty());
	CHECK(result.size() == 6);
}

TEST_CASE("utils::ltrim removes leading spaces", "[utils][trim]")
{
	std::string s = "  hello";
	utils::ltrim(s);
	CHECK(s == "hello");
}

TEST_CASE("utils::ltrim removes leading tabs", "[utils][trim]")
{
	std::string s = "\t\thello";
	utils::ltrim(s);
	CHECK(s == "hello");
}

TEST_CASE("utils::ltrim no-op when no leading whitespace", "[utils][trim]")
{
	std::string s = "hello";
	utils::ltrim(s);
	CHECK(s == "hello");
}

TEST_CASE("utils::rtrim removes trailing spaces", "[utils][trim]")
{
	std::string s = "hello  ";
	utils::rtrim(s);
	CHECK(s == "hello");
}

TEST_CASE("utils::rtrim removes trailing tabs", "[utils][trim]")
{
	std::string s = "hello\t\t";
	utils::rtrim(s);
	CHECK(s == "hello");
}

TEST_CASE("utils::rtrim no-op when no trailing whitespace", "[utils][trim]")
{
	std::string s = "hello";
	utils::rtrim(s);
	CHECK(s == "hello");
}

TEST_CASE("utils::trim removes both sides", "[utils][trim]")
{
	std::string s = "  hello  ";
	utils::trim(s);
	CHECK(s == "hello");
}

TEST_CASE("utils::trim all whitespace", "[utils][trim]")
{
	std::string s = "   \t   ";
	utils::trim(s);
	CHECK(s.empty());
}

TEST_CASE("utils::trim empty string", "[utils][trim]")
{
	std::string s;
	utils::trim(s);
	CHECK(s.empty());
}

TEST_CASE("utils::isValidHost valid domains", "[utils][host]")
{
	CHECK(utils::isValidHost("example.com"));
	CHECK(utils::isValidHost("www.example.com"));
	CHECK(utils::isValidHost("sub.domain.example.com"));
	CHECK(utils::isValidHost("a"));
	CHECK(utils::isValidHost("xn--80aswg.xn--p1ai"));
	CHECK(utils::isValidHost("example.com:8080"));
	CHECK(utils::isValidHost("1.2.3.4"));
}

TEST_CASE("utils::isValidHost invalid values", "[utils][host]")
{
	CHECK_FALSE(utils::isValidHost(""));
	CHECK_FALSE(utils::isValidHost(" example.com"));
	CHECK_FALSE(utils::isValidHost("example.com "));
	CHECK_FALSE(utils::isValidHost("-example.com"));
	CHECK_FALSE(utils::isValidHost("example.com:abc"));
	CHECK_FALSE(utils::isValidHost("example.com:123456"));
	CHECK_FALSE(utils::isValidHost("exa mple.com"));
}

TEST_CASE("utils::isValidNetwork valid IPv4", "[utils][network]")
{
	CHECK(utils::isValidNetwork("1.2.3.4"));
	CHECK(utils::isValidNetwork("255.255.255.255"));
	CHECK(utils::isValidNetwork("1.2.3.0/24"));
	CHECK(utils::isValidNetwork("0.0.0.0/0"));
	CHECK(utils::isValidNetwork("192.168.0.0/16"));
}

TEST_CASE("utils::isValidNetwork invalid IPv4", "[utils][network]")
{
	CHECK_FALSE(utils::isValidNetwork(""));
	CHECK_FALSE(utils::isValidNetwork("1.2.3"));
	CHECK_FALSE(utils::isValidNetwork("1.2.3.4.5"));
	CHECK_FALSE(utils::isValidNetwork("1.2.3.256"));
	CHECK_FALSE(utils::isValidNetwork("1.2.3.4/33"));
	CHECK_FALSE(utils::isValidNetwork("1.2.3.4/"));
	CHECK_FALSE(utils::isValidNetwork("a.b.c.d"));
}

TEST_CASE("utils::isValidNetwork valid IPv6", "[utils][network]")
{
	CHECK(utils::isValidNetwork("::1"));
	CHECK(utils::isValidNetwork("::"));
	CHECK(utils::isValidNetwork("2001:db8::1"));
	CHECK(utils::isValidNetwork("fe80::/10"));
	CHECK(utils::isValidNetwork("2001:db8::1/128"));
	CHECK(utils::isValidNetwork("1:2:3:4:5:6:7:8"));
}

TEST_CASE("utils::isValidNetwork invalid IPv6", "[utils][network]")
{
	CHECK_FALSE(utils::isValidNetwork("1::2::3"));
	CHECK_FALSE(utils::isValidNetwork("2001:db8:::1"));
	CHECK_FALSE(utils::isValidNetwork("gggg::1"));
	CHECK_FALSE(utils::isValidNetwork("::1/129"));
	CHECK_FALSE(utils::isValidNetwork("1:2:3:4:5:6:7:8:9"));
	CHECK_FALSE(utils::isValidNetwork("1:2:3"));
	CHECK_FALSE(utils::isValidNetwork(":1"));
	CHECK_FALSE(utils::isValidNetwork("1:"));
}
