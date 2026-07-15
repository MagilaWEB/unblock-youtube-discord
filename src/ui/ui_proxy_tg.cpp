#include "ui_proxy_tg.h"

#include "ui.h"
#include "ui_base.h"
#include "../unblock/unblock.h"

UiProxyTg::UiProxyTg(std::shared_ptr<Ui> ui, std::shared_ptr<Unblock> unblock)
    : _ui(std::move(ui)), _unblock(std::move(unblock))
{
}

void UiProxyTg::initialize()
{
	_settingEnableProxyTg();
	_settingEnableProxyLinkTg();
}

void UiProxyTg::_settingEnableProxyTg()
{
	_proxy_tg_enable
		->create("#tg_ws_proxy section .common", "str_checkbox_enable_proxy_tg_title",
				 Localization::Str{"str_checkbox_enable_proxy_tg_description"});
	_proxy_tg_enable->setState(_unblock->localProxyTgIsRun());
	_proxy_tg_enable->addEventClick(
		[this](JSArgs args)
		{
			Core::get().addTaskParallel(
				[this, args]
				{
					const bool state = JSToCPP<bool>(args[0]);
					if (state)
						_ui->getWindowWaitStartService()->show();
					else
						_ui->getWindowWaitStopService()->show();

					_unblock->localProxyTg(state);

					if (state)
						_ui->getWindowWaitStartService()->hide();
					else
						_ui->getWindowWaitStopService()->hide();
				}
			);

			return false;
		}
	);
}

void UiProxyTg::_settingEnableProxyLinkTg()
{
	_proxy_link_tg->create("#tg_ws_proxy section .common", "str_button_proxy_link_tg_title");

	_proxy_link_tg->addEventClick(
		[this](JSArgs)
		{
			_unblock->localProxyTgLinkRun();
			return false;
		}
	);
}
