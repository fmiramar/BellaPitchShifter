# BelaPhaseVocoder

This project provides SuperCollider UGens that port the phase-vocoder pitch-shifting algorithm from Andrew McPherson's Bela real-time audio programming course (`fft-pitchshift` example).

## UGens

- **`PitchShiftBella`**: A self-contained, audio-rate phase-vocoder pitch shifter. It handles its own FFT windowing, phase advance estimation, bin shifting, phase reconstruction, and overlap-add internally.
- **`PV_PitchShiftBella`**: An FFT-chain version of the same algorithm. It accepts an incoming SuperCollider `FFT` chain, applies the phase-vocoder pitch shifting, and returns a modified PV chain ready for further spectral processing or `IFFT`.

## DSP Strategy

The algorithm estimates the fractional frequency of the signal in each bin by measuring the phase advance between consecutive analysis frames. During the shifting stage, these precise frequencies are multiplied by the pitch ratio and their corresponding magnitudes are mapped into the appropriate target bins. The synthesis phase is then reconstructed by accumulating these frequencies over time, preserving phase continuity. 

Unlike a granular pitch shifter (like SuperCollider's `PitchShift`), this uses spectral phase-vocoder techniques. It also improves on direct bin remapping by magnitude-weighting shifted frequency collisions when multiple analysis bins land in the same synthesis bin.

## Building

```bash
cmake -S . -B build -DSC_PATH="/path/to/supercollider"
cmake --build build --config Release
cmake --install build --config Release
```

## Source and License

The underlying DSP strategy is derived from the Bela public phase-vocoder lecture and `fft-pitchshift` code example. This project is licensed under the GPL-3.0-or-later License.
