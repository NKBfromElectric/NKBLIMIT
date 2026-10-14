# NKB LIMIT UI — Threshold / Ceiling

The 0.1.3 editor uses the requested **NKB LIMIT** branding and two vertical
Threshold/Ceiling faders. Input/output peak meters sit on the outside; output
LUFS-S, True Peak and GR have large readouts above the controls. The arrangement
uses the supplied Ozone screenshot as an operational reference, with independently
drawn JUCE components and no commercial assets.

## Mapping and host compatibility

The existing VST3 ID, parameter IDs (`inputGain`, `ceiling`, `oversampling`) and
saved-state schema remain unchanged. Hosts continue to automate Input Gain in dB.
Threshold is an editor view of the input level at which the main limiter's sample
ceiling is reached:

```text
displayed threshold = ceiling − input gain
input gain = ceiling − displayed threshold
```

At Ceiling −0.5 dBTP, Threshold −7.5 dB corresponds to Input Gain +7 dB. Lowering
Threshold increases drive. The visible range tracks the legacy gain bounds:
`ceiling − 24` through `ceiling + 12`. Positive threshold values represent input
attenuation and retain the earlier negative-gain range.

Changing Ceiling preserves Input Gain and updates the derived Threshold display.
The editor does not write extra Input Gain automation while displaying a Ceiling
change. Threshold and Ceiling are therefore a view of the existing processing
model, rather than a new pair of independently stored DSP parameters. Resetting
Threshold removes input boost (Input Gain 0), making Threshold equal to Ceiling.
Ceiling reset remains −0.5 dBTP. Existing projects restore the same parameter
values and audio-processing settings; the separate DSP revision is documented
in the version's validation report.

`ParameterAttachment` translates Threshold edits and forwards drag begin/part/end
or complete gestures for numeric edits. Parameter changes from the host return to
the editor on the message thread. Ceiling retains its existing slider attachment.
Controls, resets and the 4x/8x selector operate on real processor parameters.

## Readability and verification

The editor is 860×560. Meter numbers still update at 5 Hz, bars at 30 Hz, and
peaks/GR hold for one second. The parameter numbers follow editing and automation;
meter display pacing does not slow or modify audio processing.

`ProcessorTests` exercises the real editor across all combinations of Ceiling
−3/−0.5/0 and Input Gain −12/0/+7/+24, then checks numeric-value conversion,
text parsing with unit suffixes, and one complete host gesture. Multiple Threshold
changes during a drag share one host gesture; closing the editor during the drag
ends that gesture. A Ceiling edit leaves Input Gain unchanged and emits no extra
Input Gain gestures. Existing state restore and float/double checks remain in place.
Both ready and initial metering snapshots are generated from actual JUCE widgets
for visual inspection. These automated checks do not simulate Label editor focus
or Return-key submission. Live Cubase text entry, mouse dragging and HiDPI checks
remain unverified.
