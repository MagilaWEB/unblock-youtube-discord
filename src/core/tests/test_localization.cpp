#include <catch2/catch_test_macros.hpp>

#include "../pch.h"
#include "../localization.h"

#include <fstream>

namespace fs = std::filesystem;

struct LocaleFixture
{
    fs::path root;
    fs::path original;

    LocaleFixture()
    {
        root = fs::temp_directory_path() / "unblock_locale_test";
        original = fs::current_path();
        fs::remove_all(root);
        fs::create_directories(root / "ui" / "text");
        fs::create_directories(root / "bin");
        fs::create_directories(root / "binaries");
        fs::create_directories(root / "configs");
        fs::current_path(root);
    }

    ~LocaleFixture()
    {
        fs::current_path(original);
        fs::remove_all(root);
    }

    void write(std::string_view name, std::string_view content)
    {
        std::ofstream ofs(root / "ui" / "text" / name);
        ofs << content;
    }
};

// ─── Singleton ──────────────────────────────────────────────

TEST_CASE("Localization singleton returns same instance", "[locale][singleton]")
{
    auto& a = Localization::get();
    auto& b = Localization::get();
    CHECK(&a == &b);
}

// ─── Translate ──────────────────────────────────────────────

TEST_CASE("Localization translates known key", "[locale][translate]")
{
    LocaleFixture fx;
    fx.write("US.list", "str_greeting=Hello\nstr_farewell=Goodbye\n");

    auto& loc = Localization::get();
    loc.set("US");

    CHECK(std::string_view(loc.translate("str_greeting")) == "Hello");
    CHECK(std::string_view(loc.translate("str_farewell")) == "Goodbye");
}

TEST_CASE("Localization unknown key returns key itself", "[locale][translate]")
{
    LocaleFixture fx;
    fx.write("US.list", "str_existing=yes\n");

    auto& loc = Localization::get();
    loc.set("US");

    CHECK(std::string_view(loc.translate("str_nonexistent")) == "str_nonexistent");
}

TEST_CASE("Localization switching locales changes output", "[locale][translate]")
{
    LocaleFixture fx;
    fx.write("US.list", "str_home=Home\n");
    fx.write("RU.list", "str_home=Главная\n");

    auto& loc = Localization::get();

    loc.set("US");
    CHECK(std::string_view(loc.translate("str_home")) == "Home");

    loc.set("RU");
    CHECK(std::string_view(loc.translate("str_home")) == "Главная");
}

// ─── Fallback ───────────────────────────────────────────────

TEST_CASE("Localization falls back to US when locale file missing", "[locale][fallback]")
{
    LocaleFixture fx;
    fx.write("US.list", "str_fallback=fallback_value\n");

    auto& loc = Localization::get();
    loc.set("NonExistentLocale");

    CHECK(std::string_view(loc.translate("str_fallback")) == "fallback_value");
}

TEST_CASE("Localization falls back when target and US both missing", "[locale][fallback]")
{
    LocaleFixture fx;
    // Intentionally write no US.list at all

    auto& loc = Localization::get();
    loc.set("Missing");

    CHECK(std::string_view(loc.translate("anything")) == "anything");
}

// ─── Parsing ────────────────────────────────────────────────

TEST_CASE("Localization ignores comment lines", "[locale][parse]")
{
    LocaleFixture fx;
    fx.write("US.list", "// this is a comment\nstr_key=value\n// another comment\n");

    auto& loc = Localization::get();
    loc.set("US");

    CHECK(std::string_view(loc.translate("str_key")) == "value");
}

TEST_CASE("Localization ignores empty lines", "[locale][parse]")
{
    LocaleFixture fx;
    fx.write("US.list", "\n\nstr_key=value\n\n");

    auto& loc = Localization::get();
    loc.set("US");

    CHECK(std::string_view(loc.translate("str_key")) == "value");
}

TEST_CASE("Localization handles multiline values", "[locale][parse]")
{
    LocaleFixture fx;
    fx.write("US.list", "str_multiline=line1\nline2\nline3\n");

    auto& loc = Localization::get();
    loc.set("US");

    std::string expected = "line1\nline2\nline3";
    CHECK(std::string_view(loc.translate("str_multiline")) == expected);
}

TEST_CASE("Localization trims whitespace around key", "[locale][parse]")
{
    LocaleFixture fx;
    fx.write("US.list", "  str_padded  =value\n");

    auto& loc = Localization::get();
    loc.set("US");

    CHECK(std::string_view(loc.translate("str_padded")) == "value");
}

// ─── State ──────────────────────────────────────────────────

TEST_CASE("Localization reload clears previous translations", "[locale][state]")
{
    LocaleFixture fx;
    fx.write("US.list", "str_only_first=first\n");

    auto& loc = Localization::get();
    loc.set("US");

    fx.write("US.list", "str_only_second=second\n");
    loc.set("US");

    CHECK(std::string_view(loc.translate("str_only_first")) == "str_only_first");
    CHECK(std::string_view(loc.translate("str_only_second")) == "second");
}

TEST_CASE("Localization empty file yields no translations", "[locale][state]")
{
    LocaleFixture fx;
    fx.write("US.list", "");

    auto& loc = Localization::get();
    loc.set("US");

    CHECK(std::string_view(loc.translate("anything")) == "anything");
}
