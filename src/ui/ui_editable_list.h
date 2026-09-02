#pragma once
#include "ui_base_element.h"

#include <functional>
#include <string>
#include <vector>

// EditableList — универсальный редактируемый список.
//
// Отображает набор строк с кнопкой удаления на каждом элементе и полем
// ввода внизу для добавления новых. Перед добавлением значение проходит
// через валидатор (setValidator). Каждое изменение уведомляет подписчиков
// через addEventChange.
class EditableList final : public BaseElement
{
	ui::dom::Element _list;
	ui::dom::Element _input;

	std::vector<std::string>						  _items;
	std::function<bool(const std::string&)> _validator;

public:
	EditableList(std::string_view name);

	void addEventClick(std::function<bool(JSArgs)>&& callback)								= delete;
	void create(std::string_view selector, Localization::Str title, bool first = false)	= delete;

	void initialize() override;

	void create(std::string_view selector, Localization::Str title, std::string description = {}, std::string placeholder = {}, bool first = false);

	void setValidator(std::function<bool(const std::string&)> validator);

	void addItem(std::string_view value);
	void setItems(std::vector<std::string> items);
	void removeItem(std::string_view value);
	void clear();

	const std::vector<std::string>& items() const;

	void addEventChange(std::function<bool(JSArgs)>&& callback);

private:
	void _renderItems();
	// Передаёт подписчикам событие change с {action, value}: "add"/"remove".
	void _notifyChange(std::string_view action, std::string_view value);
};

#define EDITABLE_LIST(name) \
	Ptr<EditableList>##name \
	{                       \
		#name               \
	}