#include "audio/AudioSystem.h"

#include "core/Config.h"

#include <SFML/Audio/Listener.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
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
            case Sfx::CreatureCallA: return "assets/audio/creature_call_a.wav";
            case Sfx::CreatureCallB: return "assets/audio/creature_call_b.wav";
            case Sfx::Hatch:      return "assets/audio/hatch.wav";
            case Sfx::Vent:       return "assets/audio/vent.wav";
            case Sfx::BossPhase:  return "assets/audio/boss_phase.wav";
            case Sfx::BossDeath:  return "assets/audio/boss_death.wav";
            case Sfx::Charge:     return "assets/audio/charge.wav";
            case Sfx::Siren:      return "assets/audio/siren.wav";
            case Sfx::DieSoft:    return "assets/audio/die_soft.wav";
            case Sfx::DieShell:   return "assets/audio/die_shell.wav";
            case Sfx::DieBladder: return "assets/audio/die_bladder.wav";
            case Sfx::DieWet:     return "assets/audio/die_wet.wav";
            case Sfx::DieVoid:    return "assets/audio/die_void.wav";
            case Sfx::HitSoft:    return "assets/audio/hit_soft.wav";
            case Sfx::HitShell:   return "assets/audio/hit_shell.wav";
            case Sfx::HitVoid:    return "assets/audio/hit_void.wav";
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
            case Sfx::CreatureCallA:
            case Sfx::CreatureCallB: return config::VolumeCreatureCall;
            case Sfx::Hatch:      return config::VolumeHatch;
            case Sfx::Vent:       return config::VolumeVent;
            case Sfx::BossPhase:  return config::VolumeBossPhase;
            case Sfx::BossDeath:  return config::VolumeBossDeath;
            case Sfx::Charge:     return config::VolumeCharge;
            case Sfx::Siren:      return config::VolumeSiren;
            case Sfx::DieSoft:
            case Sfx::DieShell:
            case Sfx::DieBladder:
            case Sfx::DieWet:
            case Sfx::DieVoid:    return config::VolumeEnemyDie;
            case Sfx::HitSoft:
            case Sfx::HitShell:
            case Sfx::HitVoid:    return config::VolumeImpact;
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

    m_filters.reserve(config::AudioVoiceCount);
    m_voices.reserve(config::AudioVoiceCount);

    for (std::size_t i = 0; i < config::AudioVoiceCount; ++i)
    {
        m_filters.push_back(std::make_unique<Filter>());
        m_voices.push_back(std::make_unique<sf::Sound>(m_buffers[0]));

        // A one-pole low-pass, installed once and then only ever told a new
        // corner frequency. It is the same filter the sounds themselves were
        // synthesised through, so rock in the way sounds like the rest of the
        // game rather than like an effect bolted onto it.
        Filter* filter = m_filters.back().get();

        m_voices.back()->setEffectProcessor(
            [filter](const float* in, unsigned int& inFrames,
                     float* out, unsigned int& outFrames, unsigned int channels) {
                const float cutoff = filter->cutoff.load(std::memory_order_relaxed);
                const unsigned int frames = std::min(inFrames, outFrames);
                const unsigned int lanes = std::min(channels, 8u);

                if (cutoff <= 0.0f)
                {
                    // Nothing in the way: hand it back untouched rather than
                    // running a filter that would do nothing but cost.
                    std::copy(in, in + static_cast<std::size_t>(frames) * channels, out);
                    inFrames = frames;
                    outFrames = frames;
                    return;
                }

                const float a = 1.0f - std::exp(-6.2831853f * cutoff / 44100.0f);

                for (unsigned int frame = 0; frame < frames; ++frame)
                {
                    for (unsigned int lane = 0; lane < channels; ++lane)
                    {
                        const std::size_t index = static_cast<std::size_t>(frame) * channels + lane;

                        if (lane < lanes)
                        {
                            filter->history[lane] += (in[index] - filter->history[lane]) * a;
                            out[index] = filter->history[lane];
                        }
                        else
                        {
                            out[index] = in[index];
                        }
                    }
                }

                inFrames = frames;
                outFrames = frames;
            });
    }

    m_ambient = std::make_unique<sf::Sound>(m_ambientBuffer);
    m_ambient->setLooping(true);
    m_ambient->setVolume(config::VolumeAmbient);

    // The drone lives in the sub, not in the water, so it must not swing round
    // the player's head the moment the listener moves.
    m_ambient->setRelativeToListener(true);
    m_ambient->setPosition({ 0.0f, 0.0f, 0.0f });

    // The four layers are the same length and start together, so they stay in
    // phase for as long as the game runs. Starting them at different moments
    // would leave the heartbeat drifting against the chord.
    const char* const layerPaths[LayerCount] = { config::MusicBedPath,
                                                 config::MusicPulsePath,
                                                 config::MusicDreadPath,
                                                 config::MusicStalkPath };

    for (std::size_t i = 0; i < LayerCount; ++i)
    {
        if (!m_musicBuffers[i].loadFromFile(layerPaths[i]))
            throw std::runtime_error(std::string("failed to load music: ") + layerPaths[i]);

        m_music[i] = std::make_unique<sf::Sound>(m_musicBuffers[i]);
        m_music[i]->setLooping(true);
        m_music[i]->setVolume(0.0f);

        // Score, not water. It follows the listener rather than sitting
        // wherever the level started.
        m_music[i]->setRelativeToListener(true);
        m_music[i]->setPosition({ 0.0f, 0.0f, 0.0f });
    }

    m_layerTarget[static_cast<std::size_t>(Layer::Bed)] = 1.0f;
}

void AudioSystem::setLayer(Layer layer, float target)
{
    const auto index = static_cast<std::size_t>(layer);
    if (index < LayerCount)
        m_layerTarget[index] = std::clamp(target, 0.0f, 1.0f);
}

void AudioSystem::updateMusic(float deltaTime)
{
    // Anything the room owed back, now that it is late enough. Walked
    // backwards so a fired echo can be taken out of the middle without
    // disturbing the ones that have not come due.
    for (std::size_t i = m_echoes.size(); i-- > 0;)
    {
        m_echoes[i].delay -= deltaTime;
        if (m_echoes[i].delay > 0.0f)
            continue;

        const Echo due = m_echoes[i];
        m_echoes.erase(m_echoes.begin() + static_cast<std::ptrdiff_t>(i));

        speak(due.sound, due.volume, due.cutoff, &due.at);
    }

    for (std::size_t i = 0; i < LayerCount; ++i)
    {
        if (!m_music[i])
            continue;

        // Faded rather than switched: a layer that appears on the frame a
        // creature notices you sounds like a bug, one that swells over a
        // second sounds like the game noticing.
        const float step = config::MusicFadeRate * deltaTime;
        m_layerLevel[i] += std::clamp(m_layerTarget[i] - m_layerLevel[i], -step, step);

        m_music[i]->setVolume(m_layerLevel[i] * config::VolumeMusic * m_musicScale);
    }
}

void AudioSystem::startAmbient()
{
    if (m_muted)
        return;

    if (m_ambient)
        m_ambient->play();

    for (auto& layer : m_music)
    {
        if (layer)
            layer->play();
    }
}

std::size_t AudioSystem::takeVoice()
{
    // Round robin: the oldest voice is the one most likely to have finished,
    // and stealing it is far better than dropping the new sound.
    const std::size_t index = m_next;
    m_next = (m_next + 1) % m_voices.size();
    return index;
}

void AudioSystem::speak(Sfx sound, float volume, float cutoff, const sf::Vector2f* at)
{
    if (m_muted || m_voices.empty())
        return;

    const std::size_t index = takeVoice();
    sf::Sound& voice = *m_voices[index];

    // Written before the buffer is handed over, so the audio thread never sees
    // a new sound running through the last one's corner frequency.
    m_filters[index]->cutoff.store(cutoff >= config::AudioOpenCutoff ? 0.0f : cutoff,
                                   std::memory_order_relaxed);

    voice.setBuffer(m_buffers[static_cast<std::size_t>(sound)]);
    voice.setVolume(volume);

    // Both paths set it. A pooled voice remembers whatever the last sound
    // through it was, so leaving either one out leaks position between sounds
    // that have nothing to do with each other.
    if (at != nullptr)
    {
        voice.setRelativeToListener(false);
        voice.setPosition({ at->x, 0.0f, at->y });
        voice.setMinDistance(config::AudioNearDistance);
        voice.setAttenuation(config::AudioRolloff);
    }
    else
    {
        voice.setRelativeToListener(true);
        voice.setPosition({ 0.0f, 0.0f, 0.0f });
    }

    voice.play();
}

void AudioSystem::play(Sfx sound, float volumeScale)
{
    speak(sound, volumeFor(sound) * volumeScale * m_sfxScale, 0.0f, nullptr);
}

void AudioSystem::playAt(Sfx sound, sf::Vector2f at, float volumeScale, float occlusion)
{
    const float blocked = std::clamp(occlusion, 0.0f, 1.0f);

    // Volume is the small half of being behind something. What actually says
    // it is the top coming off, swept in log space so the middle of the range
    // sounds like the middle rather than like nothing at all.
    const float muffle = 1.0f + (config::AudioMuffledVolume - 1.0f) * blocked;
    const float cutoff = config::AudioOpenCutoff
                         * std::pow(config::AudioMuffledCutoff / config::AudioOpenCutoff, blocked);

    const float volume = volumeFor(sound) * volumeScale * m_sfxScale * muffle;

    speak(sound, volume, cutoff, &at);

    // And whatever the room hands back. Queued rather than played, because the
    // whole point of an echo is that it is late.
    if (m_muted || m_openness <= 0.0f)
        return;

    m_echoes.push_back({ sound, at, config::EchoNearDelay,
                         volume * config::EchoNearVolume * m_openness,
                         config::EchoNearCutoff });

    // The second repeat is a hall only. One repeat is a room; two, later and
    // darker, is the only thing that says the walls are a long way off.
    if (m_openness > 0.55f)
    {
        m_echoes.push_back({ sound, at, config::EchoFarDelay,
                             volume * config::EchoFarVolume * m_openness,
                             config::EchoFarCutoff });
    }
}

void AudioSystem::setListener(sf::Vector2f at)
{
    // The world's y becomes the listener's z, which lays the game plane flat
    // in front of it. Mapping y to y instead would stand the whole basin on
    // edge, where left and right are the same place and nothing pans.
    sf::Listener::setPosition({ at.x, 0.0f, at.y });
}

void AudioSystem::setSpace(float openness)
{
    m_openness = std::clamp(openness, 0.0f, 1.0f);
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
        for (auto& layer : m_music)
        {
            if (layer)
                layer->pause();
        }
    }
    else
    {
        m_ambient->play();
        for (auto& layer : m_music)
        {
            if (layer)
                layer->play();
        }
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
