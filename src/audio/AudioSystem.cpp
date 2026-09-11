#include "audio/AudioSystem.h"

#include "core/Config.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
    const char* pathFor(Sfx sound)
    {
        switch (sound)
        {
            case Sfx::JetFire:    return "assets/audio/jet_fire.wav";
            case Sfx::Impact:     return "assets/audio/impact.wav";
            case Sfx::EnemyDie:   return "assets/audio/enemy_die.wav";
            case Sfx::PlayerHurt: return "assets/audio/player_hurt.wav";
            case Sfx::PartBreak:  return "assets/audio/boss_part_break.wav";
            case Sfx::LevelClear: return "assets/audio/level_clear.wav";
            case Sfx::BossRoar:   return "assets/audio/boss_roar.wav";
            default:              return "assets/audio/impact.wav";
        }
    }

    float volumeFor(Sfx sound)
    {
        switch (sound)
        {
            case Sfx::JetFire:    return config::VolumeJet;
            case Sfx::Impact:     return config::VolumeImpact;
            case Sfx::EnemyDie:   return config::VolumeEnemyDie;
            case Sfx::PlayerHurt: return config::VolumePlayerHurt;
            case Sfx::PartBreak:  return config::VolumePartBreak;
            case Sfx::LevelClear: return config::VolumeLevelClear;
            case Sfx::BossRoar:   return config::VolumeBossRoar;
            default:              return 60.0f;
        }
    }
}

AudioSystem::AudioSystem()
    : m_next(0)
    , m_muted(false)
    , m_sfxScale(1.0f)
    , m_musicScale(1.0f)
    , m_depthVolume(config::VolumeAmbient)
{
    for (std::size_t i = 0; i < SfxCount; ++i)
    {
        const char* file = pathFor(static_cast<Sfx>(i));
        if (!m_buffers[i].loadFromFile(file))
            throw std::runtime_error(std::string("failed to load sound: ") + file);
    }

    if (!m_ambientBuffer.loadFromFile(config::AmbientTrackPath))
        throw std::runtime_error(std::string("failed to load sound: ") + config::AmbientTrackPath);

    m_voices.reserve(config::AudioVoiceCount);
    for (std::size_t i = 0; i < config::AudioVoiceCount; ++i)
        m_voices.push_back(std::make_unique<sf::Sound>(m_buffers[0]));

    m_ambient = std::make_unique<sf::Sound>(m_ambientBuffer);
    m_ambient->setLooping(true);
    m_ambient->setVolume(config::VolumeAmbient);
}

void AudioSystem::startAmbient()
{
    if (!m_muted && m_ambient)
        m_ambient->play();
}

void AudioSystem::play(Sfx sound, float volumeScale)
{
    if (m_muted || m_voices.empty())
        return;

    // Round robin: the oldest voice is the one most likely to have finished,
    // and stealing it is far better than dropping the new sound.
    sf::Sound& voice = *m_voices[m_next];
    m_next = (m_next + 1) % m_voices.size();

    voice.setBuffer(m_buffers[static_cast<std::size_t>(sound)]);
    voice.setVolume(volumeFor(sound) * volumeScale * m_sfxScale);
    voice.play();
}

void AudioSystem::setSfxVolume(float scale)
{
    m_sfxScale = std::clamp(scale, 0.0f, 1.0f);
}

void AudioSystem::setMusicVolume(float scale)
{
    m_musicScale = std::clamp(scale, 0.0f, 1.0f);

    if (m_ambient)
        m_ambient->setVolume(m_depthVolume * m_musicScale);
}

void AudioSystem::setMuted(bool muted)
{
    m_muted = muted;

    if (!m_ambient)
        return;

    if (muted)
    {
        m_ambient->pause();
        for (auto& voice : m_voices)
            voice->stop();
    }
    else
    {
        m_ambient->play();
    }
}

void AudioSystem::setDepth(float pitch, float volume)
{
    if (!m_ambient)
        return;

    m_depthVolume = volume;
    m_ambient->setPitch(pitch);
    m_ambient->setVolume(volume * m_musicScale);
}
