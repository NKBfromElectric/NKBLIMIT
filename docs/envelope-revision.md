# Main limiter envelope revision

The 0.1.1 main limiter could remain several dB below unity for hundreds of milliseconds after a kick. Its release used the RMS divided by the *current sample peak*, which labelled every zero crossing and quiet post-transient bed as sustained material. A gain-reduction-dependent time constant made a larger input boost recover more slowly. That could make +24 dB slightly quieter than +16 dB and flatten the body following a transient.

This revision changes the linked main gain control. The oversampling filter, final true-peak guard, parameters, and reported latency are outside this change.

## Gain control

1. Convert each linked peak's required attenuation to dB. Maintain both the conservative full lookahead maximum and a predictive maximum whose attenuation rises by 16 dB over the existing lookahead. Each predictive constraint reaches its exact attenuation when its corresponding sample exits the delay. A preallocated monotonic deque evaluates the constraints without callback allocation.
2. Estimate crest from a 100 ms decaying peak envelope and 25 ms smoothed energy. This statistic uses the signal shape rather than the instantaneous sample denominator or the gain-reduction depth. Its normalized blend is zero at crest 1.65 and one at crest 2.80.
3. For low-crest, tonal signals, retain the conservative lookahead maximum and a 200 ms release. For high-crest transients, blend toward the predictive ramp and a 6 ms release. Intermediate signals interpolate smoothly between them.
4. Release the peak attenuation in dB. A second sustained attenuation path follows 60% of the requested reduction with 15 ms attack and 90 ms release. Apply the stronger path, preserving a modest sustained reduction while recovering transient-only reduction sooner.

The attack bound is supplied by prediction. Adding the old attack smoother would lag that bound. Both detector paths include the attenuation required by the outgoing sample, and their blend remains bounded by that requirement. The final direct sample constraint remains for rounding error and ceiling changes. Stereo channels receive identical gain; no waveshaper or clipping stage was added.

For a fixed signal shape and ceiling, the crest detector is invariant to input gain above its numerical silence floor. The dB demand and envelope operators are monotone and do not amplify the change in required attenuation beyond the input gain change. Regression coverage checks that increasing drive does not reduce individual output magnitudes or total power for the same synthetic music. This property describes the main stage; the complete plugin must also be measured with its final guard.

## Measurements

Stage-only measurements below exclude oversampling filters and the final true-peak guard. They are diagnostic measurements, not final VST3 results or claims of Ozone parity.

A quiet 997 Hz bed plus one 60 ms decaying 60 Hz kick, at 384 kHz and -0.5 dB sample ceiling:

| Measurement | 0.1.1 main stage | Revised main stage |
| --- | ---: | ---: |
| Recovery to within 0.5 dB after the kick ends | 447.09 ms | 43.42 ms |
| Remaining attenuation 100 ms from kick onset | 4.522 dB | 0.519 dB |
| Remaining attenuation 200 ms from kick onset | 2.636 dB | 0.171 dB |
| 60 Hz THD+N, steady 2 dB limiting | 0.09555% | 0.08429% |
| 60 Hz THD+N, steady 6 dB limiting | 0.15661% | 0.13530% |
| 60 Hz THD+N, steady 10 dB limiting | 0.14487% | 0.13656% |

An isolated 4.0-amplitude peak above a quiet constant bed leaves 0.343 dB attenuation 30 ms after its delayed output, versus 9.780 dB previously. Bass distortion checks after settling, at 48 kHz and approximately 12.5 dB attenuation, measure 0.754% harmonic distortion at 30 Hz and 0.414% at 40 Hz, versus 0.879% and 0.453% previously. A separate 60+997 Hz two-tone residual proxy rises modestly from 0.396% to 0.452% at 10 dB limiting; the faster transient recovery is not a guarantee of better sound for every signal.

The authorized stereo 48 kHz source, `カタフラクト_mix_2026-09-25.wav`, was read without modifying it. The 60–90 s excerpt was processed directly through the main stage with the same 120-sample lookahead and -0.5 dB sample ceiling. Median 3 s ungated K-weighted loudness, sampled every 100 ms after the first complete window:

| Input gain | 0.1.1 main stage LUFS-S | Revised main stage LUFS-S | Change |
| --- | ---: | ---: | ---: |
| +8 dB | -8.739 | -7.553 | +1.185 LU |
| +12 dB | -7.998 | -6.396 | +1.602 LU |
| +16 dB | -7.982 | -6.081 | +1.900 LU |
| +20 dB | -8.079 | -6.033 | +2.046 LU |
| +24 dB | -8.157 | -6.030 | +2.127 LU |

All of these main-stage outputs reach -0.5 dB sample peak. These values must not be presented as the complete plugin's true-peak or loudness results. The full plugin adds filtering and final true-peak limiting, and uses a longer music measurement elsewhere in validation.

## Regression coverage and limits

`LimiterBehaviourTests` checks float/double exact sample bounds; 0-, 1-, and normal lookahead lengths; finite input handling; unchanged delay; complete reset; transient recovery at 44.1, 48, 96, and 384 kHz; monotonic drive from +4 to +24 dB; and settled 30/60/997 Hz distortion at 48 and 384 kHz. The standalone MSVC Release build passed. Existing processor, host, true-peak, and plugin validation still belong to integration validation.

The signal blend can recover drums much sooner while holding a steady bass tone. It can still modulate mixed low- and high-frequency content under strong limiting. Driving already peak-normalized material harder eventually approaches a plateau. Loudness, distortion, transient shape, and perceived crushing remain a tradeoff; a final listening decision requires level-matched musical auditions. Direct comparison against an Ozone-rendered reference remains outstanding.
