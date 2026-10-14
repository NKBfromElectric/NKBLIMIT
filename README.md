# nkblimit

C++20 / JUCE 8 stereo mastering limiter foundation for Windows x64 VST3.
**Development build, version 0.1.2. This is not a production mastering release.**

Implemented: real audio processing in float/double, Input Gain (-12..24 dB), Output
Ceiling (-3..0 dBTP), Transparent predictive lookahead limiter, separate peak/sustained
dB release paths, stereo link, 4x/8x FIR oversampling, two-stage post-downsampling
predictive peak safety, state save/restore,
automatable parameters, native controls, held peaks and output Short-term LUFS (3 s).

Punch, Loud, Clipper, Integrated LUFS, GR history and certified BS.1770
metering are not implemented. They are intentionally absent from the public parameters.
Version 0.1.2 replaces the 0.1.1 blanket 0.5 dB true-peak reserve with two signed
reconstruction stages. The second measures the first stage's actual gain-modulated
output. Their small 0.02/0.06 dB reserves were selected by independent reconstruction
tests; they are not a meter offset or an unconditional 0.08 dB output attenuation.
The main limiter also recovers transient-only reduction sooner while retaining a
longer release for low-crest tonal signals. See the [current validation](docs/validation-0.1.2.md),
[envelope design](docs/envelope-revision.md), and [ceiling tracking](docs/ceiling-tracking.md).
A direct public-VST3 comparison with the pictured Ozone 9 Elements settings is
recorded in the validation report. Independent conformance testing remains
separate work. Ozone-equivalent sound or performance is not established.

The approved 238.67 s stereo music file, processed through the complete VST3 at 8x
and a -0.5 dBTP ceiling, measured:

| Input gain | 0.1.1 median LUFS-S | 0.1.2 median LUFS-S | 0.1.2 independent True Peak |
| --- | ---: | ---: | ---: |
| +16 dB | -8.410 | -6.270 | -0.546 dBTP |
| +24 dB | -8.558 | -6.169 | -0.542 dBTP |

These are complete 3 s short-term-window statistics, not Integrated LUFS. The finite
test corpus does not prove a universal true-peak bound or subjective transparency.

## Build

Required: Windows 11 x64, Visual Studio C++ x64 tools plus Windows SDK, CMake >=3.25,
Git. This checkout includes JUCE as a pinned submodule. No commercial source or assets.

In an **x64 Native Tools Command Prompt**, with Ninja available:

```bat
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

If Japanese MSVC produces garbled `/showIncludes` dependency prefixes with CMake,
configure with `-DNKBLIMIT_MSVC_INCLUDE_PREFIX="メモ: インクルード ファイル:  "`.
This machine needs that option for reliable incremental header rebuilds.

Visual Studio generator alternative:

```bat
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release --parallel 4
ctest --test-dir build-vs -C Release --output-on-failure
```

For VS2026 use its corresponding generator. The development machine uses VS2026
Insiders, MSVC 19.51, SDK 10.0.26100.0. See docs/validation-0.1.2.md for actual results.

Output: `build/nkblimit_artefacts/Release/VST3/nkblimit.vst3` (a directory bundle).
Copy the entire bundle to a user-selected VST3 location and rescan in Cubase.
Builds do not automatically install or modify DAW settings.

JUCE is fixed to **8.0.15**, commit
`91ad83ae34a81e0833b1a2b0866f54846370ae53`. CMake verifies a local checkout's commit;
without it, FetchContent obtains that exact commit.
The small source ZIP excludes JUCE itself; skip the submodule command when using
that archive outside Git. CMake obtains the pinned source through FetchContent.
The full local repository already contains the JUCE checkout for offline builds.

## Controls and measurement

Drag knobs; Shift enables fine adjustment. Double-click the numeric field to type
an exact value. Separate reset buttons restore defaults. OS selection crossfades
between prepared, latency-aligned paths. True Peak safety is always enabled.
Ceiling/Input automation is smoothed over a 20 ms exponential time constant; a new
ceiling is not an instantaneous hard step. Wait at least 200 ms before assessing
the final ceiling after automation. No bypass parameter is added in this phase.
The ceiling is a limiting target, not a command to normalize every passage to that
peak. Saturated steady tones measure about 0.06 dB below it with the current guard.
At 48 kHz the reported total latency is 567 samples (11.8125 ms): 65 samples of
maximum FIR path delay, 120 samples of main lookahead, and 382 samples of guard delay.

The UI captures all audio-block maxima between refreshes. Numerical readouts update
5 times/second, peaks hold for 1 s, and bars move at 30 Hz with gradual decay.
LUFS-S measures actual final stereo output through K weighting over an exact,
ungated 3 s window. It shows Measuring during the first 3 s after prepare/reset.
Measurement has no additional smoothing; only numerical display refresh is slowed.
This does not claim full EBU Mode or certified metering.
GR control estimates the selected/mixed path envelope plus safety gain, not input/output
peak differences across latency; filter delays mean it is not an exact sample-aligned ratio.

## Licence

Original project code: **AGPL-3.0-only**, full text in LICENSE. JUCE modules are used
under their AGPLv3 option, not the commercial licence. See docs/licensing.md and
ThirdParty/JUCE/LICENSE.md. Distributing binaries requires providing the corresponding
source and required notices under the applicable licence terms. No GitHub push or
publication has been performed.
