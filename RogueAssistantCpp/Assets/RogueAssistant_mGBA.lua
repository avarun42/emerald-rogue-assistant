require "RogueAssistant"

console:log("Launching Rogue Assistant...")
console:log("(Rogue Assistant will remain attached until the emulator closes)")
rogue_attach()

function onFrame()
	rogue_frame()

	--Process RW requests
	while true do
		local valid = rogue_next_data_request()

		if not valid then
			break
		end

		if rogue_data_request_is_read() then
			local addr, size = rogue_data_request_get_read();
			local result = emu:readRange(addr, size)
			rogue_data_request_provide_result(result)
		else
			while true do
				local addr, size, value = rogue_data_request_get_write();
				
				if size == 4 then
					emu:write32(addr, value)
				elseif size == 2 then
					emu:write16(addr, value)
				elseif size == 1 then
					emu:write8(addr, value)
				else
					break
				end
			end
			rogue_data_request_provide_result("")
		end
	end
end

function onShutdown()
	rogue_shutdown()
end

callbacks:add("frame", onFrame)
callbacks:add("shutdown", onShutdown)
callbacks:add("stop", onShutdown)