#include "utils.h"

void utils::str_replace(std::string& str, const std::string& old, const std::string& new_str)
{
	size_t start = { str.find_first_of(old) };

	while (start != std::string::npos)
	{
		str.replace(start, old.length(), new_str);
		start = str.find_first_of(old, start + new_str.length());
	}
}
