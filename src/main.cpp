#include "FreeShop.hpp"
#include "DownloadQueue.hpp"
#include "States/BrowseState.hpp"
#include "States/SleepState.hpp"
#include "States/SyncState.hpp"

#ifndef EMULATION
#include <3ds.h>
#endif

#ifndef EMULATION
namespace {

aptHookCookie cookie;

void aptHookFunc(APT_HookType hookType, void *param)
{
	switch (hookType) {
		case APTHOOK_ONSUSPEND:
			if (FreeShop::SleepState::isSleeping && R_SUCCEEDED(gspLcdInit()))
			{
				GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_TOP);
				gspLcdExit();
			}
			// Fall through
		case APTHOOK_ONSLEEP:
			FreeShop::DownloadQueue::getInstance().suspend();
			break;
		case APTHOOK_ONRESTORE:
		case APTHOOK_ONWAKEUP:
			FreeShop::SleepState::isSleeping = false;
			FreeShop::SleepState::clock.restart();
			FreeShop::BrowseState::clockDownloadInactivity.restart();
			FreeShop::DownloadQueue::getInstance().resume();
			break;
		case APTHOOK_ONEXIT:
			FreeShop::SyncState::exitRequired = true;
			break;
		default:
			break;
	}
}

}
#endif


int main(int argc, char** argv)
{
#ifndef NDEBUG
	// Console for reading stdout
	cpp3ds::Console::enable(cpp3ds::BottomScreen, cpp3ds::Color::Black);
#endif

	cpp3ds::Service::enable(cpp3ds::Audio);
	cpp3ds::Service::enable(cpp3ds::Config);
	cpp3ds::Service::enable(cpp3ds::Network);
	cpp3ds::Service::enable(cpp3ds::SSL);
	cpp3ds::Service::enable(cpp3ds::Httpc);
	cpp3ds::Service::enable(cpp3ds::AM);

#ifndef EMULATION
	aptHook(&cookie, aptHookFunc, nullptr);
	AM_InitializeExternalTitleDatabase(false);
	//aptSetSleepAllowed(false);
#endif

	srand(time(NULL));
	auto game = new FreeShop::FreeShop();
	game->run();
	FreeShop::DownloadQueue::getInstance().suspend();
	FreeShop::DownloadQueue::getInstance().save();
	delete game;
	return 0;
}
