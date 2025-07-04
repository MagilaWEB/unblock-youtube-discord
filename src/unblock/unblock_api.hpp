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
	virtual void start()										   = 0;
	virtual void baseTestDomain()								   = 0;
	virtual void testDomains() const							   = 0;
	virtual void allOpenService()								   = 0;
	virtual void allRemoveService()								   = 0;
};
