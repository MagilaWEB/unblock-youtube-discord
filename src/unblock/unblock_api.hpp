#pragma once

enum class DpiApplicationType : u8
{
	BASE,
	ALL
};

struct IUnblockAPI
{
protected:
	IUnblockAPI() = default;

public:
	virtual ~IUnblockAPI() = default;

	virtual void changeDpiApplicationType(DpiApplicationType type) = 0;
	virtual bool checkSavedConfiguration()						   = 0;
	virtual void startAuto()									   = 0;
	virtual void startManual()									   = 0;
	virtual void testDomains() const							   = 0;
	virtual void allOpenService()								   = 0;
	virtual void allRemoveService()								   = 0;
};
