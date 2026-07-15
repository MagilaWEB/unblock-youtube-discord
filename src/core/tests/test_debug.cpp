#include <catch2/catch_test_macros.hpp>

#include "../pch.h"
#include "../debug.h"

#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;

struct CoutSilencer
{
    std::streambuf* old_cout;
    std::streambuf* old_cerr;
    std::ofstream null;
    CoutSilencer() : old_cout(std::cout.rdbuf()), old_cerr(std::cerr.rdbuf()), null("nul")
    {
        std::cout.rdbuf(null.rdbuf());
        std::cerr.rdbuf(null.rdbuf());
    }
    ~CoutSilencer()
    {
        std::cout.rdbuf(old_cout);
        std::cerr.rdbuf(old_cerr);
    }
};

struct DebugFixture
{
    fs::path root;

    DebugFixture()
    {
        root = fs::temp_directory_path() / "unblock_debug_test";
        fs::remove_all(root);
        fs::create_directories(root / "bin");
        fs::create_directories(root / "binaries");
        fs::create_directories(root / "configs");
        fs::current_path(root);
    }

    ~DebugFixture()
    {
        fs::current_path(fs::temp_directory_path());
        fs::remove_all(root);
    }
};

// ─── Stacktrace ─────────────────────────────────────────────

TEST_CASE("Debug::pretty_stacktrace returns non-empty", "[debug][stacktrace]")
{
    auto trace = Debug::pretty_stacktrace();
    CHECK_FALSE(trace.empty());
    CHECK(trace.find("Stacktrace") != std::string::npos);
}

// ─── State ──────────────────────────────────────────────────

TEST_CASE("Debug::commandLine returns value from initialize", "[debug][state]")
{
    Debug::initialize("test arg1 arg2");
    CHECK(Debug::commandLine() == "test arg1 arg2");
}

// ─── Non-throwing messages ──────────────────────────────────

TEST_CASE("Debug::print does not throw", "[debug][msg]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;
    CHECK_NOTHROW(Debug::print("print message {}", 42));
}

TEST_CASE("Debug::ok does not throw", "[debug][msg]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;
    CHECK_NOTHROW(Debug::ok("ok message"));
}

TEST_CASE("Debug::info does not throw", "[debug][msg]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;
    CHECK_NOTHROW(Debug::info("info message"));
}

TEST_CASE("Debug::warning does not throw", "[debug][msg]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;
    CHECK_NOTHROW(Debug::warning("warning message"));
}

TEST_CASE("Debug::please does not throw", "[debug][msg]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;
    CHECK_NOTHROW(Debug::please("please message"));
}

// ─── Throwing messages ──────────────────────────────────────

TEST_CASE("Debug::error throws Debug::exception", "[debug][msg][throw]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;
    CHECK_THROWS_AS(Debug::error("error message"), Debug::exception);
}

// ─── Check / Verify ─────────────────────────────────────────

TEST_CASE("Debug::check does not throw on true condition", "[debug][check]")
{
    DebugFixture fx;
    Debug::initialize("");
    CHECK_NOTHROW(Debug::check(true, "should not warn"));
}

TEST_CASE("Debug::verify does not throw on true condition", "[debug][verify]")
{
    DebugFixture fx;
    Debug::initialize("");
    CHECK_NOTHROW(Debug::verify(true, "should not throw"));
}

TEST_CASE("Debug::verify throws on false condition", "[debug][verify][throw]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;
    CHECK_THROWS_AS(Debug::verify(false, "should throw"), Debug::exception);
}

// ─── Log ────────────────────────────────────────────────────

TEST_CASE("Debug::initLogFile creates logs directory", "[debug][log]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;

    Debug::initLogFile();

    CHECK(fs::exists(fx.root / "logs"));
}

TEST_CASE("Debug log file contains written messages", "[debug][log]")
{
    DebugFixture fx;
    Debug::initialize("");
    CoutSilencer silencer;

    auto log_path = fx.root / "logs" / "log.txt";
    fs::create_directories(log_path.parent_path());

    { std::ofstream touch(log_path); }

    Debug::log.clear();
    Debug::log.close();
    Debug::log.open(log_path, "", true);

    Debug::print("log test {} {}", "print", 1);
    Debug::ok("log ok test");
    Debug::info("log info test");
    Debug::warning("log warning test");

    Debug::log.close();

    REQUIRE(fs::exists(log_path));

    std::ifstream ifs(log_path);
    std::string content((std::istreambuf_iterator<char>(ifs)), {});

    CHECK(content.find("log test print 1") != std::string::npos);
    CHECK(content.find("log ok test") != std::string::npos);
    CHECK(content.find("log info test") != std::string::npos);
    CHECK(content.find("log warning test") != std::string::npos);

    auto line_count = std::count(content.begin(), content.end(), '\n');
    CHECK(line_count == 4);
}

