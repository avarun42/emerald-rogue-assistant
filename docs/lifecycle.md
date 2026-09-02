# Application lifecycle and thread ownership

`Application` owns one `SessionWorker`. `SessionWorker` owns an
`ISessionRuntime` and runs it on one managed `std::jthread`. The production
runtime is `GameConnectionManager`, which in turn owns the game connection,
observed memory, behaviors, memory transport, ENet state, and persistence
updates.

```text
UI thread                           SessionWorker
---------                           -------------
window events                       memory transport
SFML rendering       UiCommand      GameConnection
local text input  --------------->  observed memory and behaviors
                  <---------------  ENet and persistence
                    UiSnapshot
```

The UI cannot obtain a `GameConnection` or behavior pointer. It submits bounded
value commands and renders a copied `UiSnapshot`. Multiplayer input is
validated, persisted, and applied on the worker. Snapshot publication and the
command queue are the only general application thread boundaries.

At most 256 UI commands may wait. Once shutdown begins, new commands are
rejected. The worker then stops the memory transport, disconnects the game,
detaches behaviors (which completes a pending Home Box save and closes ENet),
flushes settings, publishes a final snapshot, and joins before its owner is
destroyed. Runtime exceptions also follow this teardown path and appear in the
final failed snapshot.

## Native DLL transition

The Windows DLL must still rendezvous with callbacks made by mGBA's Lua thread.
A legacy-only process object therefore owns the transitional native transport
and the managed UI `std::jthread`. It contains no game, behavior, persistence,
or UI state. The UI thread owns its SFML window and `Application`; the
application still owns exactly one session worker. This rendezvous disappears
with the DLL after socket parity. The standalone application introduced later
runs the same UI loop directly on the process main thread.
