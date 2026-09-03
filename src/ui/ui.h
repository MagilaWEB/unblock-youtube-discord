#pragma once
#include "../engine/engine_api.hpp"
#include "../core/utils.h"
#include "ui_secondary_window.h"
#include "ui_button.h"
#include "ui_check_box.h"
#include "../unblock/unblock.h"
#include "ui_dns_hosts.h"
#include "ui_proxy_tg.h"
#include "ui_zapret.h"
#include "ui_unblock.h"

#include <memory>
#include <string_view>

class UI_API Ui final : public utils::DefaultInit,
						public std::enable_shared_from_this<Ui>
{
	friend class UiUnblock;
	friend class UiZapret2;
	IEngineAPI* _engine;

	std::shared_ptr<Unblock> _unblock;

	bool _init{ false };

	// Update unblock app
	CHECK_BOX(_enable_check_update_startup);
	BUTTON(_start_check_update_app);

	// Remove app
	BUTTON(_remove_app);
	SECONDARY_WINDOW(_window_remove_app);

	std::unique_ptr<UiDnsHosts> _ui_dns_hosts;
	std::unique_ptr<UiProxyTg>	_ui_proxy_tg;
	std::unique_ptr<UiZapret2>	_ui_zapret2;
	std::unique_ptr<UiUnblock>	_ui_unblock;

	// Root directory error
	SECONDARY_WINDOW(_window_root_directory_error);

	// Update unblock app
	SECONDARY_WINDOW(_window_update_unblock);
	SECONDARY_WINDOW(_window_wait_update_unblock);
	SECONDARY_WINDOW(_window_error_update_unblock);
	SECONDARY_WINDOW(_window_wait_check_update_unblock);

	SECONDARY_WINDOW(_window_warning_conflict_service);

	SECONDARY_WINDOW(_window_warning_whitelist);
	SECONDARY_WINDOW(_window_wait_test_whitelist);
	SECONDARY_WINDOW(_window_warning_no_internet);
	SECONDARY_WINDOW(_window_wait_start_service);

	// Footer
	BUTTON(_link_to_github);
	BUTTON(_link_to_telegram);
	BUTTON(_link_report_issue);
	BUTTON(_button_report_issue_tutorial);

public:
	std::shared_ptr<Ui> self;

	explicit Ui(IEngineAPI* engine);

	void initialize();
	void postConstruct();

	void update();

	const std::shared_ptr<File>& userConfig();
	bool						 hasCyrillicOrSpaceInBinaryPath() const { return _engine->hasCyrillicOrSpaceInBinaryPath(); }

	void		console(bool show);
	std::string langText(std::string_view text_id);

	// App termination (used when uninstalling the program).
	void OnClose(saucer::application*);

	// Registers the JS bridge (expose/inject) and window subscriptions; called before navigation.
	void setup(saucer::smartview* view);

	const Ptr<SecondaryWindow>& getWindowWaitStartService() { return _window_wait_start_service; }

	const Ptr<CheckBox>& getTestingDomainsStartup() const { return _ui_unblock->getTestingDomainsStartup(); }

	std::unique_ptr<UiUnblock>& getUiUnblock() { return _ui_unblock; }

private:
	void _domReady();
	void _closeWindow();

	void _initializeAppState();
	void _initComponents();
	void _initializeFooter();
	void _initializeWindowBase() const;

	void _checkConflictService();

	void _checkValidRootDirectory();

	void _removeApp();
	void _removeAppRun();

	void _checkWhitelist();

	// Update
	void _updateApp();
	void _checkAppUpdate();
	void _updateAppWindow();
	void _updateAppProgressWindowInfo();

	// Base footer
	void _footerElements();
};
