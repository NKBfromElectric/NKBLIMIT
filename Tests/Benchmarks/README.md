# Performance status

DSPTests prints the largest short-run real-time factor (elapsed processing time /
audio duration) across its numerical matrix. It includes both prepared 4x/8x paths,
the conservative safety FIR, and the output meter. Smaller is faster; values below
1 processed faster than the audio duration on that run.

This is not a sustained CPU benchmark and is not a Cubase CPU-percentage reading.
No performance optimisation claim follows from it. Before Phase 2 completion,
add sustained, warmed runs at each sample rate/block size, report hardware details
and timing percentiles, compare the separate 4x/8x engines, and measure worst-case
host callback deadlines under UI and multi-instance stress.
