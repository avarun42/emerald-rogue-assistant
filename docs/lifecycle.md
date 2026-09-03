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

At most 256 UI commands can wait. When shutdown begins, the worker rejects new
commands. It then stops the memory transport, disconnects the game, detaches
behaviors, flushes settings, publishes a final snapshot, and joins before its
owner is destroyed. Detaching the behaviors completes a pending Home Box save
and closes ENet. Runtime exceptions follow the same teardown path and appear in
the final failed snapshot.

## Process entry point

`DesktopMain` runs the UI loop directly on the process main thread on every
supported platform. mGBA hosts only the portable Lua memory adapter; it never
loads application-native code. The retired Windows DLL rendezvous and its
second managed UI thread are not part of the production lifecycle.
