# Performance status

DSPTests prints the largest short-run real-time factor (elapsed processing time /
audio duration) across its numerical matrix. It includes both prepared 4x/8x paths,
the predictive safety FIR, and the output meters. Smaller is faster; values below
1 processed faster than the audio duration on that run.

This is not a sustained CPU benchmark and is not a Cubase CPU-percentage reading.
No performance optimisation claim follows from it. Before Phase 2 completion,
add sustained, warmed runs at each sample rate/block size, report hardware details
and timing percentiles, compare the separate 4x/8x engines, and measure worst-case
host callback deadlines under UI and multi-instance stress.

`VST3HostTests --compare <old-bundle> <new-bundle>` compares synthetic signals.
`VST3HostTests --music <old-bundle> <new-bundle>` reads the WAV specified by
`NKBLIMIT_TEST_AUDIO`, renders the complete file at +16/+24 dB with a -0.5 dBTP
ceiling and 8x, measures Short-term loudness and independent 16x true peak, and
optionally writes 30-second excerpts to `NKBLIMIT_TEST_RENDER_DIR` starting at
`NKBLIMIT_TEST_START_SECONDS`. User audio stays outside the source repository.
See docs/validation-0.1.1.md for results and the limitation observed at +24 dB.
