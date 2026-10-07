# PV_PitchShiftBela

This project provides `PV_PitchShiftBela`, a standalone SuperCollider UGen that ports the public Bela phase-vocoder pitch-shifting example associated with Andrew McPherson's C++ real-time audio programming materials.

## UGen

- `PV_PitchShiftBela`: A self-contained phase-vocoder pitch shifter based on Bela's `fft-pitchshift` example. It estimates fractional bin frequencies from phase advance, shifts them by a pitch ratio, reconstructs output phase, and overlap-adds inverse FFT frames inside the UGen.

## DSP Strategy

The port uses SuperCollider's FFT backend rather than Bela's Fft wrapper, but keeps the same broad analysis-frequency to synthesis-phase structure. Compared with SuperCollider's `PitchShift`, this is spectral and phase-vocoder based instead of granular; compared with `PV_BinShift`, it is a complete audio-rate effect rather than an FFT-chain transform.

The first implementation improves bin-collision handling by magnitude-weighting shifted frequency estimates when several analysis bins land in the same synthesis bin. That keeps the strongest partials from being overwritten by later bins and gives more stable phase reconstruction than a direct last-writer bin remap.

## Building

```bash
cmake -S . -B build -DSC_PATH="../../chow dsp to sc codex/supercollider-3.14.1"
cmake --build build --config Release
cmake --install build --config Release
```

## Source Notes

The immediate source is Bela's public phase-vocoder lecture and `fft-pitchshift` code example. SuperCollider comparisons are `PitchShift`, `PV_BinShift`, and related PV-chain UGens.

## License

This project is licensed under the GPL-3.0 License. It builds upon educational examples from Bela.io (typically CC BY-SA or LGPL/GPL) and links against the SuperCollider plugin API (GPL-3.0).
