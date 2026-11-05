# TUI async worker prototype

This standalone C++20 prototype exercises a worker-to-UI event bridge. It is
not linked into the product and does not call `Agent::send()`.

## Contract demonstrated

- The UI thread owns presentation state and consumes events with `try_pop()`.
- One worker thread constructs, calls, and destroys its `TurnExecutor`.
- `submit()` permits one turn in flight. A second submission is rejected.
- Accepted events are copied into a bounded FIFO and retain producer order.
- The callback uses `try_lock`; it does not wait for the consumer to free queue
  space. Queue limits account for event count and variable payload bytes, not
  allocator overhead or total process RSS.
- On the first overflow/oversized event/contended queue for a turn, that turn's
  remaining detail events are dropped. The count is exposed in the turn result.
  This prevents a misleading partial event stream from looking complete.
- A `std::future<TurnCompletion>` is the authoritative technical completion
  channel, independent of the detail queue. It carries the original
  `exception_ptr` when the executor throws. It does not guarantee delivery of
  every `AgentEvent` terminal payload.
- `request_close()` rejects future work but does not cancel a running send;
  `join()` and destruction can wait until it returns. The prototype has no
  cancellation support.
- Observer-exception handling in tests uses a small publisher matching the
  current Agent policy (disable observer, call one diagnostic). The actual
  Agent is deliberately not instantiated or modified here.

This is a bounded, non-waiting application-level callback, not a claim of
lock-free execution or a hard real-time latency bound. Allocation and mutex
implementation behavior remain platform dependent.

## Build and run

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

ThreadSanitizer can be attempted with:

```sh
cmake -S . -B build-tsan -DTUI_ASYNC_ENABLE_TSAN=ON
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
```

The prototype consumes the product's canonical `AgentEvent` header by include
path only. Its CMake project is independent of the root product build.
