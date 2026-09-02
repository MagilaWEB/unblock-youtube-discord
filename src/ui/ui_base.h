#pragma once

#include "../engine/engine_api.hpp"
#include "utils_saucer.hpp"

class Ui;

class UI_API UiBase final : public std::enable_shared_from_this<UiBase>
{
	std::shared_ptr<Ui> _ui;

	IEngineAPI* _engine;

public:
	UiBase() = delete;
	UiBase(IEngineAPI* engine);
	~UiBase();

	void postConstruct();

	const std::shared_ptr<File>& userConfig();
	bool hasCyrillicOrSpaceInBinaryPath() const { return _engine->hasCyrillicOrSpaceInBinaryPath(); }

public:
	void console(bool show);
	void update();
	std::string langText(std::string_view text_id);

	// App termination (used when uninstalling the program).
	void OnClose(saucer::application*);

public:
	// Registers the JS bridge (expose/inject) and window subscriptions; called before navigation.
	void setup(saucer::smartview* view);

private:
	void _domReady();
	void _closeWindow();
};
