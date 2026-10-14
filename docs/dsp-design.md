# DSP architecture and decisions — 0.1.3

Current integrated results are in [validation-0.1.3.md](validation-0.1.3.md).
Earlier validation files and [truepeakguard-design.md](truepeakguard-design.md)
retain historical 0.1.0/0.1.1 measurements; they do not describe the current guard.

## Signal path

Sanitisation -> smoothed input gain -> parallel 4x/8x high-quality linear-phase FIR
oversamplers -> Transparent lookahead engines -> downsampling -> latency alignment
-> smoothed OS crossfade -> first base-rate reconstruction guard -> second
base-rate guard inspecting the first stage's actual output -> output metering.

Source/DSP has no editor dependency. The core requires only JUCE DSP for resampling;
the envelope, predictor, buffers and safety logic are independently readable C++.
No DC blocker is included: removing DC changes sound. DC is instead a test signal.

## Transparent

`ceil(fs*0.0025)` base-rate samples of lookahead, scaled exactly by OS factor.
Both channels share a single dB attenuation. Two preallocated monotonic deques
observe the lookahead interval, including both endpoints. The conservative path
holds the full-window demand. The predictive path ramps each upcoming sample's
required attenuation by 16 dB over the lookahead and reaches its exact demand when
that sample exits the delay. A crest-dependent blend retains the conservative
path for tones while reducing early ducking before isolated transients. Prediction
supplies the attack bound; the old additional 0.1 ms attack filter was removed.
An exact linked sample cap remains for numerical rounding and ceiling changes.

AdaptiveRelease uses a 100 ms decaying peak envelope over 50 ms smoothed energy,
rather than dividing RMS by the instantaneous sample. Its normalized crest blend
runs from zero at 1.65 to one at 2.80, selecting a peak-path release from 200 ms for
tonal content to 6 ms for high-crest transients. A separate sustained path follows
60% of the requested attenuation with 15 ms attack and 90 ms release. The stronger
path supplies the gain. Both release paths work in dB; increasing input drive no
longer slows recovery simply because the attenuation is deeper.

The isolated decaying-kick diagnostic recovers to within 0.5 dB 43.42 ms after a
60 ms burst, versus 447.09 ms in 0.1.1, without increasing the settled 60 Hz tone
distortion in that test. Mixed-frequency signals can still exhibit gain modulation.
See [envelope-revision.md](envelope-revision.md) for measurements, regression limits,
and the distinction between main-stage and complete-plugin results.
This control law is not a claim of subjective transparency.
The 0.1.3 energy window increases from 25 to 50 ms to reduce rapid gain
variation on mixed transient/sustained material. The release endpoints and
prediction ramp remain unchanged. The approved music preserves mean loudness
while reducing short-window source-relative gain variation across three excerpts.
Periodic held-bed tests also show reduced 50 ms variation and kick-rate AM,
but some 400 ms windows and heavily limited periodic recovery become worse.
This is a measured tradeoff, not a claim of pumping elimination or improved
transient preservation. See [independent quality audit](quality-validation-0.1.3.md).
Punch/Loud must later use distinct control topologies rather than preset constants.

## Resampling and latency

JUCE's maximum-quality equiripple half-band FIR stages, integer latency enabled.
Linear phase is selected for predictable alignment; pre-ringing needs listening
evaluation. The higher latency determines the common delay; a preallocated stereo
delay compensates the shorter path. Both run continuously, so switching needs no
allocation or cold filter start. This costs more than running only the selected path.
The reported latency is `max(OS FIR latency) + ceil(fs*2.5ms) + 382` samples.
At 48 kHz this is 65 + 120 + 382 = **567 samples**, or **11.8125 ms**, for either
OS setting. The final guard's second stage adds 191 samples compared with 0.1.1.
Sample rate changes require host prepareToPlay. Larger/unusual blocks are subdivided
into preallocated chunks with continuous state. No process-block-size allocation.
MSVC source/execution character encoding is explicitly UTF-8, preserving JUCE's
Unicode source files on Japanese Windows installations.

## Post-downsampling safety

The 0.1.0 absolute FIR contribution bound lost signal phase cancellation, causing
unnecessary attenuation and poor loudness. The 0.1.1 signed predictor improved
that behavior but required a blanket 0.5 dB reserve. It caused output near -1.0 dBTP
when the requested ceiling was -0.5 dBTP. Reducing that reserve alone failed
adversarial reconstruction tests.

Version 0.1.2 uses two linked signal-aware reconstruction stages. The first predicts
with 128 taps / 16 phases. The second reconstructs the first stage's actual
gain-modulated output with 128 taps / 8 phases, correcting remaining peaks rather
than relying on the old large reserve. Each uses normalized beta-10 Kaiser sinc
reconstruction, a 128-sample support maximum, a 64-sample linear forecast attack,
and 32 ms release. Each delays 191 base-rate samples; the combined guard delay is
382 samples. The displayed guard gain aligns the first reduction to the samples
leaving the second stage.

The first and second stages reserve 0.02 and 0.06 dB respectively. Each targets the
requested ceiling, so these figures must not be summed into a claimed unconditional
0.08 dB output reduction. Saturated steady tones measure approximately 0.06 dB
below the ceiling. On the complete approved music file, 8x / -0.5 dBTP, independent
True Peak measures -0.546 dBTP at +16 dB drive and -0.542 dBTP at +24 dB drive.
The reserves were selected with independent 16-phase/96-tap and 20-phase/128-tap
adversarial measurements; tested worst excess remains within the +0.1 dB criterion.
They are tested headroom, not an all-input mathematical guarantee.

Every original tap and phase is evaluated in double precision. SSE2 parallelizes
the sums without a reduced-quality filter or fast-math alternative; scalar/vector
comparisons are covered by CeilingTrackingTests. Neither stage clips individual
samples or changes the displayed TP value. See [ceiling-tracking.md](ceiling-tracking.md)
for exact guard measurements and CPU scope.

This changes the safety strategy from a conservative finite-filter bound to a
signal-aware predictor. Independent 16x/96-tap and additional 20x/128-tap
reconstruction tests, the full DSP matrix and
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
