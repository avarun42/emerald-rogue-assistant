#include "Bridge/NativeLuaTransport.h"

#include <utility>

bool NativeLuaTransport::Submit(MemoryRequest request)
{
	if (m_State.load(std::memory_order_acquire) != TransportState::Connected)
		return false;

	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_State.load(std::memory_order_relaxed) != TransportState::Connected ||
		m_Requests.size() >= MaximumOutstandingMemoryRequests)
	{
		return false;
	}
	m_Requests.push(std::move(request));
	return true;
}

std::vector<MemoryResult> NativeLuaTransport::PollResults()
{
	std::vector<MemoryResult> results;
	std::lock_guard<std::mutex> lock(m_Mutex);
	results.reserve(m_Results.size());
	while (!m_Results.empty())
	{
		results.push_back(std::move(m_Results.front()));
		m_Results.pop();
	}
	return results;
}

TransportState NativeLuaTransport::State() const
{
	return m_State.load(std::memory_order_acquire);
}

void NativeLuaTransport::Stop()
{
	m_State.store(TransportState::Stopped, std::memory_order_release);
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Requests = {};
	m_Results = {};
}

bool NativeLuaTransport::TryPopRequest(MemoryRequest& request)
{
	if (m_State.load(std::memory_order_acquire) != TransportState::Connected)
		return false;

	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_Requests.empty())
		return false;
	request = std::move(m_Requests.front());
	m_Requests.pop();
	return true;
}

void NativeLuaTransport::Complete(MemoryResult result)
{
	if (m_State.load(std::memory_order_acquire) != TransportState::Connected)
		return;

	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_State.load(std::memory_order_relaxed) == TransportState::Connected)
		m_Results.push(std::move(result));
}
