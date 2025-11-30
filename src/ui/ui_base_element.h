#pragma once

#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Weverything"
#else
	#pragma warning(push)
	#pragma warning(disable : 4'100)
#endif

#include <AppCore/JSHelpers.h>
#include <Ultralight/View.h>

#ifdef __clang__
	#pragma clang diagnostic pop
#else
	#pragma warning(pop)
#endif

using namespace ultralight;
#define JS_EVENT(map) static_cast<JSCallbackWithRetval>([this](JSObject, const JSArgs& args) -> JSValue { return this->eventCPP(args, map); })

class BaseElement
{
protected:
	const std::string _name;
	pcstr			  _type;
	JSFunction		  _create;
	JSFunction		  _remove;
	JSFunction		  _set_title;
	JSFunction		  _add_event_click;
	JSFunction		  _show;
	JSFunction		  _hide;
	bool			  _created{ false };

	inline static View* _view;
#if __clang__
	[[clang::no_destroy]]
#endif
	inline static std::map<std::string, BaseElement*> _all_element;

	using MapEvent = std::map<String, std::vector<std::function<bool(JSArgs)>>>;
#if __clang__
	[[clang::no_destroy]]
#endif
	inline static MapEvent _event_click{};

public:
	BaseElement() = delete;
	BaseElement(pcstr name);
	virtual ~BaseElement();

	pcstr name() const { return _name.c_str(); }

	static void runCode(const std::function<void()>& run_code);

	void create(pcstr selector, Localization::Str title, bool first = false);
	void remove();

	void show();
	void hide();

	bool isCreate() const;

	void setTitle(Localization::Str title);

	void addEventClick(std::function<bool(JSArgs)>&& callback);

	static void initializeAll(View* view);
	static void release();

	virtual void initialize() = 0;
	static bool	 eventCPP(const JSArgs& args, MapEvent& map_event);
};
