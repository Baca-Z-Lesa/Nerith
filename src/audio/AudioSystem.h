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

    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

private:
    static constexpr std::size_t SfxCount = static_cast<std::size_t>(Sfx::Count);

    std::array<sf::SoundBuffer, SfxCount> m_buffers;
    sf::SoundBuffer m_ambientBuffer;

    // unique_ptr so the pool never needs sf::Sound to be copyable or movable.
    std::vector<std::unique_ptr<sf::Sound>> m_voices;
    std::unique_ptr<sf::Sound>              m_ambient;

    std::size_t m_next;
    bool        m_muted;
};
