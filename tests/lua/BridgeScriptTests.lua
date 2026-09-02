local scriptPath = assert(arg[1], "expected bridge script path")
local goldenPath = assert(arg[2], "expected golden-vector path")

ROGUE_ASSISTANT_TEST = true
console = {
    log = function() end,
    error = function() end,
}
socket = {ERRORS = {AGAIN = "temporary failure"}}

local Bridge = assert(dofile(scriptPath))

local function hex(bytes)
    return (bytes:gsub(".", function(character)
        return string.format("%02x", string.byte(character))
    end))
end

local vectors = {}
for line in io.lines(goldenPath) do
    if string.sub(line, 1, 1) ~= "#" and line ~= "" then
        local name, value = string.match(line, "^([^=]+)=(.+)$")
        assert(name and value, "malformed golden vector")
        vectors[name] = value
    end
end

assert(hex(Bridge.encodeClientHello()) == vectors.client_hello)
assert(hex(assert(Bridge.encodeFrame(Bridge.message.CLOSE, 0, ""))) == vectors.close)

local decoder = Bridge.newDecoder()
local hello = Bridge.encodeClientHello()
for index = 1, #hello do
    assert(Bridge.feedDecoder(decoder, string.sub(hello, index, index)))
end
assert(Bridge.feedDecoder(decoder, assert(Bridge.encodeFrame(Bridge.message.CLOSE, 0, ""))))
local frames = Bridge.pollDecoder(decoder)
assert(#frames == 2)
assert(frames[1].messageType == Bridge.message.CLIENT_HELLO)
assert(frames[2].messageType == Bridge.message.CLOSE)

local malformed = Bridge.newDecoder()
assert(not Bridge.feedDecoder(malformed, string.pack("<I4", 7)))
assert(malformed.error)

local sent = {}
local fakeSocket = {}
function fakeSocket:send(data, first, last)
    local sentLast = math.min(first + 2, last)
    table.insert(sent, string.sub(data, first, sentLast))
    return sentLast
end
local state = Bridge.newState()
state.socket = fakeSocket
assert(Bridge.queueWire(state, Bridge.encodeClientHello()))
assert(Bridge.queueWire(state, assert(Bridge.encodeFrame(Bridge.message.CLOSE, 0, ""))))
while #state.outgoing > 0 do
    Bridge.flushOutgoing(state)
end
assert(table.concat(sent) == Bridge.encodeClientHello()
    .. assert(Bridge.encodeFrame(Bridge.message.CLOSE, 0, "")))
assert(state.outgoingBytes == 0)

local writes = {}
emu = {}
emu.write8 = function(_, address, value) table.insert(writes, {8, address, value}) end
emu.write16 = function(_, address, value) table.insert(writes, {16, address, value}) end
emu.write32 = function(_, address, value) table.insert(writes, {32, address, value}) end
Bridge.writeAligned(0x02000001, string.char(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), 1, 10)
assert(#writes == 5)
assert(writes[1][1] == 8 and writes[1][2] == 0x02000001)
assert(writes[2][1] == 16 and writes[2][2] == 0x02000002)
assert(writes[3][1] == 32 and writes[3][2] == 0x02000004)
assert(writes[4][1] == 16 and writes[4][2] == 0x02000008)
assert(writes[5][1] == 8 and writes[5][2] == 0x0200000A)

local readCalls = {}
emu.readRange = function(_, address, size)
    table.insert(readCalls, {address, size})
    return string.rep(string.char(0x5A), size)
end
local operationState = Bridge.newState()
operationState.phase = "connected"
local largeReadSize = 300 * 1024
local readPayload = string.pack("<I4I4", 0x08000000, largeReadSize)
Bridge.processReceivedBytes(operationState,
    assert(Bridge.encodeFrame(Bridge.message.READ_REQUEST, 77, readPayload)))
assert(#operationState.operations == 1)
Bridge.processOperations(operationState)
assert(#operationState.operations == 1)
assert(operationState.operations[1].offset == 256 * 1024)
Bridge.processOperations(operationState)
assert(#operationState.operations == 0)
assert(#readCalls == 2)
assert(readCalls[1][2] == 256 * 1024)
assert(readCalls[2][2] == 44 * 1024)
local responseDecoder = Bridge.newDecoder()
assert(Bridge.feedDecoder(responseDecoder, operationState.outgoing[1].data))
local response = Bridge.pollDecoder(responseDecoder)[1]
assert(response.messageType == Bridge.message.READ_RESULT)
assert(response.requestId == 77 and #response.payload == largeReadSize)

assert(Bridge.isRangeAllowed(0x08000000, 4, Bridge.readableRanges))
assert(not Bridge.isRangeAllowed(0x08000000, 4, Bridge.writableRanges))
assert(Bridge.isRangeAllowed(0x0203FFFF, 1, Bridge.writableRanges))
assert(not Bridge.isRangeAllowed(0x0203FFFF, 2, Bridge.writableRanges))
assert(not Bridge.isRangeAllowed(0xFFFFFFF0, 32, Bridge.readableRanges))

local reconnect = Bridge.newState()
assert(Bridge.shouldReconnect(reconnect, 100))
assert(not Bridge.shouldReconnect(reconnect, 100))
assert(not Bridge.shouldReconnect(reconnect, 100.9))
assert(Bridge.shouldReconnect(reconnect, 101))
assert(Bridge.shouldReconnect(reconnect, 99))

print("Rogue Assistant Lua bridge tests passed")
