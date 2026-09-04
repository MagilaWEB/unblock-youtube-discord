#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Include the project's precompiled header to get all types, macros, and API definitions
#include "../pch.h"

#include "../file_system.h"
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static const fs::path& testDir()
{
	static fs::path dir = fs::temp_directory_path() / "unblock_core_test";
	return dir;
}

static fs::path createFile(std::string_view name, std::string_view content)
{
	fs::create_directories(testDir());
	auto		  path = testDir() / name;
	std::ofstream ofs(path);
	ofs << content;
	return path;
}

// ─── Lifecycle ──────────────────────────────────────────────

TEST_CASE("File default state", "[file][lifecycle]")
{
	File f;
	CHECK_FALSE(f.isOpen());
	CHECK(f.empty());
	CHECK(f.lineSize() == 0);
	CHECK(f.name().empty());
}

TEST_CASE("File open and close", "[file][lifecycle]")
{
	auto path = createFile("open_close.txt", "hello\nworld\n");
	File f;
	f.open(path, "", true);
	CHECK(f.isOpen());
	CHECK_FALSE(f.empty());
	f.close();
	CHECK_FALSE(f.isOpen());
	CHECK(f.empty());
}

TEST_CASE("File destructor closes", "[file][lifecycle]")
{
	auto path = createFile("dtor.txt", "data\n");
	{
		File f;
		f.open(path, "", true);
		CHECK(f.isOpen());
	}
}

TEST_CASE("File re-open clears state", "[file][lifecycle]")
{
	auto a = createFile("reopen_a.txt", "file a\n");
	auto b = createFile("reopen_b.txt", "file b\n");

	File f;
	f.open(a, "", true);
	CHECK(f.name() == "reopen_a.txt");

	f.open(b, "", true);
	CHECK(f.name() == "reopen_b.txt");
	CHECK(f.lineSize() == 1);
}

// ─── Reading ────────────────────────────────────────────────

TEST_CASE("File reading lines", "[file][read]")
{
	auto path = createFile("lines.txt", "one\ntwo\nthree\n");
	File f;
	f.open(path, "", true);

	REQUIRE(f.isOpen());
	REQUIRE(f.lineSize() == 3);

	std::vector<std::string> lines;
	f.forLine(
		[&](auto str)
		{
			lines.push_back(str);
			return false;
		}
	);
	REQUIRE(lines.size() == 3);
	CHECK(lines[0] == "one");
	CHECK(lines[1] == "two");
	CHECK(lines[2] == "three");
}

TEST_CASE("File forLine stops early", "[file][read]")
{
	auto path = createFile("lines_stop.txt", "a\nb\nc\n");
	File f;
	f.open(path, "", true);

	std::vector<std::string> lines;
	f.forLine(
		[&](auto str)
		{
			lines.push_back(str);
			return str == "b";
		}
	);
	REQUIRE(lines.size() == 2);
	CHECK(lines[0] == "a");
	CHECK(lines[1] == "b");
}

TEST_CASE("File forLine empty file", "[file][read]")
{
	auto path = createFile("empty.txt", "");
	File f;
	f.open(path, "", true);

	int count = 0;
	f.forLine(
		[&](auto)
		{
			count++;
			return false;
		}
	);
	CHECK(count == 0);
}

TEST_CASE("File forLine not open", "[file][read]")
{
	File f(false);
	int	 count = 0;
	f.forLine(
		[&](auto)
		{
			count++;
			return false;
		}
	);
	CHECK(count == 0);
}

TEST_CASE("File iterators", "[file][read]")
{
	auto path = createFile("iter.txt", "x\ny\nz\n");
	File f;
	f.open(path, "", true);

	std::vector<std::string> lines(f.begin(), f.end());
	REQUIRE(lines.size() == 3);
	CHECK(lines[0] == "x");
	CHECK(lines[1] == "y");
	CHECK(lines[2] == "z");
}

// ─── Sections ───────────────────────────────────────────────

static constexpr auto INI_CONTENT = R"([General]
key1=value1
key2=value2

[Network]
host=example.com
port=8080

[Flags]
enabled=true
count=42
ratio=3.14
)";

TEST_CASE("forLineSection iterates section lines", "[file][section]")
{
	auto path = createFile("sections.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	std::vector<std::string> lines;
	f.forLineSection(
		"Network",
		[&](auto& str)
		{
			lines.push_back(str);
			return false;
		}
	);
	REQUIRE(lines.size() == 2);
	CHECK(lines[0] == "host=example.com");
	CHECK(lines[1] == "port=8080");
}

TEST_CASE("forLineSection caches after first call", "[file][section]")
{
	auto path = createFile("section_cache.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	std::vector<std::string> first;
	f.forLineSection(
		"General",
		[&](auto& str)
		{
			first.push_back(str);
			return false;
		}
	);

	std::vector<std::string> second;
	f.forLineSection(
		"General",
		[&](auto& str)
		{
			second.push_back(str);
			return false;
		}
	);

	REQUIRE(first.size() == second.size());
}

TEST_CASE("forLineSection missing section", "[file][section]")
{
	auto path = createFile("section_missing.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	int count = 0;
	f.forLineSection(
		"DoesNotExist",
		[&](auto&)
		{
			count++;
			return false;
		}
	);
	CHECK(count == 0);
}

TEST_CASE("forLineParametersSection parses key=value", "[file][section]")
{
	auto path = createFile("params.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	std::map<std::string, std::string> kv;
	f.forLineParametersSection(
		"General",
		[&](auto k, auto v)
		{
			kv[k] = v;
			return false;
		}
	);
	REQUIRE(kv.size() == 2);
	CHECK(kv["key1"] == "value1");
	CHECK(kv["key2"] == "value2");
}

TEST_CASE("positionSection returns 1-based index", "[file][section]")
{
	auto path = createFile("position.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto pos1 = f.positionSection("General");
	REQUIRE(pos1.has_value());
	CHECK(*pos1 == 1);

	auto pos2 = f.positionSection("Network");
	REQUIRE(pos2.has_value());
	CHECK(*pos2 == 2);

	auto pos3 = f.positionSection("Flags");
	REQUIRE(pos3.has_value());
	CHECK(*pos3 == 3);
}

TEST_CASE("positionSection missing section", "[file][section]")
{
	auto path = createFile("position_missing.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto pos = f.positionSection("Garbage");
	CHECK_FALSE(pos.has_value());
}

// ─── Parameter Reading ──────────────────────────────────────

TEST_CASE("parameterSection string", "[file][parameter]")
{
	auto path = createFile("param_str.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto res = f.parameterSection<std::string>("General", "key1");
	REQUIRE(res.has_value());
	CHECK(*res == "value1");
}

TEST_CASE("parameterSection bool", "[file][parameter]")
{
	auto path = createFile("param_bool.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto res = f.parameterSection<bool>("Flags", "enabled");
	REQUIRE(res.has_value());
	CHECK(*res == true);
}

TEST_CASE("parameterSection u32", "[file][parameter]")
{
	auto path = createFile("param_u32.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto res = f.parameterSection<u32>("Flags", "count");
	REQUIRE(res.has_value());
	CHECK(*res == 42);
}

TEST_CASE("parameterSection float", "[file][parameter]")
{
	auto path = createFile("param_float.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto res = f.parameterSection<float>("Flags", "ratio");
	REQUIRE(res.has_value());
	CHECK(*res == Catch::Approx(3.14f));
}

TEST_CASE("parameterSection missing parameter", "[file][parameter]")
{
	auto path = createFile("param_missing.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto res = f.parameterSection<std::string>("General", "nope");
	CHECK_FALSE(res.has_value());
}

TEST_CASE("parameterSection missing section", "[file][parameter]")
{
	auto path = createFile("param_missing_section.ini", INI_CONTENT);
	File f;
	f.open(path, "", true);

	auto res = f.parameterSection<std::string>("Void", "key1");
	CHECK_FALSE(res.has_value());
}

TEST_CASE("parameterSection not open", "[file][parameter]")
{
	File f(false);
	auto res = f.parameterSection<std::string>("Any", "any");
	CHECK_FALSE(res.has_value());
}

// ─── Writing ────────────────────────────────────────────────

TEST_CASE("writeText and save", "[file][write]")
{
	auto path = createFile("write_out.txt", "");
	File f;
	f.open(path, "", true);

	f.writeText("hello");
	f.writeText("world");
	f.save();

	std::ifstream			 ifs(path);
	std::string				 line;
	std::vector<std::string> lines;
	while (std::getline(ifs, line))
		lines.push_back(line);

	REQUIRE(lines.size() == 2);
	CHECK(lines[0] == "hello");
	CHECK(lines[1] == "world");
}

TEST_CASE("writeSectionParameter adds new parameter", "[file][write]")
{
	auto path = createFile("write_new_param.ini", "[Test]\n");
	File f;
	f.open(path, "", true);

	f.writeSectionParameter("Test", "foo", "bar");
	f.save();

	std::ifstream ifs(path);
	std::string	  content(std::istreambuf_iterator<char>(ifs), {});
	CHECK(content.find("foo=bar") != std::string::npos);
}

TEST_CASE("writeSectionParameter updates existing parameter", "[file][write]")
{
	auto path = createFile("write_update.ini", "[Test]\nfoo=old\n");
	File f;
	f.open(path, "", true);

	f.writeSectionParameter("Test", "foo", "new");
	f.save();

	std::ifstream ifs(path);
	std::string	  content(std::istreambuf_iterator<char>(ifs), {});
	CHECK(content.find("foo=new") != std::string::npos);
	CHECK(content.find("foo=old") == std::string::npos);
}

TEST_CASE("parameterSectionVector splits by ';'", "[file][parameter]")
{
	auto path = createFile("param_vec.ini", "[List]\nitems=ru;eu;us\n");
	File f;
	f.open(path, "", true);

	auto res = f.parameterSectionVector("List", "items");
	REQUIRE(res.has_value());
	REQUIRE(res->size() == 3);
	CHECK((*res)[0] == "ru");
	CHECK((*res)[1] == "eu");
	CHECK((*res)[2] == "us");
}

TEST_CASE("parameterSectionVector skips empty items", "[file][parameter]")
{
	auto path = createFile("param_vec_empty.ini", "[List]\nitems=ru;;us;\n");
	File f;
	f.open(path, "", true);

	auto res = f.parameterSectionVector("List", "items");
	REQUIRE(res.has_value());
	REQUIRE(res->size() == 2);
	CHECK((*res)[0] == "ru");
	CHECK((*res)[1] == "us");
}

TEST_CASE("writeSectionParameterVector joins by ';'", "[file][write]")
{
	auto path = createFile("write_vec.ini", "[Test]\n");
	File f;
	f.open(path, "", true);

	f.writeSectionParameterVector("Test", "items", { "ru", "eu", "us" });
	f.save();

	std::ifstream ifs(path);
	std::string	  content(std::istreambuf_iterator<char>(ifs), {});
	CHECK(content.find("items=ru;eu;us") != std::string::npos);
}

TEST_CASE("close saves when open and dirty", "[file][write]")
{
	auto path = createFile("close_save.txt", "original\n");
	{
		File f;
		f.open(path, "", true);
		f.writeText("added");
	}

	std::ifstream ifs(path);
	std::string	  content(std::istreambuf_iterator<char>(ifs), {});
	CHECK(content.find("original") != std::string::npos);
	CHECK(content.find("added") != std::string::npos);
}

// ─── Edge Cases ─────────────────────────────────────────────

TEST_CASE("File clear resets state", "[file][edge]")
{
	auto path = createFile("clear.txt", "some\ndata\n");
	File f;
	f.open(path, "", true);
	CHECK_FALSE(f.empty());

	f.clear();
	CHECK(f.empty());
	CHECK(f.lineSize() == 0);
}

TEST_CASE("File info_debug false suppresses warnings", "[file][edge]")
{
	File f(false);
	CHECK_NOTHROW(f.forLine([](auto) { return false; }));
	CHECK_NOTHROW(f.forLineSection("x", [](auto&) { return false; }));
	CHECK_NOTHROW(f.parameterSection<std::string>("x", "y"));
}

TEST_CASE("File open with expansion appends to path", "[file][edge]")
{
	createFile("base.txt", "content\n");

	File f;
	f.open(testDir() / "base", ".txt", true);
	CHECK(f.isOpen());
	CHECK(f.lineSize() == 1);
}

TEST_CASE("File empty check after write", "[file][edge]")
{
	auto path = createFile("empty_write.txt", "");
	File f;
	f.open(path, "", true);
	CHECK(f.empty());

	f.writeText("data");
	CHECK_FALSE(f.empty());
}

TEST_CASE("File re-open existing file after write", "[file][edge]")
{
	auto path = createFile("reopen_write.txt", "first\n");
	File f;
	f.open(path, "", true);

	f.writeText("second");
	f.close();

	f.open(path, "", true);
	CHECK(f.lineSize() == 2);
}

TEST_CASE("File forLineSection with empty section", "[file][edge]")
{
	auto path = createFile("empty_section.ini", "[Empty]\n[Next]\nk=v\n");
	File f;
	f.open(path, "", true);

	int count = 0;
	f.forLineSection(
		"Empty",
		[&](auto&)
		{
			count++;
			return false;
		}
	);
	CHECK(count == 0);
}

TEST_CASE("File parameterSection with empty value", "[file][edge]")
{
	auto path = createFile("empty_val.ini", "[S]\nk=\n");
	File f;
	f.open(path, "", true);

	auto res = f.parameterSection<std::string>("S", "k");
	CHECK_FALSE(res.has_value());
}

// ─── Regression: empty sections/params, duplicates, order ───────────────

static std::vector<std::string> fileLines(const fs::path& p)
{
	std::vector<std::string> lines;
	std::ifstream ifs(p);
	std::string  line;
	while (std::getline(ifs, line))
		lines.push_back(line);
	return lines;
}

static std::string fileContent(const fs::path& p)
{
	std::ifstream ifs(p);
	return std::string(std::istreambuf_iterator<char>(ifs), {});
}

static int countOf(const std::string& haystack, const std::string& needle)
{
	int count = 0;
	for (auto pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + needle.size()))
		count++;
	return count;
}

/// Collects the [X] section headers in order of appearance.
static std::vector<std::string> sectionTitles(const std::vector<std::string>& lines)
{
	std::vector<std::string> titles;
	for (auto& l : lines)
		if ((!l.empty()) && l.front() == '[' && l.back() == ']')
			titles.push_back(l);
	return titles;
}

TEST_CASE("regression: reads do not create empty section stubs on first run", "[file][regression]")
{
	// Simulates the first run: the UI reads a bunch of sections (not in the file yet)
	// and then only writes real values.
	auto path = createFile("reg_first.ini", "");
	File f;
	f.open(path, "", true);

	(void)f.parameterSection<std::string>("SYSTEM", "enable_dns_hosts");
	(void)f.parameterSection<std::string>("TG_WS_PROXY", "host");
	(void)f.parameterSection<std::string>("REMEMBER_CONFIGURATION", "config");
	(void)f.parameterSection<std::string>("UNBLOCK", "enable_game_mod");
	(void)f.parameterSection<std::string>("ZAPRET", "custom_hosts");

	f.writeSectionParameter("WINDOW", "width", "1040");
	f.writeSectionParameter("WINDOW", "height", "1020");
	f.writeSectionParameter("SYSTEM", "enable_dns_hosts", "false");
	f.writeSectionParameter("UNBLOCK", "enable_game_mod", "true");
	f.save();
	f.close();

	auto content = fileContent(path);

	// empty stub sections did not survive
	CHECK(content.find("[TG_WS_PROXY]") == std::string::npos);
	CHECK(content.find("[ZAPRET]") == std::string::npos);

	// real sections present and not duplicated
	REQUIRE(countOf(content, "[WINDOW]") == 1);
	REQUIRE(countOf(content, "[SYSTEM]") == 1);
	REQUIRE(countOf(content, "[UNBLOCK]") == 1);
}

TEST_CASE("regression: empty value parameter is not written", "[file][regression]")
{
	auto path = createFile("reg_empty_val.ini", "");
	File f;
	f.open(path, "", true);

	f.writeSectionParameter("ZAPRET", "custom_hosts", "");
	f.writeSectionParameter("ZAPRET", "custom_ip_set", "");
	f.writeSectionParameter("UNBLOCK", "enable_game_mod", "true");
	f.save();
	f.close();

	auto content = fileContent(path);
	CHECK(content.find("custom_hosts=") == std::string::npos);
	CHECK(content.find("[ZAPRET]") == std::string::npos);
	CHECK(content.find("enable_game_mod=true") != std::string::npos);
}

TEST_CASE("regression: empty vector writer does not create empty parameter", "[file][regression]")
{
	auto path = createFile("reg_empty_vec.ini", "");
	File f;
	f.open(path, "", true);

	f.writeSectionParameterVector("ZAPRET", "custom_hosts", {});
	f.writeSectionParameterVector("ZAPRET", "custom_domains_exclude", {});
	f.writeSectionParameter("UNBLOCK", "enable_facebook", "true");
	f.save();
	f.close();

	auto content = fileContent(path);
	CHECK(content.find("custom_hosts=") == std::string::npos);
	CHECK(content.find("[ZAPRET]") == std::string::npos);
	CHECK(content.find("enable_facebook=true") != std::string::npos);
}

TEST_CASE("regression: non-empty vector value is kept", "[file][regression]")
{
	auto path = createFile("reg_keep.ini", "");
	File f;
	f.open(path, "", true);

	f.writeSectionParameterVector("ZAPRET", "custom_hosts", { "ru", "eu" });
	f.writeSectionParameter("UNBLOCK", "enable_game_mod", "true");
	f.save();
	f.close();

	auto content = fileContent(path);
	CHECK(content.find("custom_hosts=ru;eu") != std::string::npos);
	CHECK(content.find("[ZAPRET]") != std::string::npos);
	// not split or duplicated
	REQUIRE(countOf(content, "custom_hosts=") == 1);
}

TEST_CASE("regression: empty value update keeps existing value", "[file][regression]")
{
	auto path = createFile("reg_empty_update.ini", "[SYSTEM]\nenable_dns_hosts=false\nshow_console=true\n");
	File f;
	f.open(path, "", true);

	// attempting to overwrite with empty must be ignored
	f.writeSectionParameter("SYSTEM", "show_console", "");
	f.save();
	f.close();

	auto content = fileContent(path);
	CHECK(content.find("show_console=true") != std::string::npos);
	// no separate empty show_console= appeared
	CHECK(content.find("show_console=\n") == std::string::npos);
	REQUIRE(countOf(content, "show_console=") == 1);
}

TEST_CASE("regression: reopen and update does not duplicate sections or params", "[file][regression]")
{
	auto path = createFile("reg_sessions.ini", "");

	{
		File f;
		f.open(path, "", true);
		f.writeSectionParameter("WINDOW", "width", "1040");
		f.writeSectionParameter("WINDOW", "height", "1020");
		f.writeSectionParameter("SYSTEM", "enable_dns_hosts", "false");
	}
	{
		File f;
		f.open(path, "", true);
		(void)f.parameterSection<std::string>("SYSTEM", "enable_dns_hosts");
		f.writeSectionParameter("WINDOW", "height", "1022");
		f.writeSectionParameter("SYSTEM", "show_console", "true");
	}
	{
		File f;
		f.open(path, "", true);
		f.writeSectionParameter("WINDOW", "height", "1000");
		f.writeSectionParameter("UNBLOCK", "enable_game_mod", "true");
	}

	auto content = fileContent(path);
	REQUIRE(countOf(content, "[WINDOW]") == 1);
	REQUIRE(countOf(content, "[SYSTEM]") == 1);
	REQUIRE(countOf(content, "height=") == 1);
	REQUIRE(countOf(content, "enable_dns_hosts=") == 1);

	// the last write wins
	CHECK(content.find("height=1000") != std::string::npos);
	CHECK(content.find("height=1020") == std::string::npos);
	CHECK(content.find("height=1022") == std::string::npos);
}

TEST_CASE("regression: section order follows creation, not alphabetical", "[file][regression]")
{
	auto path = createFile("reg_order.ini", "");
	File f;
	f.open(path, "", true);

	// Alphabetical order would have been: REMEMBER, SYSTEM, UNBLOCK, WINDOW.
	f.writeSectionParameter("WINDOW", "width", "1040");
	f.writeSectionParameter("SYSTEM", "enable_dns_hosts", "false");
	f.writeSectionParameter("UNBLOCK", "enable_game_mod", "true");
	f.writeSectionParameter("REMEMBER_CONFIGURATION", "config", "strategy.config");
	f.save();
	f.close();

	auto titles = sectionTitles(fileLines(path));
	REQUIRE(titles.size() == 4);
	CHECK(titles[0] == "[WINDOW]");
	CHECK(titles[1] == "[SYSTEM]");
	CHECK(titles[2] == "[UNBLOCK]");
	CHECK(titles[3] == "[REMEMBER_CONFIGURATION]");
}

TEST_CASE("regression: sanitizer cleans polluted file on reopen", "[file][regression]")
{
	// On-disk file already polluted (empty params/sections) from an old version.
	auto path = createFile("reg_polluted.ini",
		"[ZAPRET]\ncustom_hosts=\ncustom_ip_set=\n\n[SYSTEM]\nenable_dns_hosts=false\nshow_console=\n");
	File f;
	f.open(path, "", true);

	// any write forces _normalize() → the empties get swept out
	f.writeSectionParameter("SYSTEM", "check_update_app_startup", "true");
	f.save();
	f.close();

	auto content = fileContent(path);
	CHECK(content.find("custom_hosts=") == std::string::npos);
	CHECK(content.find("show_console=") == std::string::npos);
	CHECK(content.find("[ZAPRET]") == std::string::npos);
	CHECK(content.find("enable_dns_hosts=false") != std::string::npos);
	CHECK(content.find("check_update_app_startup=true") != std::string::npos);
}
