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
#define JS_EVENT(map) static_cast<JSCallbackWithRetval>([this](JSObject, const JSArgs& args) -> JSValue { return eventCPP(args, map); })

class BaseElement
{
protected:
	const pcstr _name;
	pcstr		_type;
	JSFunction	_create;
	JSFunction	_remove;
	JSFunction	_set_title;
	JSFunction	_add_event_click;
	bool		_created{ false };

	inline static View*							_view;
	inline static std::map<pcstr, BaseElement*> _all_element;

	using MapEvent = std::map<String, std::vector<std::function<bool(JSArgs)>>>;
	inline static MapEvent _event_click{};

	void runCode(std::function<void()> run_code);

public:
	BaseElement() = delete;
	BaseElement(pcstr name);
	virtual ~BaseElement();

	void create(pcstr selector, Localization::Str title, bool first = false);
	void remove();

	void setTitle(Localization::Str title);

	void addEventClick(std::function<bool(JSArgs)>&& callback);

	static void initializeAll(View* view);
	static void release();

	virtual void initialize() = 0;
	static bool	 eventCPP(const JSArgs& args, MapEvent& map_event);
};
