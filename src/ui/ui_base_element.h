#pragma once
#include <AppCore/JSHelpers.h>
#include <Ultralight/View.h>

using namespace ultralight;

class BaseElement
{
protected:
	pcstr	   _name;
	pcstr	   _type;
	JSFunction _create;
	JSFunction _remove;
	JSFunction _add_event_click;

	inline static View*														 _view;
	inline static std::map<pcstr, BaseElement*>								 _all_element;
	inline static std::map<String, std::vector<std::function<bool(JSArgs)>>> _event_click{};

	void runCode(std::function<void()>&& run_code);

public:
	BaseElement() = delete;
	BaseElement(pcstr name);
	~BaseElement();

	void create(std::string selector, std::string title, bool first = false);
	void remove();
	void addEventClick(std::function<bool(JSArgs)>&& fn);

	static void initialize_all(View* view);
	static void release();

	virtual void initialize() = 0;
	static bool	 event_click(const JSObject& obj, const JSArgs& args);
};
