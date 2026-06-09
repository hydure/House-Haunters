// AudioPlayer implementation. This is the *only* TU that compiles the
// miniaudio single-header (via MINIAUDIO_IMPLEMENTATION) so the ~4 MB
// header isn't reparsed everywhere.

#include "engine/AudioPlayer.hpp"
#include "engine/Paths.hpp"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

// miniaudio: vendored at vendor/miniaudio/miniaudio.h.
// Pre-implementation knobs:
//  * Disable the optional resource-recording back-end (we never record).
//  * Disable WebAudio (we're a desktop game).
//  * Disable the Vorbis / Opus / MP3 decoders we don't use to keep the
//    final .exe size down. We do enable WAV + FLAC because the vanilla
//    mod cues use those, and OGG because some default loops are .ogg.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_GENERATION             // no procedural waveform/noise generators
#define MA_NO_ENCODING               // no encoders, only decoders
#define MA_API static                // keep miniaudio symbols TU-local
#include "miniaudio.h"

// When the resources/ tree is baked into HH.exe we have to teach
// miniaudio's resource manager to resolve filenames to memory buffers
// instead of opening files on disk. We do that by walking the embedded
// table once at engine init and calling
// ma_resource_manager_register_encoded_data() for every audio file.
// Subsequent ma_sound_init_from_file() calls then resolve the same
// string key against the in-memory buffer rather than CreateFile.
#if defined(HH_EMBED_RESOURCES)
#  include "engine/EmbeddedResources.hpp"
#endif

namespace hh {

// ---------------------------------------------------------------
// Engine bootstrap
// ---------------------------------------------------------------
//
// One process-wide ma_engine. Initialized lazily on the first audio
// operation, torn down explicitly via AudioPlayer::shutdown() (or
// implicitly at process exit -- the OS reclaims the device).
//
// All accesses are guarded by g_mutex because Character::init / each
// screen's init can run from the (main) game thread but the engine's
// device thread reads the same state. The lock is held only for the
// brief setup / teardown windows; play / stop calls hit miniaudio's
// own atomics directly.

namespace {

std::mutex g_mutex;
ma_engine  g_engine{};
bool       g_engineReady = false;
bool       g_engineFailed = false; // set if init failed once -- don't retry every call

// Boot the engine on demand. Returns true if the engine is usable.
// On failure (no audio device, driver unavailable in CI, etc.) we
// remember the failure and short-circuit subsequent calls so the
// rest of the game keeps running silent rather than spamming retries.
bool ensureEngine_unlocked()
{
    if (g_engineReady) return true;
    if (g_engineFailed) return false;

    ma_engine_config cfg = ma_engine_config_init();
    // Defaults: stereo, 48 kHz, the default playback device. Suitable
    // for everything we ship.
    if (ma_engine_init(&cfg, &g_engine) != MA_SUCCESS) {
        std::fprintf(stderr,
            "[AudioPlayer] miniaudio engine init failed -- running silent.\n");
        g_engineFailed = true;
        return false;
    }
    g_engineReady = true;

#if defined(HH_EMBED_RESOURCES)
    // Pre-register every embedded audio file with miniaudio's resource
    // manager. Each entry is registered under its bare logical path
    // (e.g. "sound/foo.wav") -- the EXACT string Sound::load /
    // Music::openFromFile pass to ma_sound_init_from_file via
    // Paths::resolveForBackend in embedded mode. Since registration
    // and lookup share the same Paths translation, the two cannot
    // silently drift out of sync.
    //
    // We only register file extensions we actually decode (.wav, .flac,
    // .ogg, .mp3). The table includes textures, fonts, XML, etc. -- we
    // skip those because their bytes are served via ResourceFS, not the
    // audio resource manager.
    ma_resource_manager* rm = ma_engine_get_resource_manager(&g_engine);
    if (rm) {
        const hh::embedded::Entry* tbl = hh::embedded::table();
        const std::size_t n = hh::embedded::table_size();
        for (std::size_t i = 0; i < n; ++i) {
            const char* path = tbl[i].path;
            const std::size_t plen = std::strlen(path);

            // Cheap extension sniff. Lowercase comparison so the table
            // doesn't care about resource filename casing.
            auto endsWithCI = [path, plen](const char* ext) -> bool {
                const std::size_t elen = std::strlen(ext);
                if (plen < elen) return false;
                for (std::size_t k = 0; k < elen; ++k) {
                    char a = path[plen - elen + k];
                    char b = ext[k];
                    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                    if (a != b) return false;
                }
                return true;
            };
            if (!endsWithCI(".wav") && !endsWithCI(".flac") &&
                !endsWithCI(".ogg") && !endsWithCI(".mp3")) {
                continue;
            }

            const void* data = nullptr;
            std::size_t size = 0;
            if (!hh::embedded::find(path, &data, &size) || !data || size == 0) {
                continue;
            }

            // The registry key matches what Paths::resolveForBackend
            // produces in embedded mode (identity on the logical key),
            // so Sound::load("sound/foo.wav") finds the registered
            // buffer instead of falling through to disk I/O.
            const std::string key = Paths::resolveForBackend(path);
            ma_result rr = ma_resource_manager_register_encoded_data(
                rm,
                key.c_str(),
                const_cast<void*>(data),  // ma signature is non-const but doesn't write
                size);
            if (rr != MA_SUCCESS) {
                std::fprintf(stderr,
                    "[AudioPlayer] failed to register embedded audio \"%s\" (ma_result=%d)\n",
                    key.c_str(), static_cast<int>(rr));
            }
        }
    }
#endif  // HH_EMBED_RESOURCES

    return true;
}

// Convert SFML's 0..100 volume to miniaudio's 0..1 linear gain.
float to_ma_volume(float sfml0to100)
{
    if (sfml0to100 < 0.0f)   sfml0to100 = 0.0f;
    if (sfml0to100 > 100.0f) sfml0to100 = 100.0f;
    return sfml0to100 / 100.0f;
}

float from_ma_volume(float ma0to1)
{
    return ma0to1 * 100.0f;
}

} // namespace

// ---------------------------------------------------------------
// Sound (cached decoded one-shot)
// ---------------------------------------------------------------

struct SoundImpl {
    ma_sound sound{};
    bool     ready = false;
};

Sound::Sound() : impl_(new SoundImpl) {}

Sound::~Sound()
{
    if (impl_ && impl_->ready) {
        ma_sound_uninit(&impl_->sound);
        impl_->ready = false;
    }
}

Sound::Sound(Sound&& other) noexcept : impl_(std::move(other.impl_)) {}

Sound& Sound::operator=(Sound&& other) noexcept
{
    if (this != &other) {
        if (impl_ && impl_->ready) {
            ma_sound_uninit(&impl_->sound);
            impl_->ready = false;
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool Sound::load(const std::string& key)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!ensureEngine_unlocked()) return false;
    if (!impl_) return false;

    // Drop any previously loaded sample on this Sound.
    if (impl_->ready) {
        ma_sound_uninit(&impl_->sound);
        impl_->ready = false;
    }

    // Translate the logical key to whatever the backend wants:
    //   * Disk build: "sound/foo.wav" -> "../resources/sound/foo.wav"
    //                 (miniaudio opens that via the C stdio VFS).
    //   * Embedded build: identity -- the same string was pre-registered
    //                 against an in-memory buffer in ensureEngine_unlocked,
    //                 so ma_sound_init_from_file resolves it without
    //                 touching the filesystem.
    const std::string maPath = Paths::resolveForBackend(key);

    // MA_SOUND_FLAG_DECODE: fully decode the file into memory at load
    // time so subsequent play() calls don't stutter on the first
    // chunk. miniaudio's engine-owned resource manager auto-caches
    // the decoded buffer keyed on the file path, so loading the same
    // path from multiple Sound objects is cheap after the first hit.
    ma_result r = ma_sound_init_from_file(
        &g_engine,
        maPath.c_str(),
        MA_SOUND_FLAG_DECODE,
        /*pGroup=*/nullptr,
        /*pDoneFence=*/nullptr,
        &impl_->sound);

    if (r != MA_SUCCESS) {
        std::fprintf(stderr,
            "[AudioPlayer] Sound::load failed for \"%s\" (ma_result=%d)\n",
            maPath.c_str(), static_cast<int>(r));
        return false;
    }
    impl_->ready = true;
    return true;
}

void Sound::play()
{
    if (!impl_ || !impl_->ready) return;
    // SFML's sf::Sound::play() restarts a playing sound from the
    // beginning. miniaudio's ma_sound_start does not implicitly
    // rewind, so we seek to 0 first to preserve the cue-restart
    // behavior the hurt / death stings rely on.
    ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
    ma_sound_start(&impl_->sound);
}

void Sound::stop()
{
    if (!impl_ || !impl_->ready) return;
    ma_sound_stop(&impl_->sound);
}

bool Sound::isPlaying() const
{
    if (!impl_ || !impl_->ready) return false;
    return ma_sound_is_playing(&impl_->sound) == MA_TRUE;
}

// ---------------------------------------------------------------
// Music (streaming)
// ---------------------------------------------------------------

struct MusicImpl {
    ma_sound sound{};
    bool     ready = false;
    bool     looping = false;
};

Music::Music() : impl_(new MusicImpl) {}

Music::~Music()
{
    if (impl_ && impl_->ready) {
        ma_sound_uninit(&impl_->sound);
        impl_->ready = false;
    }
}

Music::Music(Music&& other) noexcept : impl_(std::move(other.impl_)) {}

Music& Music::operator=(Music&& other) noexcept
{
    if (this != &other) {
        if (impl_ && impl_->ready) {
            ma_sound_uninit(&impl_->sound);
            impl_->ready = false;
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool Music::openFromFile(const std::string& key)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!ensureEngine_unlocked()) return false;
    if (!impl_) return false;

    // Drop any previously open file on this Music.
    if (impl_->ready) {
        ma_sound_uninit(&impl_->sound);
        impl_->ready = false;
    }

    // See Sound::load -- Paths::resolveForBackend hides the disk-vs-
    // embedded difference behind a single helper.
    const std::string maPath = Paths::resolveForBackend(key);

    // MA_SOUND_FLAG_STREAM: stream the file in chunks. Mirrors
    // sf::Music's "don't load multi-MB tracks into memory" semantics.
    ma_result r = ma_sound_init_from_file(
        &g_engine,
        maPath.c_str(),
        MA_SOUND_FLAG_STREAM,
        /*pGroup=*/nullptr,
        /*pDoneFence=*/nullptr,
        &impl_->sound);

    if (r != MA_SUCCESS) {
        std::fprintf(stderr,
            "[AudioPlayer] Music::openFromFile failed for \"%s\" (ma_result=%d)\n",
            maPath.c_str(), static_cast<int>(r));
        return false;
    }
    impl_->ready = true;
    // Re-apply the persisted loop flag so a re-open after stop() keeps
    // looping behavior intact.
    ma_sound_set_looping(&impl_->sound, impl_->looping ? MA_TRUE : MA_FALSE);
    return true;
}

void Music::play()
{
    if (!impl_ || !impl_->ready) return;
    ma_sound_start(&impl_->sound);
}

void Music::stop()
{
    if (!impl_ || !impl_->ready) return;
    ma_sound_stop(&impl_->sound);
    ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
}

void Music::setLoop(bool loop)
{
    if (!impl_) return;
    impl_->looping = loop;
    if (impl_->ready) {
        ma_sound_set_looping(&impl_->sound, loop ? MA_TRUE : MA_FALSE);
    }
}

// ---------------------------------------------------------------
// Process-wide controls
// ---------------------------------------------------------------

void AudioPlayer::setMasterVolume(float volume0to100)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!ensureEngine_unlocked()) return;
    ma_engine_set_volume(&g_engine, to_ma_volume(volume0to100));
}

float AudioPlayer::getMasterVolume()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!ensureEngine_unlocked()) return 100.0f;
    // miniaudio doesn't expose a direct getter on ma_engine pre-0.11;
    // we read from the engine's internal node graph master endpoint.
    float v = ma_node_get_output_bus_volume(
        ma_engine_get_endpoint(&g_engine), 0);
    return from_ma_volume(v);
}

void AudioPlayer::shutdown()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_engineReady) {
        ma_engine_uninit(&g_engine);
        g_engineReady = false;
    }
}

} // namespace hh
