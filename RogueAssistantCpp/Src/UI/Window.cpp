#include "UI/Window.h"
#include "Defines.h"
#include "Log.h"
#include "Platform/FileSystem.h"
#include "Platform/ResourceLocator.h"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace
{
bool LoadWindowIcon(sf::Image& icon, std::filesystem::path const& resourceDirectory)
{
	if (!resourceDirectory.empty())
	{
		rogue::platform::ResourceLocator const resources(resourceDirectory);
		std::vector<std::byte> bytes;
		std::string error;
		if (rogue::platform::ReadFile(resources.Resolve(rogue::platform::Resource::Icon), 16U * 1024U * 1024U, bytes,
									  error) &&
			icon.loadFromMemory(bytes.data(), bytes.size()))
		{
			return true;
		}
	}
	return false;
}
} // namespace

Window::Window(WindowConfig const& config) : m_Config(config)
{
}

Window::~Window()
{
	Destroy();
}

bool Window::Create()
{
	LOG_INFO("Creating Window");
	unsigned int style = sf::Style::Titlebar | sf::Style::Close;
	if (m_Config.resizable)
		style |= sf::Style::Resize;

	m_WindowHandle = std::make_unique<sf::RenderWindow>();
	m_WindowHandle->create(sf::VideoMode(m_Config.width, m_Config.height), m_Config.title, style);
	m_WindowHandle->setVerticalSyncEnabled(true);

	sf::Image icon;
	if (LoadWindowIcon(icon, m_Config.resourceDirectory))
		m_WindowHandle->setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
	else
		LOG_WARN("Cannot load the Rogue Assistant window icon");
	return m_WindowHandle->isOpen();
}

bool Window::Destroy()
{
	if (m_WindowHandle)
	{
		m_WindowHandle->close();
		m_WindowHandle.reset();
	}
	return true;
}

void Window::EnterMainLoop(WindowCallback callback, void* userData)
{
	if (!m_WindowHandle)
	{
		ASSERT_FAIL("Window not created yet");
		return;
	}

	bool continueLoop = true;
	while (m_WindowHandle->isOpen() && continueLoop)
	{
		m_PreviousKeyStates = m_CurrentKeyStates;

		sf::Event event{};
		while (m_WindowHandle->pollEvent(event))
		{
			if (event.type == sf::Event::KeyPressed && event.key.code != sf::Keyboard::Unknown)
				m_CurrentKeyStates.set(static_cast<std::size_t>(event.key.code), true);
			if (event.type == sf::Event::KeyReleased && event.key.code != sf::Keyboard::Unknown)
				m_CurrentKeyStates.set(static_cast<std::size_t>(event.key.code), false);

			if (event.type == sf::Event::TextEntered && event.text.unicode < 128)
			{
				if (event.text.unicode == 8)
				{
					if (!m_TextEntered.empty())
						m_TextEntered.pop_back();
				}
				else if (event.text.unicode == 22)
				{
					std::string const clipboard = sf::Clipboard::getString().toAnsiString();
					std::size_t const available = 256U - std::min<std::size_t>(m_TextEntered.size(), 256U);
					m_TextEntered.append(clipboard, 0, std::min(clipboard.size(), available));
				}
				else if (event.text.unicode == 1)
				{
					m_TextEntered.clear();
				}
				else if (event.text.unicode >= 0x20)
				{
					m_TextEntered += static_cast<char>(event.text.unicode);
				}
			}

			if (event.type == sf::Event::Closed && m_Config.canBeDestroyed)
				continueLoop = false;
		}

		m_WindowHandle->clear();
		if (!callback(this, userData))
			continueLoop = false;
		m_WindowHandle->display();
	}

	Destroy();
}
