#include "BrowseState.hpp"
#include "SyncState.hpp"
#include "../Notification.hpp"
#include "../AssetManager.hpp"
#include "../Util.hpp"
#include "../Installer.hpp"
#include "SleepState.hpp"
#include "../InstalledList.hpp"
#include <TweenEngine/Tween.h>
#include <cpp3ds/Window/Window.hpp>
#include <sstream>
#include <cpp3ds/System/I18n.hpp>
#include <cpp3ds/System/FileSystem.hpp>

#define SECONDS_TO_SLEEP 60.f


namespace FreeShop {

BrowseState::BrowseState(StateStack& stack, Context& context, StateCallback callback)
: State(stack, context, callback)
, m_appListPositionX(0.f)
, m_threadInitialize(&BrowseState::initialize, this)
, m_threadLoadApp(&BrowseState::loadApp, this)
, m_threadMusic(&BrowseState::playMusic, this)
, m_threadBusy(false)
, m_activeDownloadCount(0)
, m_mode(Downloads)
, m_gwenRenderer(nullptr)
, m_isTransitioning(false)
{
	m_threadInitialize.launch();
}

BrowseState::~BrowseState()
{
	if (m_gwenRenderer)
	{
		delete m_settingsGUI;
		delete m_gwenCanvas;
		delete m_gwenSkin;
		delete m_gwenRenderer;
	}
}

void BrowseState::initialize()
{
	AppList::getInstance().refresh();
	InstalledList::getInstance().refresh();

	m_iconSet.addIcon(L"\uf11b");
	m_iconSet.addIcon(L"\uf019");
	m_iconSet.addIcon(L"\uf290");
	m_iconSet.addIcon(L"\uf013");
	m_iconSet.addIcon(L"\uf002");
	m_iconSet.setPosition(180.f, 15.f);

	m_textActiveDownloads.setCharacterSize(8);
	m_textActiveDownloads.setFillColor(cpp3ds::Color::Black);
	m_textActiveDownloads.setOutlineColor(cpp3ds::Color::White);
	m_textActiveDownloads.setOutlineThickness(1.f);
	m_textActiveDownloads.setPosition(218.f, 3.f);

	m_textListEmpty.setString(_("No title keys found.\nMake sure you have encTitleKeys.bin\nin your sdmc:/freeShop/ directory."));
	m_textListEmpty.useSystemFont();
	m_textListEmpty.setCharacterSize(16);
	m_textListEmpty.setFillColor(cpp3ds::Color(80, 80, 80, 255));
	m_textListEmpty.setPosition((400.f - m_textListEmpty.getLocalBounds().width) / 2, 70.f);

	m_whiteScreen.setPosition(0.f, 30.f);
	m_whiteScreen.setSize(cpp3ds::Vector2f(320.f, 210.f));
	m_whiteScreen.setFillColor(cpp3ds::Color::White);

	m_keyboard.loadFromFile("kb/keyboard.xml");

	m_textMatches.resize(4);
	for (auto& text : m_textMatches)
	{
		text.setCharacterSize(13);
		text.useSystemFont();
	}

	setMode(App);

	m_soundBlip.setBuffer(AssetManager<cpp3ds::SoundBuffer>::get("sounds/blip.ogg"));

	m_musicIntro.openFromFile("sounds/shop-intro.ogg");
	m_musicLoop.openFromFile("sounds/shop-loop.ogg");
	m_musicLoop.setLoop(true);

	g_browserLoaded = true;

	SleepState::clock.restart();
	requestStackClearUnder();
}

void BrowseState::renderTopScreen(cpp3ds::Window& window)
{
	if (!g_syncComplete || !g_browserLoaded)
		return;

	if (AppList::getInstance().getList().size() == 0)
		window.draw(m_textListEmpty);
	else
		window.draw(AppList::getInstance());

	// Special draw method to draw top screenshot if selected
	m_appInfo.drawTop(window);
}

void BrowseState::renderBottomScreen(cpp3ds::Window& window)
{
	if (!m_gwenRenderer)
	{
		m_gwenRenderer = new Gwen::Renderer::cpp3dsRenderer(window);
		m_gwenSkin = new Gwen::Skin::TexturedBase(m_gwenRenderer);
		m_gwenSkin->Init("DefaultSkin.png");
		m_gwenSkin->SetDefaultFont(L"", 11);

		m_settingsGUI = new GUI::Settings(m_gwenSkin, this);
	}
	if (!g_syncComplete || !g_browserLoaded)
		return;

	if (m_mode == Search)
	{
		window.draw(m_keyboard);
		for (auto& textMatch : m_textMatches)
			window.draw(textMatch);
	}
	if (m_mode == Settings)
	{
		m_settingsGUI->render();
	}

	window.draw(m_iconSet);

	if (m_activeDownloadCount > 0)
		window.draw(m_textActiveDownloads);

	if (m_mode == App)
		window.draw(m_appInfo);
	if (m_mode == Downloads)
	{
		window.draw(DownloadQueue::getInstance());
	}
	if (m_mode == Installed)
	{
		window.draw(InstalledList::getInstance());
	}

//	window.draw(m_whiteScreen);
	if (m_isTransitioning)
		window.draw(m_whiteScreen);
}

bool BrowseState::update(float delta)
{
	if (!g_syncComplete || !g_browserLoaded)
		return true;
	if (m_threadBusy)
		SleepState::clock.restart();

	if (SleepState::clock.getElapsedTime() > cpp3ds::seconds(SECONDS_TO_SLEEP))
	{
		requestStackPush(States::Sleep);
		return false;
	}

	int iconIndex = m_iconSet.getSelectedIndex();
	if (m_mode != iconIndex)
		setMode((Mode)iconIndex);

	if (m_mode == App)
	{
		m_appInfo.update(delta);
	}

	if (m_activeDownloadCount != DownloadQueue::getInstance().getActiveCount())
	{
		m_activeDownloadCount = DownloadQueue::getInstance().getActiveCount();
		m_textActiveDownloads.setString(_("%u", m_activeDownloadCount));
	}

	m_iconSet.update(delta);
	AppList::getInstance().update(delta);
	m_keyboard.update(delta);
	DownloadQueue::getInstance().update(delta);
	InstalledList::getInstance().update(delta);
	m_tweenManager.update(delta);
	return true;
}

bool BrowseState::processEvent(const cpp3ds::Event& event)
{
	SleepState::clock.restart();

	if (m_threadBusy || !g_syncComplete || !g_browserLoaded)
		return false;

	if (m_mode == App) {
		if (!m_appInfo.processEvent(event))
			return false;
	}
	else if (m_mode == Downloads) {
		DownloadQueue::getInstance().processEvent(event);
	} else if (m_mode == Installed) {
		InstalledList::getInstance().processEvent(event);
	} else if (m_mode == Settings) {
		m_settingsGUI->processEvent(event);
	}

	m_iconSet.processEvent(event);

	if (m_mode == Search)
	{
		if (!m_keyboard.processEvents(event))
			return true;

		cpp3ds::String currentInput;
		if (m_keyboard.popString(currentInput))
		{
			// Enter was pressed, so close keyboard
			m_iconSet.setSelectedIndex(App);
			m_lastKeyboardInput.clear();
		}
		else
		{
			currentInput = m_keyboard.getCurrentInput();
			if (m_lastKeyboardInput != currentInput)
			{
				m_lastKeyboardInput = currentInput;
				AppList::getInstance().filterBySearch(currentInput, m_textMatches);
				TweenEngine::Tween::to(AppList::getInstance(), AppList::POSITION_XY, 0.3f)
					.target(0.f, 0.f)
					.start(m_tweenManager);
			}
		}
	}
	else
	{
		// Events for all modes except Search
		if (event.type == cpp3ds::Event::KeyPressed)
		{
			int index = AppList::getInstance().getSelectedIndex();

			switch (event.key.code)
			{
				case cpp3ds::Keyboard::DPadUp:
					m_soundBlip.play();
					if (index % 4 == 0)
						break;
					setItemIndex(index - 1);
					break;
				case cpp3ds::Keyboard::DPadDown:
					m_soundBlip.play();
					if (index % 4 == 3)
						break;
					setItemIndex(index + 1);
					break;
				case cpp3ds::Keyboard::DPadLeft:
					m_soundBlip.play();
					setItemIndex(index - 4);
					break;
				case cpp3ds::Keyboard::DPadRight:
					m_soundBlip.play();
					setItemIndex(index + 4);
					break;
				case cpp3ds::Keyboard::L:
					setItemIndex(index - 8);
					break;
				case cpp3ds::Keyboard::R:
					setItemIndex(index + 8);
					break;
				default:
					break;
			}
		}
	}

	if (event.type == cpp3ds::Event::KeyPressed)
	{
		int index = AppList::getInstance().getSelectedIndex();

		switch (event.key.code)
		{
			case cpp3ds::Keyboard::Select:
				requestStackClear();
				return true;
			case cpp3ds::Keyboard::A: {
				m_threadBusy = true;
				if (!m_appInfo.getAppItem())
					m_threadLoadApp.launch();
				else
					TweenEngine::Tween::to(m_appInfo, AppInfo::ALPHA, 0.3f)
						.target(0.f)
						.setCallback(TweenEngine::TweenCallback::COMPLETE, [this](TweenEngine::BaseTween* source) {
							m_threadLoadApp.launch();
						})
						.start(m_tweenManager);
				break;
			}
			case cpp3ds::Keyboard::B:
				AppList::getInstance().filterBySearch("", m_textMatches);
				setItemIndex(0);
				break;
			case cpp3ds::Keyboard::X: {
				auto app = AppList::getInstance().getSelected()->getAppItem();
				if (app && !DownloadQueue::getInstance().isDownloading(app))
				{
					app->queueForInstall();
					cpp3ds::String s = app->getTitle();
					s.insert(0, _("Queued install: "));
					Notification::spawn(s);
				}
				break;
			}
			default:
				break;
		}
	}

	return true;
}


void BrowseState::setItemIndex(int index)
{
	if (AppList::getInstance().getVisibleCount() == 0)
		return;

	if (index < 0)
		index = 0;
	else if (index >= AppList::getInstance().getVisibleCount())
		index = AppList::getInstance().getVisibleCount() - 1;

	float extra = 1.0f; //std::abs(m_appList.getSelectedIndex() - index) == 8.f ? 2.f : 1.f;

	float pos = -200.f * extra * (index / 4);
	if (pos > m_appListPositionX)
		m_appListPositionX = pos;
	else if (pos <= m_appListPositionX - 400.f)
		m_appListPositionX = pos + 200.f * extra;

	TweenEngine::Tween::to(AppList::getInstance(), AppList::POSITION_X, 0.3f)
			.target(m_appListPositionX)
			.start(m_tweenManager);

	AppList::getInstance().setSelectedIndex(index);
}


void BrowseState::loadApp()
{
	auto item = AppList::getInstance().getSelected()->getAppItem();
	if (!item)
		return;

	m_iconSet.setSelectedIndex(App);
	if (m_appInfo.getAppItem() == item)
	{
		m_threadBusy = false;
		return;
	}

	setMode(App);

	// No cache to load, so show loading state
	bool showLoading = g_browserLoaded; //!item->isCached();
	if (showLoading)
		requestStackPush(States::Loading);

	m_appInfo.loadApp(item);

	TweenEngine::Tween::to(m_appInfo, AppInfo::ALPHA, 0.5f)
		.target(255.f)
		.start(m_tweenManager);

	if (showLoading)
		requestStackPop();

	m_threadBusy = false;
}


void BrowseState::setMode(BrowseState::Mode mode)
{
	if (m_mode == mode || m_isTransitioning)
		return;

	// Transition / end current mode
	if (m_mode == Search)
	{
		float delay = 0.f;
		for (auto& text : m_textMatches)
		{
			TweenEngine::Tween::to(text, util3ds::RichText::POSITION_X, 0.2f)
				.target(-text.getLocalBounds().width)
				.delay(delay)
				.start(m_tweenManager);
			delay += 0.05f;
		}

		AppList::getInstance().setCollapsed(false);
	}

	// Transition / start new mode
	if (mode == Search)
	{
		float posY = 1.f;
		for (auto& text : m_textMatches)
		{
			text.clear();
			text.setPosition(5.f, posY);
			posY += 13.f;
		}
		AppList::getInstance().setCollapsed(true);

		m_lastKeyboardInput = "";
		m_keyboard.setCurrentInput(m_lastKeyboardInput);
	}

	TweenEngine::Tween::to(m_whiteScreen, m_whiteScreen.FILL_COLOR_ALPHA, 0.4f)
		.target(255.f)
		.setCallback(TweenEngine::TweenCallback::COMPLETE, [=](TweenEngine::BaseTween* source) {
			m_mode = mode;
		})
		.start(m_tweenManager);
	TweenEngine::Tween::to(m_whiteScreen, m_whiteScreen.FILL_COLOR_ALPHA, 0.4f)
		.target(0.f)
		.setCallback(TweenEngine::TweenCallback::COMPLETE, [=](TweenEngine::BaseTween* source) {
			m_isTransitioning = false;
		})
		.delay(0.4f)
		.start(m_tweenManager);

	m_isTransitioning = true;
}

void BrowseState::playMusic()
{
	while (!g_syncComplete || !g_browserLoaded)
		cpp3ds::sleep(cpp3ds::milliseconds(50));
	cpp3ds::Clock clock;
	m_musicIntro.play();
	while (clock.getElapsedTime() < m_musicIntro.getDuration())
		cpp3ds::sleep(cpp3ds::milliseconds(5));
	m_musicLoop.play();
}

} // namespace FreeShop
