# nkblimit

C++20 / JUCE 8 stereo mastering limiter foundation for Windows x64 VST3.
**Phase 1 development build, version 0.1.0. This is not a production mastering release.**

Implemented: real audio processing in float/double, Input Gain (-12..24 dB), Output
Ceiling (-3..0 dBTP), Transparent lookahead limiter, adaptive release, stereo link,
4x/8x FIR oversampling, post-downsampling conservative peak safety, state save/restore,
automatable parameters, functional native JUCE controls and actual output meters.

Punch, Loud, Clipper, LUFS, GR history, final knob artwork and certified BS.1770
metering are not implemented. They are intentionally absent from the public parameters.
The safety guard can attenuate appreciably below the nominal ceiling. Listening tests
and independent conformance tests must precede mastering/release claims.

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
Insiders, MSVC 19.51, SDK 10.0.26100.0. See docs/validation.md for actual results.

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

The UI displays latest audio-block peaks and a production 8x FIR TP estimate. It
does not yet provide peak hold, LUFS, or certified metering. Meter readings at a
30 Hz UI refresh can miss short blocks; the offline tests inspect the entire stream.
GR control estimates the selected/mixed path envelope plus safety gain, not input/output
peak differences across latency; filter delays mean it is not an exact sample-aligned ratio.

## Licence

Original project code: **AGPL-3.0-only**, full text in LICENSE. JUCE modules are used
under their AGPLv3 option, not the commercial licence. See docs/licensing.md and
ThirdParty/JUCE/LICENSE.md. Distributing binaries requires providing the corresponding
source and required notices under the applicable licence terms. No GitHub push or
publication has been performed.
