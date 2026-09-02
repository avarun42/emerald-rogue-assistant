#include "Bridge/NativeLuaTransport.h"

#include <utility>

NativeLuaTransport::NativeLuaTransport(NativeSubmitter submitter) : m_Submitter(std::move(submitter))
{
}

bool NativeLuaTransport::Submit(MemoryRequest request)
{
	if (m_State.load(std::memory_order_acquire) != TransportState::Connected || !m_Submitter)
		return false;

	std::weak_ptr<NativeLuaTransport> const weakSelf = weak_from_this();
	return m_Submitter(std::move(request), [weakSelf](MemoryResult result) {
		if (auto const self = weakSelf.lock())
			self->PushResult(std::move(result));
	});
}

std::vector<MemoryResult> NativeLuaTransport::PollResults()
{
	std::vector<MemoryResult> results;
	std::lock_guard<std::mutex> lock(m_ResultMutex);
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
	std::lock_guard<std::mutex> lock(m_ResultMutex);
	m_Results = {};
}

void NativeLuaTransport::PushResult(MemoryResult result)
{
	if (m_State.load(std::memory_order_acquire) != TransportState::Connected)
		return;
	std::lock_guard<std::mutex> lock(m_ResultMutex);
	if (m_State.load(std::memory_order_relaxed) == TransportState::Connected)
		m_Results.push(std::move(result));
}
