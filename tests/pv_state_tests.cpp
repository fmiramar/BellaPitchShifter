#include "../src/BelaPhaseVocoder.cpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

bool failAllocation = false;
void* allocate(World*, size_t size) { return failAllocation ? nullptr : std::malloc(size); }
void release(World*, void* ptr) { std::free(ptr); }
int quietPrint(const char*, ...) { return 0; }

struct Fixture {
    World world {};
    Graph graph {};
    SndBuf buffer {};
    PV_PitchShiftBella unit {};
    std::array<float, 8192> spectrum {};
    float chain = 0;
    float ratio = 2;
    float output = 0;
    float* inputs[2] {&chain, &ratio};
    float* outputs[1] {&output};

    Fixture()
    {
        world.mFullRate.mBufLength = 64;
        world.mNumSndBufs = 1;
        world.mSndBufs = &buffer;
        graph.localBufNum = -1;
        buffer.data = spectrum.data();
        buffer.samples = 1024;
        buffer.channels = 1;
        buffer.coord = coord_Complex;
        unit.mWorld = &world;
        unit.mParent = &graph;
        unit.mInBuf = inputs;
        unit.mOutBuf = outputs;
        PV_PitchShiftBella_Ctor(&unit);
    }
    ~Fixture() { PV_PitchShiftBella_Dtor(&unit); }
    void next()
    {
        // SC's SETCALC casts the typed callback to UnitCalcFunc. Invoke it with
        // its original type here so UBSan can check the DSP without flagging
        // the SDK's standard callback cast. The real ABI is covered by NRT.
        if (unit.mCalcFunc == reinterpret_cast<UnitCalcFunc>(&PV_PitchShiftBella_next))
            PV_PitchShiftBella_next(&unit, 1);
        else
            unit.mCalcFunc(&unit, 1);
    }
};

void timingAndMapping()
{
    Fixture f;
    require(f.output == 0 && f.unit.state == nullptr, "constructor forwards buffer identity without processing");
    f.chain = -1;
    for (int i = 0; i < 3; ++i) f.next();
    require(f.output == -1 && f.unit.state == nullptr, "idle chain must not allocate or advertise a frame");
    f.chain = 0;
    // Bin 8 advances by an exact number of cycles at a 256-sample hop.
    f.spectrum[16] = 1;
    f.next();
    require(f.output == 0 && f.unit.hopSize == 256, "derive first hop from FFT update timing");
    require(std::abs(f.spectrum[32]) > 0.99f, "ratio two moves bin 8 to bin 16");
    require(f.spectrum[16] == 0, "original bin was cleared");
    const auto saved = f.spectrum;
    f.chain = -1;
    f.next();
    require(f.output == -1 && f.spectrum == saved, "no-frame tick leaves FFT storage untouched");
    f.chain = 0;
    f.buffer.samples = 2048;
    f.spectrum.fill(0);
    f.next();
    require(f.unit.numBins == 1025 && f.unit.hopSize == 128, "size changes allocate matching state");
    f.graph.mLocalSndBufs = &f.buffer;
    f.graph.localBufNum = 0;
    f.chain = 1;
    f.next();
    require(f.output == 1 && f.unit.previousBuffer == 1, "accept synth-local buffers");
    f.buffer.coord = coord_Polar;
    f.next();
    require(f.buffer.coord == coord_Complex, "polar input produces a complex chain");
}

void invalidBuffers()
{
    Fixture f;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    for (const float value : {nan, inf, -inf, -1.0f, 0.5f, 100.0f, 4294967296.0f}) {
        f.chain = value;
        f.next();
        require(f.output == -1 && f.unit.state == nullptr, "reject invalid buffer tokens without allocating");
    }
    f.chain = 0;
    for (const int size : {0, 64, 255, 1000, 16384}) {
        f.buffer.samples = size;
        f.next();
        require(f.output == -1 && f.unit.state == nullptr, "reject unsupported FFT sizes");
    }
    f.buffer.samples = 1024;
    f.buffer.channels = 2;
    f.next();
    require(f.output == -1, "reject multichannel FFT storage");
    f.buffer.channels = 1;
    f.buffer.data = nullptr;
    f.next();
    require(f.output == -1, "reject unallocated FFT storage");
}

void allocationFailure()
{
    Fixture f;
    failAllocation = true;
    f.next();
    require(f.output == -1 && f.unit.mDone, "allocation failure disables PV processing safely");
    f.next();
    require(f.output == -1, "disabled PV must not signal buffer zero");
    failAllocation = false;
}
} // namespace

int main()
{
    InterfaceTable table {};
    table.fRTAlloc = allocate;
    table.fRTFree = release;
    table.fPrint = quietPrint;
    ft = &table;
    timingAndMapping();
    invalidBuffers();
    allocationFailure();
    std::puts("BELLA_PV_STATE_OK");
}
