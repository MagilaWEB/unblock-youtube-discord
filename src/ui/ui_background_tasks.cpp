#include "ui_background_tasks.h"

UiBackgroundTasks::UiBackgroundTasks() = default;

void UiBackgroundTasks::initialize()
{
	_indicator->create("footer", Localization::Str{ "str_tasks_indicator_title" });
	auto anchor = BaseElement::element(_indicator->name());
	anchor.addClass("tasks_indicator");
	_indicator->addTutorialStep("str_tour_tasks_title", "str_tour_tasks_description", 18);
	_indicator->addEventClick(
		[this](JSArgs)
		{
			_panel_open = !_panel_open;
			if (_panel_open)
				_panel.addClass("show");
			else
				_panel.removeClass("show");
			return false;
		}
	);
	_indicator->hide();

	// Panel docks to the indicator itself, so it always floats right above
	// it — footer sliding in/out moves both together, and hovering the
	// indicator never touches the footer box (no unwanted slide-out).
	_panel = ui::dom::create("div");
	_panel.addClass("tasks_panel");
	anchor.append(_panel);

	auto title = ui::dom::create("div");
	title.addClass("tasks_panel_title").text(Localization::Str{ "str_tasks_window_title" }());
	_panel.append(title);

	_panel_list = ui::dom::create("div");
	_panel_list.addClass("tasks_panel_list");
	_panel.append(_panel_list);
}

void UiBackgroundTasks::start(const std::string& id, const std::string& titleKey)
{
	std::lock_guard lock{ _mutex };
	auto&			slot = _tasks[id];
	if (!slot)
		slot = std::make_unique<Entry>();
	slot->titleKey = titleKey;
	slot->progress.store(-1.f);
}

void UiBackgroundTasks::setProgress(const std::string& id, float value)
{
	std::lock_guard lock{ _mutex };
	if (auto it = _tasks.find(id); it != _tasks.end())
		it->second->progress.store(value);
}

void UiBackgroundTasks::finish(const std::string& id)
{
	std::lock_guard lock{ _mutex };
	_tasks.erase(id);
}

bool UiBackgroundTasks::exists(const std::string& id) const
{
	std::lock_guard lock{ _mutex };
	return _tasks.contains(id);
}

std::vector<UiBackgroundTasks::SnapshotItem> UiBackgroundTasks::_snapshot() const
{
	std::lock_guard			  lock{ _mutex };
	std::vector<SnapshotItem> out;
	out.reserve(_tasks.size());
	for (const auto& [id, entry] : _tasks)
		out.push_back(SnapshotItem{ id, entry->titleKey, entry->progress.load() });
	return out;
}

void UiBackgroundTasks::update()
{
	if (!_indicator->isCreate() || !_panel.valid())
		return;

	const auto items = _snapshot();
	_syncIndicator(items);
	_syncRows(items);
}

void UiBackgroundTasks::_syncIndicator(const std::vector<SnapshotItem>& items)
{
	if (items.empty())
	{
		if (_indicator->isShow())
			_indicator->hide();
		_panel_open = false;
		_panel.removeClass("show");
		_last_indicator_text.clear();
		return;
	}

	const std::string text = utils::format(Localization::Str{ "str_tasks_indicator_title" }(), items.size());
	if (text != _last_indicator_text)
	{
		_last_indicator_text = text;
		_indicator->setTitle(Localization::Str{ text });
	}
	if (!_indicator->isShow())
		_indicator->show();
}

void UiBackgroundTasks::_syncRows(const std::vector<SnapshotItem>& items)
{
	if (!_panel_list.valid())
		return;

	// Remove rows of finished tasks.
	for (auto it = _rows.begin(); it != _rows.end();)
	{
		const bool alive = std::ranges::any_of(items, [&](const SnapshotItem& item) { return item.id == it->first; });
		if (!alive)
		{
			it->second.row.remove();
			it = _rows.erase(it);
		}
		else
			++it;
	}

	for (const auto& item : items)
	{
		auto rowIt = _rows.find(item.id);
		if (rowIt == _rows.end())
		{
			RowWidgets widgets;
			widgets.row = ui::dom::create("div");
			widgets.row.addClass("tasks_row");
			_panel_list.append(widgets.row);

			widgets.label = ui::dom::create("div");
			widgets.label.addClass("tasks_label").text(Localization::Str{ item.titleKey }());
			widgets.row.append(widgets.label);

			auto bar = ui::dom::create("div");
			bar.addClass("tasks_bar");
			widgets.row.append(bar);

			widgets.fill = ui::dom::create("div");
			widgets.fill.addClass("tasks_fill");
			bar.append(widgets.fill);

			_rows.emplace(item.id, std::move(widgets));
			rowIt = _rows.find(item.id);
		}

		auto& widgets = rowIt->second;
		if (item.progress < 0.f)
		{
			widgets.fill.addClass("tasks_indeterminate");
			widgets.fill.style("width", "40%");
		}
		else
		{
			widgets.fill.removeClass("tasks_indeterminate");
			const float clamped = std::clamp(item.progress, 0.f, 100.f);
			widgets.fill.style("width", utils::format("{}%", clamped));
		}
	}
}
