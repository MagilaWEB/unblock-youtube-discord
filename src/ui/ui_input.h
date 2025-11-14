#pragma once
#include "ui_base_element.h"

class Input : public BaseElement
{
	JSFunction _set_value;
	JSFunction _get_value;

public:
	enum class Types : u8
	{
		text,
		number,
		color,
		time,
		ip,
		port
	};

	inline static std::pair<Types, pcstr> convert_types[]{
		{	  Types::text,   "text" },
		 { Types::number, "number" },
		  {	Types::color,  "color" },
		{	  Types::time,   "time" },
		 {	   Types::ip,	  "ip" },
		  {	Types::port,	 "port" }
	};

public:
	Input(pcstr name);

	void addEventClick(std::function<bool(JSArgs)>&& fn)					 = delete;
	void create(pcstr selector, Localization::Str title, bool first = false) = delete;

	void initialize() override;

	void create(pcstr selector, Types type, JSValue value, Localization::Str title, Localization::Str description, bool first = false);
	void addEventSubmit(std::function<bool(JSArgs)>&& callback);

	void	setValue(JSValue value);
	JSValue getValue();
};

#define INPUT(name)  \
	Ptr<Input>##name \
	{                \
		#name        \
	}
