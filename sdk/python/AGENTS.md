# SDK contributor instructions

- All source comments, docstrings and public API documentation in this SDK are English.
- Match docs/LYO_D42_API.md in the firmware repository. Keep protocol byte layouts synchronized with the protocol test fixtures and both language implementations.
- High-level speed methods require acceleration in m/s² and reject wheel speeds above 0.32 m/s. Reject non-finite values before transport I/O.
- Never retry movement automatically. Match sequence/rid; ignore unrelated replies. Use bounded I/O and bounded response buffers.
- Do not create an independent heartbeat thread that can keep stale application motion alive. Provide explicit heartbeat and bounded motion helpers.
- Constructors and default examples must never move the chassis. Do not flash or connect to live hardware during automated tests.
- Preserve partial-frame handling, CAN extended IDs, little-endian fields, signed values, and the distinction between acceptance and execution.
- CAN does not support every generic JSON command or return full JSON telemetry; document this limitation rather than inventing fields.
- Run unit tests and examples compilation before packaging. No package-registry publishing without an explicit request.
