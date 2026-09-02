#include "Application/Application.h"
#include "Assets.h"
#include "Bridge/NativeLuaTransport.h"
#include "GameConnectionManager.h"
#include "Log.h"
#include "UI/PrimaryUI.h"
#include "UI/Window.h"
#include "UserData.h"

#include <exception>
#include <fstream>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <Windows.h>

#pragma warning(disable : 4244)

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
	return TRUE;
}

namespace
{
void DumpScriptsNextToExe()
{
	std::ofstream fileStream("RogueAssistant_mGBA.lua", std::ios::out);
	if (!fileStream)
	{
		LOG_ERROR("Cannot export RogueAssistant_mGBA.lua");
		return;
	}

	LOG_INFO("Dumping RogueAssistant_mGBA.lua next to exe");
	auto const& data = bin2cpp::getRogueAssistant_mGBALuaFile();
	char const* ptr = data.getBuffer();
	for (; *ptr != 0; ++ptr)
	{
		if (*ptr != '\r')
			fileStream << *ptr;
	}
}

void RunStubWindow()
{
	WindowConfig config;
	config.title = "Rogue Assistant";
	config.canBeDestroyed = true;

	Window window(config);
	PrimaryUI ui;
	ui.SetToStubTheme();
	rogue::app::UiSnapshot snapshot;
	snapshot.workerState = rogue::app::WorkerState::Stopped;

	if (!window.Create())
		return;
	window.EnterMainLoop([&ui, &snapshot](Window* currentWindow, void*) {
		ui.Render(*currentWindow, snapshot, [](rogue::app::UiCommand) { return false; });
		return true;
	});
}

struct DesktopLoopData
{
	rogue::app::Application& application;
	PrimaryUI& ui;
	std::stop_token stopToken;
};

bool RunDesktopFrame(Window* window, void* userData)
{
	auto& loop = *static_cast<DesktopLoopData*>(userData);
	rogue::app::UiSnapshot const snapshot = loop.application.Snapshot();
	loop.ui.Render(*window, snapshot,
				   [&loop](rogue::app::UiCommand command) { return loop.application.Submit(std::move(command)); });
	return !loop.stopToken.stop_requested();
}

void RunDesktop(std::stop_token stopToken, std::shared_ptr<NativeLuaTransport> transport)
{
	SetThreadDescription(GetCurrentThread(), L"RogueAssistant UI");

	WindowConfig config;
	config.title = "Rogue Assistant";
	config.imGuiEnabled = false;
	config.canBeDestroyed = false;

	Window window(config);
	PrimaryUI ui;
	if (!window.Create())
		return;

	rogue::app::Application application(std::make_unique<GameConnectionManager>(transport));
	DesktopLoopData loop{application, ui, stopToken};
	window.EnterMainLoop(RunDesktopFrame, &loop);
	application.Stop();
}

// The in-process DLL route uses one process-wide rendezvous because mGBA calls
// the C ABI from its Lua thread. Application and session state are not global.
class LegacyDllRuntime
{
  public:
	void Start()
	{
		Stop();
		auto transport = std::make_shared<NativeLuaTransport>();
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Transport = transport;
		m_UiThread = std::jthread([transport = std::move(transport)](std::stop_token token) {
			try
			{
				RunDesktop(token, transport);
			}
			catch (std::exception const& exception)
			{
				LOG_ERROR("Rogue Assistant UI failed: %s", exception.what());
			}
			catch (...)
			{
				LOG_ERROR("Rogue Assistant UI failed: unknown exception");
			}
		});
	}

	void Stop()
	{
		std::jthread uiThread;
		std::shared_ptr<NativeLuaTransport> transport;
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (!m_UiThread.joinable())
				return;
			m_UiThread.request_stop();
			uiThread = std::move(m_UiThread);
			transport = m_Transport;
		}

		uiThread.join();
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_Transport == transport)
			m_Transport.reset();
	}

	std::shared_ptr<NativeLuaTransport> Transport() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Transport;
	}

  private:
	mutable std::mutex m_Mutex;
	std::jthread m_UiThread;
	std::shared_ptr<NativeLuaTransport> m_Transport;
};

LegacyDllRuntime& LegacyRuntime()
{
	static LegacyDllRuntime runtime;
	return runtime;
}
} // namespace

__declspec(dllexport) int RogueAssistant_Main(bool isStub, std::vector<std::string> const&)
{
	if (isStub)
	{
		if (!UserData::Init())
			return 1;
		DumpScriptsNextToExe();
		RunStubWindow();
		UserData::Shutdown();
	}
	else
	{
		LegacyRuntime().Start();
	}
	return 0;
}

void RogueAssistant_Frame()
{
}

void RogueAssistant_Shutdown()
{
	LegacyRuntime().Stop();
}

std::shared_ptr<NativeLuaTransport> RogueAssistant_GetNativeTransport()
{
	return LegacyRuntime().Transport();
}
