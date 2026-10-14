# DSP architecture and decisions — 0.1.1

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
The reported latency is `max(OS FIR latency) + ceil(fs*2.5ms) + 191` samples.
Sample rate changes require host prepareToPlay. Larger/unusual blocks are subdivided
into preallocated chunks with continuous state. No process-block-size allocation.
MSVC source/execution character encoding is explicitly UTF-8, preserving JUCE's
Unicode source files on Japanese Windows installations.

## Post-downsampling safety

The 0.1.0 absolute FIR contribution bound lost signal phase cancellation, causing
unnecessary attenuation and poor loudness. In 0.1.1, signed reconstruction uses a
128-tap / 8-phase normalised Kaiser sinc predictor, a 128-sample support maximum,
64-sample forecast attack ramp and 32 ms release. Both channels share the gain.
The total guard delay is 191 base samples. A 0.5 dB measured margin covers tested
gain-modulation/filter differences. It is a predictive gain stage, not a clipper.
See truepeakguard-design.md for details, adversarial results and measured CPU cost.

This changes the safety strategy from a conservative finite-filter bound to a
signal-aware predictor. It is not an all-input mathematical guarantee. Independent
16x/96-tap and additional 20x/128-tap reconstruction tests, the full DSP matrix and
the supplied music file are used to validate the target, rather than claiming a proof.

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

Final output feeds a signed 8x FIR TP estimator and independent stereo LoudnessMeter.
Safety uses a different reconstruction filter. Peak and GR interval maxima cross to
UI through lock-free CAS/exchange atomics; loudness uses atomic value/readiness.
UI reset epochs clear held values. Numerical text updates at 5 Hz, peak holds are
1 s, bars repaint at 30 Hz. These display ballistics never affect audio processing.
The offline reference is separately
implemented at 16x/96 taps with a Kaiser window and zero-padding through its tail.
This follows the oversampled reconstruction approach in BS.1770-5 Annex 2, but does
not claim BS.2217/EBU test-suite conformance. Output LUFS-S is an exact 3 s ungated
K-weighted energy window; see loudness-design.md. Integrated gating is future work.

References: https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en and
JUCE modules/juce_dsp/processors/juce_Oversampling.h at the pinned commit.
