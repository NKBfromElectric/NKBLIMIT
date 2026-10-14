# Phase 1 DSP architecture and decisions

## Signal path

Sanitisation -> smoothed input gain -> parallel 4x/8x high-quality linear-phase FIR
oversamplers -> Transparent lookahead engines -> downsampling -> latency alignment
-> smoothed OS crossfade -> base-rate TruePeakGuard -> output metering.

Source/DSP has no editor dependency. The core requires only JUCE DSP for resampling;
the envelope, predictor, buffers and safety logic are independently readable C++.
No DC blocker is included: removing DC changes sound. DC is instead a test signal.

## Transparent

`ceil(fs*0.0025)` base-rate samples of lookahead, scaled exactly by OS factor.
A preallocated monotonic maximum deque observes the entire lookahead window,
including both endpoints. Both channels share a single target gain
`min(1, ceiling / windowPeak)`. A 0.1 ms attack approaches that target before the
delayed peak arrives. An exact linked gain cap enforces the delayed sample constraint
for unusually extreme events; this is not the main gain control or a clipper.

AdaptiveRelease tracks 30 ms peak energy and smooths its time constant over 20 ms.
Release varies from roughly 45 to 313 ms with sustain ratio and reduction depth.
It is an initial explicit control law, not a claim of subjective transparency.
Punch/Loud must later use distinct control topologies rather than preset constants.

## Resampling and latency

JUCE's maximum-quality equiripple half-band FIR stages, integer latency enabled.
Linear phase is selected for predictable alignment; pre-ringing needs listening
evaluation. The higher latency determines the common delay; a preallocated stereo
delay compensates the shorter path. Both run continuously, so switching needs no
allocation or cold filter start. This costs more than running only the selected path.
The reported latency is `max(OS FIR latency) + ceil(fs*2.5ms) + 31` samples.
Sample rate changes require host prepareToPlay. Larger/unusual blocks are subdivided
into preallocated chunks with continuous state. No process-block-size allocation.
MSVC source/execution character encoding is explicitly UTF-8, preserving JUCE's
Unicode source files on Japanese Windows installations.

## Post-downsampling safety

The safety guard is intentionally conservative for the first build. An 8-phase,
32-tap normalised Blackman-windowed sinc FIR forms **absolute contribution bounds**
`B[n] = max_phase,channel sum_k |h[phase,k] * x[channel,n-k]|`, including sample peaks.
For an input sample m, every FIR support containing it corresponds to n=m..m+31.
Delay output by 31 samples and take the maximum of those bounds. Apply
`g[m] <= min(1, ceiling*10^(-0.3/20) / max B[n])`. Attack is immediate in the guard;
release is 80 ms. Under constant ceiling, triangle inequality bounds all reconstructed
phases for this finite production FIR even when neighbouring gains differ. This is a
gain multiplier on delayed audio, not sample hard clipping.

This finite-filter bound **is not a mathematical guarantee for all continuous-time
reconstructions**. Longer/different filters, frequency response, automation and arbitrary
adversarial waveforms require validation. The 0.3 dB margin is measured with an
independent 16x 96-tap Kaiser reference in tests, not a certification guarantee.
The conservative absolute bound loses phase cancellation and may over-attenuate
high-frequency/sustained audio and create additional gain modulation. Replacing it
with a less conservative predictor is a future quality task; it must retain peak tests.

## Parameters and state

Stable IDs: inputGain, ceiling, oversampling; APVTS schemaVersion=1.
Unavailable modes/clipper are not exposed as fake functionality. Input and ceiling
smooth in dB; OS mix smooths linearly. A falling ceiling settles exponentially and
must not be judged against its final requested value until settled. Parameter snapshots
are read at each host block (sample-offset VST3 automation is not explicitly handled).

Input NaN/Inf becomes zero; extreme finite values are bounded to +/-1e6 before
filters, avoiding contaminated filter histories and overflow. Process callbacks set
ScopedNoDenormals. All vectors/filters initialise in prepare; no locks, I/O or GUI.
Float atomics are asserted lock-free for the target. State serialisation remains host
control work, not processBlock work.

## Metering

Final output feeds a signed 8x FIR TP estimator; safety uses a distinct detector state.
Peak and GR values cross to UI through atomics. The offline reference is separately
implemented at 16x/96 taps with a Kaiser window and zero-padding through its tail.
This follows the oversampled reconstruction approach in BS.1770-5 Annex 2, but does
not claim BS.2217/EBU test-suite conformance. LUFS and integrated gating are future work.

References: https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en and
JUCE modules/juce_dsp/processors/juce_Oversampling.h at the pinned commit.
