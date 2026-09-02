#pragma once

#include "Bridge/GameMemoryTransport.h"

#include <atomic>
#include <mutex>
#include <queue>

class NativeLuaTransport : public IGameMemoryTransport
{
  public:
	bool Submit(MemoryRequest request) override;
	std::vector<MemoryResult> PollResults() override;
	TransportState State() const override;
	void Stop() override;

	// Transitional API used only by the in-process Windows Lua bridge.
	bool TryPopRequest(MemoryRequest& request);
	void Complete(MemoryResult result);

  private:
	mutable std::mutex m_Mutex;
	std::queue<MemoryRequest> m_Requests;
	std::queue<MemoryResult> m_Results;
	std::atomic<TransportState> m_State{TransportState::Connected};
};
