#pragma once

#include <memory>

template<typename T>
class Ptr : public std::unique_ptr<T>
{
	using inherited = std::unique_ptr<T>;

public:
	template<typename... Args>
	explicit Ptr(Args&&... args) : inherited(std::make_unique<T>(std::forward<Args>(args)...))
	{
	}
	explicit Ptr(nullptr_t) : inherited(nullptr) {}
};