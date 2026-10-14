# Licensing review — 2026-10-14

Decision: AGPL-3.0-only for original NKBLIMIT code (LICENSE contains the official
GNU AGPLv3 text from https://www.gnu.org/licenses/agpl-3.0.txt).

JUCE 8.0.15's own LICENSE.md explicitly offers its modules under AGPLv3 or the
commercial JUCE licence. This project selects AGPLv3, consistent with the requested
open-source distribution. No alternative proprietary licence is being selected.

Pinned source: https://github.com/juce-framework/JUCE/blob/8.0.15/LICENSE.md
and commit 91ad83ae34a81e0833b1a2b0866f54846370ae53.

The bundled VST3 SDK in this version is MIT licensed (Copyright 2025 Steinberg),
not the licensing of an older SDK assumed from memory. The actual SDK LICENSE.txt
is retained in ThirdParty/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/.
The MIT option is compatible with inclusion in this AGPL project while retaining
its copyright and permission notices.

JUCE includes additional dependencies with their own notices (fonts, image codecs,
compression, etc.). Those remain in the pinned submodule. A copied VST3 bundle is
accompanied by third-party-notices and source/build instructions in this delivery.
Commercial plugins are comparison targets only; no commercial code/assets were used.

Before an actual public release, include the corresponding complete source,
dependency commit and build instructions with the AGPL notices, preserve third-party
licences, and review distribution obligations for that release. This review does not
substitute for legal advice on a particular distribution model.
