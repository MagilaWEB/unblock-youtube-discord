#pragma once
#include "ui_input.h"
#include "ui_list_ul.h"
#include "ui_text.h"

class Ui;

class UiZapretHelper
{
	std::shared_ptr<Ui> _ui;
	std::string			_root;

	// Hosts currently being checked by zapret-helper
	UL_LIST(_list_helper_checking);
	std::vector<std::string> _last_helper_checking;
	std::string				 _last_helper_checking_title;

	// Hosts that have been checked by zapret-helper at least once
	UL_LIST(_list_helper_seen);
	std::vector<std::string> _last_helper_seen;

	// Valid hosts with confirmed strategy (zapret statistics)
	UL_LIST(_list_helper_valid);
	std::vector<std::pair<std::string, std::string>> _last_helper_valid;

	// Hosts with current errors
	UL_LIST(_list_helper_error);
	std::vector<std::pair<std::string, std::string>> _last_helper_error;

	// Hosts fully tried on every strategy (dead, nothing left to attempt)
	UL_LIST(_list_helper_exhausted);
	std::vector<std::pair<std::string, std::string>> _last_helper_exhausted;

	// Hosts the helper tried, but Lua never judged (no valid/error/exhausted)
	UL_LIST(_list_helper_unjudged);
	std::vector<std::string> _last_helper_unjudged;

	// Live effectiveness line at the bottom of the general settings block:
	// valid / (valid+error+exhausted)
	TEXT_LABEL(_helper_summary);

	// Helper runtime settings ([HELPER] section). The on-disk file is stale
	// while unblock runs, so Apply writes userConfig (memory) + pushes UDP
	// CONFIG: to the running helper + stores the message in Unblock for the
	// next startService() push.
	INPUT(_helper_pool);
	INPUT(_helper_recheck_min);
	INPUT(_helper_errors_progress_min);
	INPUT(_helper_errors_recheck_sec);

	bool _visible{ true };

	struct HelperSettingDef
	{
		std::string_view key;
		Input::Types	 type;
		u32				 fallback;
		u32				 min_value;
		u32				 max_value;
		std::string_view unit;
		std::string_view title;
		std::string_view description;
		Ptr<Input> UiZapretHelper::* widget;
	};

	static std::span<const HelperSettingDef> helperSettingDefs();

public:
	UiZapretHelper(std::shared_ptr<Ui> ui, std::string_view root);

	void initialize();

	// Shows/hides the whole helper block (only meaningful on Zapret2).
	// Cheap: touches the DOM only on change.
	void setVisible(bool visible);

	void updateChecking();
	void updateSeen();
	void updateValid();
	void updateError();
	void updateExhausted();
	void updateUnjudged();
	void updateSummary();

private:
	std::string _sel(std::string_view tail) const { return _root + std::string{ tail }; }

	void _initHelperChecking();
	void _initHelperSeen();
	void _initHelperValid();
	void _initHelperError();
	void _initHelperExhausted();
	void _initHelperUnjudged();
	void _initHelperSummary();

	void _initHelperSettings();
	void _applyHelperSettings();
	u32	 _helperSettingU32(std::string_view key, u32 fallback) const;
	void _pushHelperSettings() const;
};
