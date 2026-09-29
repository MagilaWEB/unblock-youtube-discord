#include "app_update.h"

#include "../engine/version.hpp"
#include "http_load_content.h"

#include <bit7z/bitfileextractor.hpp>
#include <filesystem>
#include <string>

std::optional<std::string> AppUpdater::check() const
{
	HttpsLoad version{ "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest" };

	auto lines = version.run();

	if (version.codeResult() != 200)
		return {};

	for (auto& line : lines)
	{
		constexpr static std::string_view version_mask{ "/MagilaWEB/unblock-youtube-discord/tree/v" };
		size_t							  pos = line.find(version_mask);
		if (pos != std::string::npos)
		{
			constexpr static std::string_view mask_end{ "\" data-tab-item=\"i0code-tab\"" };
			size_t							  pos_end = line.find(mask_end);
			if (pos_end != std::string::npos)
			{
				auto start_str = pos + version_mask.length();
				auto str	   = line.substr(start_str, pos_end - start_str);
				if (Core::get().isVersionNewer(str, VERSION_STR))
					return str;

				return {};
			}
		}
	}

	return {};
}

bool AppUpdater::run()
{
	// The update is delegated to the standalone unblock_update.exe (see src/unblock_update);
	// all temporary files live under %TEMP%\unblock — no .bat scripts are dropped into
	// the application root.
	const auto temp_root = Core::get().tempPath() / "unblock";
	const auto bin_path	 = Core::get().binPath();

	std::error_code ec;
	std::filesystem::remove_all(temp_root, ec);
	std::filesystem::create_directories(temp_root, ec);

	const auto archive = temp_root / "new_unblock.7z";

	auto load = std::make_shared<HttpsLoad>("https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest/download/unblock.7z");
	{
		std::scoped_lock lock(_load_mutex);
		_load = load;
	}

	if (!load->runToFile(archive))
		return false;

	const u32 code = load->codeResult();
	if (code != 200)
		return false;

	try
	{
		// Absolute path: the process working directory is not guaranteed
		// to be bin/ (shortcuts, service starts), so a relative "7za.dll"
		// silently fails to load on the first update attempt.
		static bit7z::Bit7zLibrary	   lib{ (Core::get().binPath() / "7za.dll").string() };
		static bit7z::BitFileExtractor extractor{ lib, bit7z::BitFormat::SevenZip };

		extractor.extract(archive.string(), temp_root.string());
	}
	catch (const bit7z::BitException& ex)
	{
		Debug::warning("{}", ex.what());
		return false;
	}

	// The archive must contain the wrapped payload with the new engine —
	// otherwise the helper would wipe temp and restart the old build,
	// looking like "the update did nothing".
	const auto payload_dir = temp_root / "unblock";
	if (!std::filesystem::exists(payload_dir / "bin" / "engine.exe", ec) || ec)
	{
		Debug::warning("Update payload is missing unblock/bin/engine.exe");
		return false;
	}

	// Stage the helper into %TEMP% and run it from there (same as the
	// remove flow): a helper running from bin/ locks its own image, so the
	// payload copy of bin/ either fails mid-way (partial update that looks
	// like "nothing happened") or can never refresh the helper itself.
	// From %TEMP% the helper can wipe + copy managed dirs freely.
	const auto staged_updater = temp_root / "unblock_update.exe";
	{
		const auto	payload_updater = payload_dir / "bin" / "unblock_update.exe";
		const auto& source			= std::filesystem::exists(payload_updater) ? payload_updater : bin_path / "unblock_update.exe";

		std::error_code copy_ec;
		std::filesystem::copy_file(source, staged_updater, std::filesystem::copy_options::overwrite_existing, copy_ec);
		if (copy_ec)
		{
			Debug::error("Failed to stage unblock_update: {}", copy_ec.message());
			return false;
		}
	}

	std::wstring cmd_line = L"\"" + staged_updater.wstring() + L"\" \"" + Core::get().currentPath().wstring() + L"\" "
						  + std::to_wstring(GetCurrentProcessId()) + L" update \"" + temp_root.wstring() + L"\"";

	STARTUPINFOW		startup{};
	PROCESS_INFORMATION process{};
	startup.cb = sizeof(startup);

	if (!CreateProcessW(nullptr, cmd_line.data(), nullptr, nullptr, FALSE, 0, nullptr, temp_root.c_str(), &startup, &process))
	{
		Debug::error("Failed to start unblock_update: {}", static_cast<u32>(GetLastError()));
		return false;
	}

	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

float AppUpdater::progress() const
{
	std::shared_ptr<HttpsLoad> load;
	{
		std::scoped_lock lock(_load_mutex);
		load = _load;
	}

	return load ? load->progress() : 0.F;
}
