#ifndef HH_AUDIO_PLAYER_HPP
#define HH_AUDIO_PLAYER_HPP

// Engine-side audio wrapper. Mirrors the small subset of SFML 2.x's
// audio module that this game actually uses (sf::Sound / sf::Music /
// sf::Listener::setGlobalVolume) but is backed by miniaudio so the
// final HH.exe carries zero audio-runtime DLLs.
//
// Why a custom wrapper instead of leaning on SFML/Audio?
//   * SFML 2.x's audio module is a thin shell over OpenAL Soft, which
//     is LGPL and must remain a dynamic dependency. We want a truly
//     single-file redistributable -> no openal32.dll allowed.
//   * miniaudio (mackron/miniaudio, MIT / public-domain dual license)
//     is a single-header, statically-linked audio engine that supports
//     the exact handful of operations we need (load + play + loop +
//     master volume). Vendored at vendor/miniaudio/miniaudio.h.
//
// Threading: every method here is intended to be called from the main
// (UI) thread, same as the rest of the engine. miniaudio internally
// spawns its own audio device thread; the API surface below is safe to
// call from the game loop without extra locking.
//
// Failure mode: a missing or malformed audio file is non-fatal -- the
// load returns false, the corresponding Sound / Music stays silent,
// and the rest of the game keeps running. This matches the previous
// SFML behavior (sf::SoundBuffer::loadFromFile just printed a warning).

#include <memory>
#include <string>

namespace hh {

// Opaque PIMPL: lets the header stay free of miniaudio.h (which is a
// ~4 MB single-header that should only compile in one TU).
struct SoundImpl;
struct MusicImpl;

// One-shot in-memory sound effect. Mirrors sf::Sound + sf::SoundBuffer
// rolled together; the underlying decoded PCM buffer is auto-shared by
// miniaudio's resource manager when multiple Sounds reference the same
// file, so repeated load(path) calls for the same path are cheap.
class Sound {
public:
    Sound();
    ~Sound();
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;
    Sound(Sound&&) noexcept;
    Sound& operator=(Sound&&) noexcept;

    // Decode the file at `key` (a logical resource key returned by
    // Paths::resource(), e.g. "sound/hurt.wav") into memory. Returns
    // false if the audio backend hasn't initialized or the file can't
    // be opened / decoded. Calling load() twice on the same instance
    // releases the previous buffer first. AudioPlayer internally
    // translates the key for the active backend (disk or embedded).
    bool load(const std::string& key);

    // Restart from the beginning and play. Matches sf::Sound::play()'s
    // "calling play() on a playing sound restarts it" semantics; this
    // is important for repeating cues like the hurt sting.
    void play();

    // Halt playback without releasing the buffer. Safe to call when
    // not playing. After stop(), the next play() restarts from the
    // beginning.
    void stop();

    // True while audio is actively coming out of this Sound.
    bool isPlaying() const;

private:
    std::unique_ptr<SoundImpl> impl_;
};

// Streaming long-form music. Mirrors sf::Music. The file stays open on
// disk for the lifetime of the Music object (because miniaudio streams
// chunks); call openFromFile() with a new path or destroy the Music to
// release the file handle.
class Music {
public:
    Music();
    ~Music();
    Music(const Music&) = delete;
    Music& operator=(const Music&) = delete;
    Music(Music&&) noexcept;
    Music& operator=(Music&&) noexcept;

    // Open the music file for streaming. `key` is a logical resource
    // key returned by Paths::resource() (e.g. "music/theme.ogg").
    // Returns true on success. Calling openFromFile() while another
    // file is loaded stops the previous track and releases its handle
    // first, matching the sf::Music::openFromFile semantics the title
    // screen relies on.
    bool openFromFile(const std::string& key);

    // Start (or resume) playback. No-op if no file is loaded.
    void play();

    // Stop and rewind to the beginning. Safe to call when not playing.
    void stop();

    // Enable / disable looping. The flag persists across play()/stop().
    void setLoop(bool loop);

private:
    std::unique_ptr<MusicImpl> impl_;
};

// Process-wide audio controls. The first call lazily boots the
// miniaudio engine; subsequent calls reuse the same engine instance.
class AudioPlayer {
public:
    // Master volume in the same 0..100 range SFML's
    // sf::Listener::getGlobalVolume() / setGlobalVolume() exposed, so
    // the PauseMenu's existing slider doesn't have to be re-scaled.
    static void  setMasterVolume(float volume0to100);
    static float getMasterVolume();

    // Tear down the engine (and free any cached decoded samples).
    // Optional; the OS reclaims everything at process exit anyway, but
    // tests call this between cases to keep valgrind / leak sanitizers
    // happy.
    static void shutdown();
};

} // namespace hh

#endif // HH_AUDIO_PLAYER_HPP
