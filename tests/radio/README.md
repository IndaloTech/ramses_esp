# Radio recovery regression tests

Run from the repository root:

```sh
python3 tests/radio/test_regressions.py
```

Requires Clang (or a sanitizer-capable compiler selected by `CC`), pthreads,
Python 3, and the checked-out managed cJSON dependency. Generated test sources,
ESP-IDF shims, and binaries live in temporary directories, not the firmware build.

The tests compile the production frame, UART, CC1101, and message code with
deterministic hardware/RTOS mocks. The MQTT test extracts the production publish
function and links the real cJSON implementation. AddressSanitizer and
UndefinedBehaviorSanitizer are enabled.

Coverage includes:

- Concurrent pool allocation/free (six threads, 120,000 ownership transfers),
  TX queue ownership, and abort cleanup.
- SPI receive-buffer boundaries, failed transfers, bounded mode waits, recovery
  retries, unstable FIFO status, and early TX underflow.
- Lost FIFO-low/completion notifications, short frames, stuck FIFO GPIO,
  failed TX preparation/start, and unconditional TX timeout recovery.
- Partial RX frame deadlines, multiple frames in one UART batch, and failed RX
  entry with delayed retries.
- UART data with lost notifications, overflow/framing/parity recovery, oversized
  event lengths, and bounded work under continuous input.
- MQTT payload cleanup on success and cJSON allocation failures.

Recovery uses monotonic time: 50 ms RX inter-byte timeout, 200 ms maximum RX
frame duration, 500 ms maximum TX duration, 20 ms radio mode-transition waits,
and 500 ms between failed RX-entry attempts. TX FIFO priming happens in IDLE;
its threshold is 33 bytes to provide approximately 6.9 ms of refill headroom.
Aborted transmissions release their message without a successful-TX echo.

These are host-side tests, not RF validation. Before deployment, run a device
soak test with real traffic, repeated TX, MQTT reconnects/backpressure, and flash
writes. Verify that every forced TX failure returns to RX (or logs retrying RX
entry), that RX resumes after malformed/truncated packets, and that there are no
heap assertions or progressive message-pool exhaustion. Nothing in this test
runner flashes a device.
