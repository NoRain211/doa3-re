#include "audio_capture_xapo.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>
#include <xapobase.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace {

const XAPO_REGISTRATION_PROPERTIES properties = {
    {0xa6da3ade, 0x8211, 0x4ae4, {0x8a, 0x09, 0x11, 0x25, 0x62, 0x31, 0xd1, 0x72}},
    L"Recomp audio capture", L"", 1, 0,
    XAPOBASE_DEFAULT_FLAG | XAPO_FLAG_INPLACE_REQUIRED, 1, 1, 1, 1
};

class Capture : public CXAPOBase {
public:
    Capture() : CXAPOBase(&properties) {}
    std::vector<float> samples;
    size_t used = 0;
    ULONGLONG first_tick = 0;
    uint32_t rate = 0, channels = 0;

    HRESULT STDMETHODCALLTYPE LockForProcess(UINT32 inputs,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *input, UINT32 outputs,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *output) override
    {
        HRESULT result = CXAPOBase::LockForProcess(inputs, input, outputs, output);
        if (FAILED(result)) return result;
        rate = input[0].pFormat->nSamplesPerSec;
        channels = input[0].pFormat->nChannels;
        // ponytail: retain the first 60 seconds; use a writer thread for longer runs.
        try { samples.resize(size_t(rate) * channels * 60); }
        catch (const std::bad_alloc &) { UnlockForProcess(); return E_OUTOFMEMORY; }
        return S_OK;
    }

    void STDMETHODCALLTYPE Process(UINT32,
        const XAPO_PROCESS_BUFFER_PARAMETERS *input, UINT32,
        XAPO_PROCESS_BUFFER_PARAMETERS *output, BOOL enabled) override
    {
        output[0].ValidFrameCount = input[0].ValidFrameCount;
        output[0].BufferFlags = input[0].BufferFlags;
        if (!enabled) return;
        size_t count = size_t(input[0].ValidFrameCount) * channels;
        if (count && !first_tick) first_tick = GetTickCount64();
        if (count > samples.size() - used) count = samples.size() - used;
        if (input[0].BufferFlags == XAPO_BUFFER_SILENT)
            std::memset(samples.data() + used, 0, count * sizeof(float));
        else
            std::memcpy(samples.data() + used, input[0].pBuffer, count * sizeof(float));
        used += count;
    }
};

Capture *capture;
FILE *capture_file;

void write_u32(FILE *file, uint32_t value)
{
    std::fwrite(&value, sizeof value, 1, file);
}

} // namespace

void recomp_audio_capture_attach(IXAudio2MasteringVoice *master)
{
    const char *path = std::getenv("RECOMP_AUDIO_CAPTURE");
    if (!path || !*path || capture) return;
    // Never overwrite an earlier recording, including after device loss.
    capture_file = std::fopen(path, "wbx");
    if (!capture_file) {
        std::fprintf(stderr, "[audio-capture] cannot create recording\n");
        return;
    }
    capture = new (std::nothrow) Capture;
    if (!capture) { std::fclose(capture_file); capture_file = nullptr; return; }
    XAUDIO2_VOICE_DETAILS details{};
    master->GetVoiceDetails(&details);
    XAUDIO2_EFFECT_DESCRIPTOR effect{capture, TRUE, details.InputChannels};
    XAUDIO2_EFFECT_CHAIN chain{1, &effect};
    HRESULT result = master->SetEffectChain(&chain);
    if (FAILED(result)) {
        std::fprintf(stderr, "[audio-capture] attach failed=0x%08lx\n", (unsigned long)result);
        capture->Release(); capture = nullptr;
        std::fclose(capture_file); capture_file = nullptr;
        return;
    }
    std::fprintf(stderr, "[audio-capture] attached rate=%u channels=%u limit=60s\n",
        capture->rate, capture->channels);
}

void recomp_audio_capture_save(void)
{
    if (!capture) return;
    const uint32_t bytes = uint32_t(capture->used * sizeof(float));
    std::fwrite("RIFF", 4, 1, capture_file); write_u32(capture_file, bytes + 36);
    std::fwrite("WAVEfmt ", 8, 1, capture_file); write_u32(capture_file, 16);
    uint16_t format[] = {3, uint16_t(capture->channels)}; // IEEE float
    std::fwrite(format, sizeof format, 1, capture_file);
    write_u32(capture_file, capture->rate);
    write_u32(capture_file, capture->rate * capture->channels * 4);
    uint16_t align[] = {uint16_t(capture->channels * 4), 32};
    std::fwrite(align, sizeof align, 1, capture_file);
    std::fwrite("data", 4, 1, capture_file); write_u32(capture_file, bytes);
    size_t written = std::fwrite(capture->samples.data(), 1, bytes, capture_file);
    int closed = std::fclose(capture_file);
    std::fprintf(stderr, "[audio-capture] saved frames=%zu rate=%u first_tick=%llu complete=%u\n",
        capture->used / capture->channels, capture->rate, capture->first_tick,
        written == bytes && closed == 0);
    capture_file = nullptr;
    capture->Release(); capture = nullptr;
}
