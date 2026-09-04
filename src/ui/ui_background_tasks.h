#pragma once
#include "../core/ptr.h"
#include "ui_button.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// -----------------------------------------------------------------------
// UiBackgroundTasks — minimalist indicator of silent background jobs.
//
// Only quiet tasks are registered here (e.g. update check at startup).
// Visible long operations (DNS update, auto-config, testing) keep their
// own modal windows and are NOT duplicated in the indicator.
//
// The task list lives in a custom expandable block in the bottom-right
// corner (NOT a modal SecondaryWindow): its size follows the number of
// tasks and the longest label, show/hide is animated via CSS.
//
// progress: < 0 — indeterminate (spinner bar), otherwise 0..100.
// Threading: start/setProgress/finish are called from Core worker threads,
// update() is called from the UI thread (Ui::update).
// -----------------------------------------------------------------------
class UiBackgroundTasks
{
	struct Entry
	{
		std::string		   titleKey;
		std::atomic<float> progress{ -1.f };
	};

	struct RowWidgets
	{
		ui::dom::Element row;
		ui::dom::Element label;
		ui::dom::Element fill;
	};

	struct SnapshotItem
	{
		std::string id;
		std::string titleKey;
		float		progress{ -1.f };
	};

	mutable std::mutex							  _mutex;
	std::map<std::string, std::unique_ptr<Entry>> _tasks;

	BUTTON(_indicator);

	ui::dom::Element _panel;
	ui::dom::Element _panel_list;
	bool			 _panel_open{ false };

	std::map<std::string, RowWidgets> _rows;
	std::string						  _last_indicator_text;

public:
	UiBackgroundTasks();

	void initialize();
	void update();

	// Registry (thread-safe, callable from any thread).
	void start(const std::string& id, const std::string& titleKey);
	void setProgress(const std::string& id, float value);
	void finish(const std::string& id);
	bool exists(const std::string& id) const;

private:
	std::vector<SnapshotItem> _snapshot() const;
	void					  _syncIndicator(const std::vector<SnapshotItem>& items);
	void					  _syncRows(const std::vector<SnapshotItem>& items);
};
