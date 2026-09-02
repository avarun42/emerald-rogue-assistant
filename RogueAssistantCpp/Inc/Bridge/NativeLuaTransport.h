#pragma once

#include "Bridge/GameMemoryTransport.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

class NativeLuaTransport : public IGameMemoryTransport, public std::enable_shared_from_this<NativeLuaTransport>
{
  public:
	using NativeCompletion = std::function<void(MemoryResult)>;
	using NativeSubmitter = std::function<bool(MemoryRequest, NativeCompletion)>;

	explicit NativeLuaTransport(NativeSubmitter submitter);

	bool Submit(MemoryRequest request) override;
	std::vector<MemoryResult> PollResults() override;
	TransportState State() const override;
	void Stop() override;

  private:
	void PushResult(MemoryResult result);

	NativeSubmitter m_Submitter;
	mutable std::mutex m_ResultMutex;
	std::queue<MemoryResult> m_Results;
	std::atomic<TransportState> m_State{TransportState::Connected};
};
