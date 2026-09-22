#pragma once
#include "ui_input.h"
#include "ui_list_ul.h"

class Ui;

class UiZapretHelper
{
	std::shared_ptr<Ui> _ui;
	std::string			_root;

	// Hosts currently being checked by zapret-helper
	UL_LIST(_list_helper_checking);
	std::vector<std::string> _last_helper_checking;

	// Hosts that have been checked by zapret-helper at least once
	UL_LIST(_list_helper_seen);
	std::vector<std::string> _last_helper_seen;

	// Valid hosts with confirmed strategy (zapret statistics)
	UL_LIST(_list_helper_valid);
	std::vector<std::pair<std::string, std::string>> _last_helper_valid;

	// Hosts with current errors
	UL_LIST(_list_helper_error);
	std::vector<std::pair<std::string, std::string>> _last_helper_error;

	// Helper runtime settings ([HELPER] section). The on-disk file is stale
	// while unblock runs, so Apply writes userConfig (memory) + pushes UDP
	// CONFIG: to the running helper + stores the message in Unblock for the
	// next startService() push.
	INPUT(_helper_pool);
	INPUT(_helper_check_timeout);
	INPUT(_helper_connect_timeout);
	INPUT(_helper_max_redirects);
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

private:
	std::string _sel(std::string_view tail) const { return _root + std::string{ tail }; }

	void _initHelperChecking();
	void _initHelperSeen();
	void _initHelperValid();
	void _initHelperError();

	void _initHelperSettings();
	void _applyHelperSettings();
	u32	 _helperSettingU32(std::string_view key, u32 fallback) const;
	void _pushHelperSettings() const;
};
