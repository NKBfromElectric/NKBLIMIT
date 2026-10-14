# Cubase validation still required

Use a copy of an existing project or a new test project. This development build is
not yet approved for production masters. Cubase 13 is installed on the development
machine, but installation, live DAW operation, and subjective listening are separate
from automated processor/VST3 tests.

- Copy the entire nkblimit.vst3 bundle to a chosen VST3 directory; rescan and confirm
  stereo effect classification, correct name, parameter names and reported latency.
- Load at 44.1/48/88.2/96/192 kHz. Test real-time playback and offline export.
- Match a dry impulse and a low-level sine using host latency compensation.
- Record Input Gain, Ceiling and OS automation; listen for clicks. Test rapid OS changes.
- Save/reopen projects and presets; confirm each parameter restores.
- Compare real-time capture and offline export with identical initial state/automation;
  flush latency and filter tails. Check host render tails and project boundaries.
- Create/delete editors, resize host windows, use HiDPI, fine adjustment, direct numeric
  entry and separate reset controls. Keep playback running during UI stress.
- Render drums, bass, vocals, guitars and cymbals. Loudness-match comparisons. Investigate
  gain modulation, transient preservation and pumping before any quality claim.
- Run 60+ minutes, multiple instances and CPU stress, then document crashes/dropouts.
- Validate final output using an independent certified true-peak meter.

Do not mark Phase 1 complete until the real Cubase checks succeed.

The user's screenshot confirms the initial0.1.0 appeared in a Cubase Stereo Out
insert and displayed live readings. The0.1.1 update still requires the complete
host checklist above; CLI host/pluginval validation is documented separately.
