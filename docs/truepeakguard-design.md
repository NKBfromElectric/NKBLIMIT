# True-peak guard revision (0.1.1)

The 0.1.0 guard used the sum of the absolute contributions of every reconstruction FIR tap. That bound is safe under arbitrary gain changes, but throws away cancellation between signed samples. It consequently reduces signals whose actual reconstructed peak is already near the ceiling.

A standalone 48 kHz, 997 Hz sine comparison, using a 2 FS input and a -0.5 dBTP requested ceiling, measured:

| Guard | Steady output sample peak | Change in steady RMS |
| --- | ---: | ---: |
| Original absolute-contribution guard | -7.70032 dBFS | Reference |
| Revised signed guard | -1.00000 dBFS | +6.70040 dB |

This is a numerical guard comparison. It does not establish listening quality or parity with another limiter.

## Current implementation

- Predict signed stereo reconstruction with a 128-tap, eight-phase Kaiser-windowed sinc interpolator (beta 10), also checking direct sample peaks.
- Hold the maximum prediction for the complete 128-sample reconstruction support.
- Add 64 samples of lookahead so a linear gain attack reaches each predicted constraint before the constrained samples emerge.
- Apply the same gain to both channels; release over a 32 ms time constant.
- Reserve 0.5 dB below the requested ceiling for gain modulation and differences between reconstruction filters.
- Store inverse predicted peaks rather than already-clipped gains. This preserves unity gain for quiet signals during a ceiling change.
- Use mirrored preallocated histories so inner FIR and forecast loops do not perform circular modulo operations. `process` and `reset` allocate no memory.

Guard latency is 191 base-rate samples, compared with 31 before this revision. At 48 kHz this adds 3.333 ms to the old total plugin latency. The oversampling engine obtains the reported guard latency from the constant and the host must use the newly reported total.

## Numerical validation

`TruePeakGuardTests.cpp` runs 768 cases across float/double, 44.1/48/88.2/96/192 kHz, and -3/-0.5/0 dB ceilings. Its deterministic inputs include random-frequency tones, abrupt amplitude bursts, impulses, DC polarity changes, noise, multitone signals, near-Nyquist bursts at 0.499 times the sample rate, and chirps. Both channels are checked, including independent-channel input and constant stereo ratios. Additional checks verify no allocations in processing/reset, unity gain below threshold during a ceiling change, the reported delay, and useful steady-state loudness.

Against the separately implemented offline 96-tap, 16-phase Kaiser beta-9 reference, the maximum excess across these 768 cases was **-0.014168 dB**. All cases met the required ceiling + 0.1 dB limit.

An additional scratch audit of 384 double-precision adversarial cases used a longer 128-tap, 20-phase beta-9 reference. Maximum excess was **-0.0129997 dB**. The production predictor uses eight phases and beta 10, so neither reference reuses production coefficients or history code.

The 0.5 dB margin was selected after measurement. A 0.25 dB margin with the same 128-tap predictor exceeded ceiling + 0.1 dB in six of the 384 isolated adversarial cases, reaching +0.235832 dB. Reducing the margin would require further signal-aware protection and validation.

An optimized guard-only loop processing 1,920,000 double-precision stereo frames measured 1.666 seconds, corresponding to a real-time factor of 0.167 at 192 kHz. The earlier unoptimized 96-tap/16-phase candidate measured 3.894 seconds for that loop. These scratch timings exclude the oversampling limiter, host and GUI; they are not a DAW CPU benchmark.

## Limits

The revised guard is a signal-aware predictor with tested headroom, not an all-input mathematical reconstruction bound. Its gain changes modify the waveform on which the prediction was based. The lookahead, support hold, linear attack and margin reduce this risk, but a finite test corpus cannot establish a universal guarantee. Longer interpolation filters, different reconstruction kernels and untested pathological signals may yield different numerical peaks. BS.1770/BS.2217 metering conformance and audio-quality comparisons remain separate work.
