#pragma once
#include "Application/UiModel.h"
#include "Timer.h"

#include <functional>

struct AssetCollection;

class Window;

class PrimaryUI
{
public:
	PrimaryUI();
	~PrimaryUI();

	using CommandSink = std::function<bool(rogue::app::UiCommand)>;

	void Render(Window& window, rogue::app::UiSnapshot const& snapshot, CommandSink const& submitCommand);
	void SetToStubTheme();

private:
	void RenderAwaitingPage(Window& window);
	void RenderMultiplayerPage(Window& window, rogue::app::UiSnapshot const& snapshot,
		rogue::app::ConnectionSnapshot const& connection, bool initialLoad, CommandSink const& submitCommand);
	void RenderHomeBoxPage(Window& window, rogue::app::HomeBoxSnapshot const& homeBox, bool initialLoad);

	AssetCollection* m_Assets;

	int m_CurrentConnectionIdx = 0;
	std::uint64_t m_CurrentConnectionId = 0;
	TimeDurationNS m_LastDrawTime;
	rogue::app::UiPage m_CurrentPage;
};
