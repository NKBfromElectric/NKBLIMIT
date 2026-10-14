# NKBLIMIT development rules

- Prioritise DSP correctness, sound quality, and real-time stability.
- Do not intentionally lower processing quality to optimise CPU.
- Run relevant DSP/processor tests after implementation changes.
- Never claim performance improvements without measurements.
- Do not copy commercial plugin code, assets, or private algorithms.
- Audio callbacks must not allocate memory, acquire locks, block, perform file I/O, or draw UI.
- Keep DSP independent of the editor; meters cross threads using lock-free atomics.
- Sanitise non-finite inputs and handle denormals.
- Preserve sample-rate/block-size support and accurate reported latency.
- Record consequential DSP decisions and limitations in docs/.
- Never report unfinished modes, metering, host validation, or quality goals as complete.
- Fix build errors before delivering; document genuinely unavailable checks.
- Do not push, publish, or change GitHub settings without explicit user instruction.
- Keep JUCE pinned. Retain AGPLv3 and third-party notices.
