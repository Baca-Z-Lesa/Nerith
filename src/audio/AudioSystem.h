#pragma once

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

enum class Sfx
{
    JetFire,
    Impact,
    EnemyDie,
    PlayerHurt,
    PartBreak,
    LevelClear,
    BossRoar,
    Count
};

// Loads every sound once and plays them through a small pool of voices, so a
// busy frame can never run out of players or cut an effect short by reusing the
// one object that was already speaking.
class AudioSystem
{
public:
    AudioSystem(); // throws if a sound file is missing

    void play(Sfx sound, float volumeScale = 1.0f);
    void startAmbient();

    // Deeper water: the drone drops in pitch and leans on you harder.
    void setDepth(float pitch, float volume);

    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

    // Two independent trims, each 0..1, applied on top of the per-sound volumes
    // in the config. The ambient drone is the loudest thing in the game and the
    // first thing anyone wants to turn down, so it has its own.
    void setSfxVolume(float scale);
    void setMusicVolume(float scale);
    float getSfxVolume() const { return m_sfxScale; }
    float getMusicVolume() const { return m_musicScale; }

private:
    static constexpr std::size_t SfxCount = static_cast<std::size_t>(Sfx::Count);

    std::array<sf::SoundBuffer, SfxCount> m_buffers;
    sf::SoundBuffer m_ambientBuffer;

    // unique_ptr so the pool never needs sf::Sound to be copyable or movable.
    std::vector<std::unique_ptr<sf::Sound>> m_voices;
    std::unique_ptr<sf::Sound>              m_ambient;

    std::size_t m_next;
    bool        m_muted;

    // The depth theme sets the drone's own level; the player's trim multiplies
    // it, so both are remembered rather than one overwriting the other.
    float m_sfxScale;
    float m_musicScale;
    float m_depthVolume;
};
