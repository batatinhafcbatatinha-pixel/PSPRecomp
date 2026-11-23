#include "audio_output.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace {
constexpr std::uint64_t rate = 44100, capacity = rate * 2;
std::mutex audio_mutex;
SDL_AudioDeviceID device{};
bool attempted{}, anchored{};
std::uint64_t origin{}, cursor{};
std::array<std::uint64_t, 9> channel_end{};
std::array<std::array<std::int64_t, 2>, capacity> mix{};

bool open_audio() {
    if (attempted) return device != 0;
    attempted = true;
    SDL_AudioSpec desired{}, actual{};
    desired.freq = static_cast<int>(rate);
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    device = SDL_OpenAudioDevice(nullptr, 0, &desired, &actual, 0);
    if (!device) SDL_Log("Native audio unavailable: %s", SDL_GetError());
    else SDL_PauseAudioDevice(device, 0);
    return device != 0;
}
std::uint64_t frame_at(std::uint64_t time) {
    return time > origin ? (time - origin) / 1000000 * rate + (time - origin) % 1000000 * rate / 1000000 : 0;
}
}

namespace vcs {
bool audio_output_enabled() {
    const char *value = std::getenv("PSPRECOMP_AUDIO");
    return !value || *value != '0';
}
void audio_output_submit(std::span<const std::int16_t> pcm, std::uint32_t frames,
                         bool stereo, std::uint32_t left, std::uint32_t right,
                         std::uint32_t source_rate, std::uint32_t channel, std::uint64_t time) {
    if (!audio_output_enabled() || !source_rate || channel >= channel_end.size()) return;
    const unsigned channels = stereo ? 2 : 1;
    frames = static_cast<std::uint32_t>(std::min<std::size_t>(frames, pcm.size() / channels));
    std::lock_guard lock(audio_mutex);
    if (!open_audio()) return;
    if (!anchored) { origin = time; anchored = true; }
    const auto start = std::max({frame_at(time), cursor, channel_end[channel]});
    const auto count = std::min<std::uint64_t>(std::uint64_t(frames) * rate / source_rate, capacity);
    for (std::uint64_t i = 0; i < count && start + i < cursor + capacity; ++i) {
        const auto source = std::min<std::uint64_t>(i * source_rate / rate, frames - 1);
        auto &dest = mix[(start + i) % capacity];
        dest[0] += std::int64_t(pcm[source * channels]) * std::min(left, 32768u) / 32768;
        dest[1] += std::int64_t(pcm[source * channels + (stereo ? 1 : 0)]) * std::min(right, 32768u) / 32768;
    }
    channel_end[channel] = std::min(start + count, cursor + capacity);
}
void audio_output_advance(std::uint64_t time) {
    std::lock_guard lock(audio_mutex);
    if (!device || !anchored) return;
    const auto now = frame_at(time);
    const auto end = now > rate / 50 ? now - rate / 50 : 0;
    if (end <= cursor) return;
    if (end - cursor > capacity || SDL_GetQueuedAudioSize(device) > rate * 4 / 2) {
        SDL_ClearQueuedAudio(device);
        mix = {}; channel_end = {}; cursor = end;
        return;
    }
    std::vector<std::int16_t> pcm;
    pcm.reserve(static_cast<std::size_t>((end - cursor) * 2));
    while (cursor < end) {
        auto &frame = mix[cursor++ % capacity];
        for (auto sample : frame) pcm.push_back(static_cast<std::int16_t>(std::clamp<std::int64_t>(sample, -32768, 32767)));
        frame = {};
    }
    if (SDL_QueueAudio(device, pcm.data(), static_cast<Uint32>(pcm.size() * sizeof(pcm[0]))) != 0)
        SDL_Log("Cannot queue native audio: %s", SDL_GetError());
}
void audio_output_reset_channel(std::uint32_t channel) {
    std::lock_guard lock(audio_mutex);
    if (channel < channel_end.size()) channel_end[channel] = cursor;
}
void audio_output_shutdown() {
    std::lock_guard lock(audio_mutex);
    if (device) SDL_CloseAudioDevice(device);
    device = 0; attempted = false; anchored = false; cursor = 0;
    mix = {}; channel_end = {};
}
}
