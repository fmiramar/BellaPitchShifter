#include "FFT_UGens.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <new>

InterfaceTable* ft;

namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

enum InputIndex
{
    InputIn = 0,
    InputPitchRatio,
    InputFFTSize,
    InputHopSize
};

struct PitchShiftBella : public Unit
{
    int fftSize = 1024;
    int halfSize = 512;
    int numBins = 513;
    int hopSize = 128;
    int bufferSize = 16384;
    int inputPointer = 0;
    int outputReadPointer = 0;
    int outputWritePointer = 1280;
    int hopCounter = 0;

    float pitchRatio = 1.0f;
    float scale = 0.125f;

    float* inputBuffer = nullptr;
    float* outputBuffer = nullptr;
    float* analysisWindow = nullptr;
    float* synthesisWindow = nullptr;
    float* timeInput = nullptr;
    float* timeOutput = nullptr;
    float* fftBuffer = nullptr;
    float* lastInputPhases = nullptr;
    float* lastOutputPhases = nullptr;
    float* analysisMagnitudes = nullptr;
    float* analysisFrequencies = nullptr;
    float* synthesisMagnitudes = nullptr;
    float* synthesisFrequencySums = nullptr;

    scfft* forwardFFT = nullptr;
    scfft* inverseFFT = nullptr;
};

struct PV_PitchShiftBella : public Unit
{
    int fftSize = 0;
    int halfSize = 0;
    int numBins = 0;
    int hopSize = 0;
    int samplesSinceFrame = 0;
    float pitchRatio = 1.0f;
    float previousBuffer = -1.0f;
    float* state = nullptr;
    float* fftBuffer = nullptr; // Borrowed from the incoming chain for this frame.
    float* lastInputPhases = nullptr;
    float* lastOutputPhases = nullptr;
    float* analysisMagnitudes = nullptr;
    float* analysisFrequencies = nullptr;
    float* synthesisMagnitudes = nullptr;
    float* synthesisFrequencySums = nullptr;
};

inline int nextPowerOfTwo(int value) noexcept
{
    int result = 1;
    while (result < value)
        result <<= 1;
    return result;
}

inline int clampPowerOfTwoFFTSize(float input) noexcept
{
    int size = nextPowerOfTwo(std::max(256, static_cast<int>(input + 0.5f)));
    return std::clamp(size, 256, 8192);
}

inline int clampHopSize(float input, int fftSize, int blockSize) noexcept
{
    int hop = std::max(1, static_cast<int>(input + 0.5f));
    return std::clamp(hop, std::max(1, blockSize), std::max(1, fftSize / 2));
}

inline float clampPitchRatio(float ratio) noexcept
{
    if (!std::isfinite(ratio))
        return 1.0f;
    return std::clamp(ratio, 0.125f, 8.0f);
}

inline float wrapPhase(float phase) noexcept
{
    if (phase >= 0.0f)
        return std::fmod(phase + pi, twoPi) - pi;
    return std::fmod(phase - pi, -twoPi) + pi;
}

inline float magnitudeAt(const float* fftBuffer, int bin, int halfSize) noexcept
{
    if (bin == 0)
        return std::abs(fftBuffer[0]);
    if (bin == halfSize)
        return std::abs(fftBuffer[1]);

    const float real = fftBuffer[bin * 2];
    const float imag = fftBuffer[bin * 2 + 1];
    return std::sqrt(real * real + imag * imag);
}

inline float phaseAt(const float* fftBuffer, int bin, int halfSize) noexcept
{
    if (bin == 0)
        return fftBuffer[0] < 0.0f ? pi : 0.0f;
    if (bin == halfSize)
        return fftBuffer[1] < 0.0f ? pi : 0.0f;

    return std::atan2(fftBuffer[bin * 2 + 1], fftBuffer[bin * 2]);
}

template <typename BellaUnit>
void clearComplexSpectrum(BellaUnit* unit) noexcept
{
    std::memset(unit->fftBuffer, 0, static_cast<size_t>(unit->fftSize) * sizeof(float));
}

template <typename BellaUnit>
void setSpectrumBin(BellaUnit* unit, int bin, float magnitude, float phase) noexcept
{
    if (bin == 0) {
        unit->fftBuffer[0] = magnitude * std::cos(phase);
        return;
    }

    if (bin == unit->halfSize) {
        unit->fftBuffer[1] = magnitude * std::cos(phase);
        return;
    }

    unit->fftBuffer[bin * 2] = magnitude * std::cos(phase);
    unit->fftBuffer[bin * 2 + 1] = magnitude * std::sin(phase);
}

// Shared by the audio effect and the PV-chain effect. Analysis, bin mapping,
// collision weighting, and phase reconstruction retain the original arithmetic.
template <typename BellaUnit>
void processSpectrum(BellaUnit* unit) noexcept
{
    const int fftSize = unit->fftSize;
    const int halfSize = unit->halfSize;
    const int hopSize = unit->hopSize;
    const float ratio = clampPitchRatio(unit->pitchRatio);

    for (int bin = 0; bin <= halfSize; ++bin) {
        const float amplitude = magnitudeAt(unit->fftBuffer, bin, halfSize);
        const float phase = phaseAt(unit->fftBuffer, bin, halfSize);
        float phaseDiff = phase - unit->lastInputPhases[bin];
        const float binCentre = twoPi * static_cast<float>(bin) / static_cast<float>(fftSize);

        phaseDiff = wrapPhase(phaseDiff - binCentre * static_cast<float>(hopSize));
        const float binDeviation = phaseDiff * static_cast<float>(fftSize) / (static_cast<float>(hopSize) * twoPi);

        unit->analysisMagnitudes[bin] = amplitude;
        unit->analysisFrequencies[bin] = static_cast<float>(bin) + binDeviation;
        unit->lastInputPhases[bin] = phase;
    }

    std::memset(unit->synthesisMagnitudes, 0, static_cast<size_t>(unit->numBins) * sizeof(float));
    std::memset(unit->synthesisFrequencySums, 0, static_cast<size_t>(unit->numBins) * sizeof(float));

    for (int bin = 0; bin <= halfSize; ++bin) {
        const float shiftedFrequency = unit->analysisFrequencies[bin] * ratio;
        const int newBin = static_cast<int>(std::floor(shiftedFrequency + 0.5f));

        if (newBin >= 0 && newBin <= halfSize) {
            const float magnitude = unit->analysisMagnitudes[bin];
            unit->synthesisMagnitudes[newBin] += magnitude;
            unit->synthesisFrequencySums[newBin] += shiftedFrequency * magnitude;
        }
    }

    clearComplexSpectrum(unit);

    for (int bin = 0; bin <= halfSize; ++bin) {
        const float magnitude = unit->synthesisMagnitudes[bin];
        if (magnitude <= 0.0f) {
            unit->lastOutputPhases[bin] = wrapPhase(unit->lastOutputPhases[bin]);
            continue;
        }

        const float synthesisFrequency = unit->synthesisFrequencySums[bin] / magnitude;
        const float binCentre = static_cast<float>(bin);
        const float binDeviation = synthesisFrequency - binCentre;

        float phaseDiff = binDeviation * twoPi * static_cast<float>(hopSize) / static_cast<float>(fftSize);
        phaseDiff += twoPi * binCentre * static_cast<float>(hopSize) / static_cast<float>(fftSize);

        const float phase = wrapPhase(unit->lastOutputPhases[bin] + phaseDiff);
        setSpectrumBin(unit, bin, magnitude, phase);
        unit->lastOutputPhases[bin] = phase;
    }
}

void processFrame(PitchShiftBella* unit) noexcept
{
    const int fftSize = unit->fftSize;
    const int bufferSize = unit->bufferSize;
    for (int i = 0; i < fftSize; ++i) {
        const int index = (unit->inputPointer + i - fftSize + bufferSize) & (bufferSize - 1);
        unit->timeInput[i] = unit->inputBuffer[index] * unit->analysisWindow[i];
    }
    scfft_dofft(unit->forwardFFT);
    processSpectrum(unit);
    scfft_doifft(unit->inverseFFT);

    for (int i = 0; i < fftSize; ++i) {
        const int index = (unit->outputWritePointer + i - fftSize + bufferSize) & (bufferSize - 1);
        unit->outputBuffer[index] += unit->timeOutput[i] * unit->synthesisWindow[i];
    }
}

void PitchShiftBella_next(PitchShiftBella* unit, int inNumSamples)
{
    const float* in = IN(InputIn);
    const float* ratioIn = IN(InputPitchRatio);
    float* out = OUT(0);

    const bool ratioAudioRate = INRATE(InputPitchRatio) == calc_FullRate;
    float ratio = unit->pitchRatio;
    const float nextRatio = ratioAudioRate ? ratio : IN0(InputPitchRatio);
    const float ratioSlope = ratioAudioRate ? 0.0f : CALCSLOPE(nextRatio, ratio);

    for (int i = 0; i < inNumSamples; ++i) {
        if (ratioAudioRate)
            ratio = ratioIn[i];
        else
            ratio += ratioSlope;

        unit->pitchRatio = ratio;
        unit->inputBuffer[unit->inputPointer] = in[i];
        unit->inputPointer = (unit->inputPointer + 1) & (unit->bufferSize - 1);

        float sample = unit->outputBuffer[unit->outputReadPointer] * unit->scale;
        sample = zapgremlins(sample);
        unit->outputBuffer[unit->outputReadPointer] = 0.0f;
        unit->outputReadPointer = (unit->outputReadPointer + 1) & (unit->bufferSize - 1);

        out[i] = sample;

        if (++unit->hopCounter >= unit->hopSize) {
            unit->hopCounter = 0;
            processFrame(unit);
            unit->outputWritePointer = (unit->outputWritePointer + unit->hopSize) & (unit->bufferSize - 1);
        }
    }

    unit->pitchRatio = ratioAudioRate ? ratio : nextRatio;
}

void PitchShiftBella_Dtor(PitchShiftBella* unit)
{
    SCWorld_Allocator alloc(ft, unit->mWorld);

    if (unit->forwardFFT)
        scfft_destroy(unit->forwardFFT, alloc);
    if (unit->inverseFFT)
        scfft_destroy(unit->inverseFFT, alloc);

    RTFree(unit->mWorld, unit->inputBuffer);
    RTFree(unit->mWorld, unit->outputBuffer);
    RTFree(unit->mWorld, unit->analysisWindow);
    RTFree(unit->mWorld, unit->synthesisWindow);
    RTFree(unit->mWorld, unit->timeInput);
    RTFree(unit->mWorld, unit->timeOutput);
    RTFree(unit->mWorld, unit->fftBuffer);
    RTFree(unit->mWorld, unit->lastInputPhases);
    RTFree(unit->mWorld, unit->lastOutputPhases);
    RTFree(unit->mWorld, unit->analysisMagnitudes);
    RTFree(unit->mWorld, unit->analysisFrequencies);
    RTFree(unit->mWorld, unit->synthesisMagnitudes);
    RTFree(unit->mWorld, unit->synthesisFrequencySums);
}

void PitchShiftBella_Ctor(PitchShiftBella* unit)
{
    new (unit) PitchShiftBella;

    unit->fftSize = clampPowerOfTwoFFTSize(IN0(InputFFTSize));
    unit->halfSize = unit->fftSize / 2;
    unit->numBins = unit->halfSize + 1;
    unit->hopSize = clampHopSize(IN0(InputHopSize), unit->fftSize, unit->mBufLength);
    unit->bufferSize = nextPowerOfTwo(unit->fftSize * 16);
    unit->outputWritePointer = (unit->fftSize + 2 * unit->hopSize) & (unit->bufferSize - 1);
    unit->scale = static_cast<float>(unit->hopSize) / static_cast<float>(unit->fftSize);
    unit->pitchRatio = IN0(InputPitchRatio);

    const size_t signalBytes = static_cast<size_t>(unit->bufferSize) * sizeof(float);
    const size_t fftBytes = static_cast<size_t>(unit->fftSize) * sizeof(float);
    const size_t binBytes = static_cast<size_t>(unit->numBins) * sizeof(float);

    unit->inputBuffer = static_cast<float*>(RTAlloc(unit->mWorld, signalBytes));
    unit->outputBuffer = static_cast<float*>(RTAlloc(unit->mWorld, signalBytes));
    unit->analysisWindow = static_cast<float*>(RTAlloc(unit->mWorld, fftBytes));
    unit->synthesisWindow = static_cast<float*>(RTAlloc(unit->mWorld, fftBytes));
    unit->timeInput = static_cast<float*>(RTAlloc(unit->mWorld, fftBytes));
    unit->timeOutput = static_cast<float*>(RTAlloc(unit->mWorld, fftBytes));
    unit->fftBuffer = static_cast<float*>(RTAlloc(unit->mWorld, fftBytes));
    unit->lastInputPhases = static_cast<float*>(RTAlloc(unit->mWorld, binBytes));
    unit->lastOutputPhases = static_cast<float*>(RTAlloc(unit->mWorld, binBytes));
    unit->analysisMagnitudes = static_cast<float*>(RTAlloc(unit->mWorld, binBytes));
    unit->analysisFrequencies = static_cast<float*>(RTAlloc(unit->mWorld, binBytes));
    unit->synthesisMagnitudes = static_cast<float*>(RTAlloc(unit->mWorld, binBytes));
    unit->synthesisFrequencySums = static_cast<float*>(RTAlloc(unit->mWorld, binBytes));

    ClearUnitIfMemFailed(unit->inputBuffer && unit->outputBuffer && unit->analysisWindow && unit->synthesisWindow
                         && unit->timeInput && unit->timeOutput && unit->fftBuffer && unit->lastInputPhases
                         && unit->lastOutputPhases && unit->analysisMagnitudes && unit->analysisFrequencies
                         && unit->synthesisMagnitudes && unit->synthesisFrequencySums);

    std::memset(unit->inputBuffer, 0, signalBytes);
    std::memset(unit->outputBuffer, 0, signalBytes);
    std::memset(unit->timeInput, 0, fftBytes);
    std::memset(unit->timeOutput, 0, fftBytes);
    std::memset(unit->fftBuffer, 0, fftBytes);
    std::memset(unit->lastInputPhases, 0, binBytes);
    std::memset(unit->lastOutputPhases, 0, binBytes);
    std::memset(unit->analysisMagnitudes, 0, binBytes);
    std::memset(unit->analysisFrequencies, 0, binBytes);
    std::memset(unit->synthesisMagnitudes, 0, binBytes);
    std::memset(unit->synthesisFrequencySums, 0, binBytes);

    for (int i = 0; i < unit->fftSize; ++i) {
        const float window = 0.5f * (1.0f - std::cos(twoPi * static_cast<float>(i) / static_cast<float>(unit->fftSize - 1)));
        unit->analysisWindow[i] = window;
        unit->synthesisWindow[i] = window;
    }

    SCWorld_Allocator alloc(ft, unit->mWorld);
    unit->forwardFFT = scfft_create(unit->fftSize, unit->fftSize, kRectWindow, unit->timeInput, unit->fftBuffer,
                                    kForward, alloc);
    unit->inverseFFT = scfft_create(unit->fftSize, unit->fftSize, kRectWindow, unit->fftBuffer, unit->timeOutput,
                                    kBackward, alloc);
    ClearUnitIfMemFailed(unit->forwardFFT && unit->inverseFFT);

    SETCALC(PitchShiftBella_next);
    ClearUnitOutputs(unit, 1);
}

void PV_PitchShiftBella_next(PV_PitchShiftBella* unit, int /*inNumSamples*/)
{
    // PV units run once per server block. Count samples, not control ticks.
    unit->samplesSinceFrame = static_cast<int>(std::min<int64_t>(
        static_cast<int64_t>(unit->samplesSinceFrame) + FULLBUFLENGTH, 0x40000000));
    OUT0(0) = -1.0f;
    const float chain = IN0(0);
    if (!std::isfinite(chain) || chain < 0.0f || chain != std::floor(chain)
        || static_cast<double>(chain) > UINT32_MAX)
        return;

    const auto index = static_cast<uint32_t>(chain);
    auto* world = unit->mWorld;
    SndBuf* buf = nullptr;
    if (index < world->mNumSndBufs) {
        buf = world->mSndBufs + index;
    } else {
        const auto localIndex = index - world->mNumSndBufs;
        auto* parent = unit->mParent;
        if (!parent || !parent->mLocalSndBufs || parent->localBufNum < 0
            || localIndex > static_cast<uint32_t>(parent->localBufNum))
            return;
        buf = parent->mLocalSndBufs + localIndex;
    }
    LOCK_SNDBUF(buf);
    const int size = buf->samples;
    if (!buf->data || buf->channels != 1 || size < 256 || size > 8192 || (size & (size - 1)) != 0)
        return;

    if (unit->fftSize != size) {
        const int bins = size / 2 + 1;
        auto* state = static_cast<float*>(RTAlloc(world, static_cast<size_t>(bins) * 6 * sizeof(float)));
        ClearFFTUnitIfMemFailed(state);
        RTFree(world, unit->state);
        unit->state = state;
        unit->fftSize = size;
        unit->halfSize = size / 2;
        unit->numBins = bins;
        unit->lastInputPhases = state;
        unit->lastOutputPhases = state + bins;
        unit->analysisMagnitudes = state + 2 * bins;
        unit->analysisFrequencies = state + 3 * bins;
        unit->synthesisMagnitudes = state + 4 * bins;
        unit->synthesisFrequencySums = state + 5 * bins;
        std::memset(state, 0, static_cast<size_t>(bins) * 6 * sizeof(float));
    } else if (unit->previousBuffer != chain) {
        std::memset(unit->state, 0, static_cast<size_t>(unit->numBins) * 6 * sizeof(float));
    }

    // Other PV processors may have converted the buffer to polar coordinates.
    ToComplexApx(buf);
    unit->fftBuffer = buf->data;
    unit->pitchRatio = IN0(1);
    unit->hopSize = unit->samplesSinceFrame;
    processSpectrum(unit);
    unit->samplesSinceFrame = 0;
    unit->previousBuffer = chain;
    buf->coord = coord_Complex;
    OUT0(0) = chain;
}

void PV_PitchShiftBella_Ctor(PV_PitchShiftBella* unit)
{
    new (unit) PV_PitchShiftBella;
    SETCALC(PV_PitchShiftBella_next);
    // Downstream IFFT/PV constructors need the buffer identity immediately.
    // Frame processing still starts on the first actual FFT update.
    OUT0(0) = IN0(0);
}

void PV_PitchShiftBella_Dtor(PV_PitchShiftBella* unit)
{
    RTFree(unit->mWorld, unit->state);
}
} // namespace

PluginLoad(BelaPhaseVocoder)
{
    ft = inTable;
    DefineDtorUnit(PitchShiftBella);
    DefineDtorUnit(PV_PitchShiftBella);
}
