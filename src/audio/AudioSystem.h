#pragma once

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <atomic>
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
    CreatureCallA, // something big, far off, that has noticed you
    CreatureCallB,
    Hatch,
    Vent,
    BossPhase,     // a creature changing its mind
    BossDeath,
    Charge,
    Siren,         // the hull alarm, when there is almost none of it left

    // A basin full of different animals used to sound like one animal being
    // killed over and over. One voice per kind of body instead: what it is
    // made of, how much air is in it, and how far it falls on the way out.
    DieSoft,
    DieShell,
    DieBladder,
    DieWet,
    DieVoid,
    HitSoft,
    HitShell,
    HitVoid,

    // The surface, and everything the rework added under it. None of these
    // existed until the deck did, and a deck you walk about on in silence is
    // a screenshot with a man on it.
    DeckStepA,
    DeckStepB,
    DeckRig,      // steel complaining about the swell
    DeckWinch,    // the crane taking the hold off you
    DeckPaid,     // and what it hands back
    DeckBoard,    // the tree, opening
    BellIn,       // the hatch, from inside
    BellGo,       // and the winch letting you down

    LiftHold,     // sitting in the bell, waiting for it to decide
    LiftGo,       // and it has
    ShaftHold,
    ShaftGo,
    ShaftOpen,    // the way down unlocking, heard from anywhere in the basin

    // Three, because the sound has to say which end of fourteen to three
    // hundred and seventy the thing on the floor was.
    FindSmall,
    FindGood,
    FindRich,

    CutHull,      // a torch on somebody else's plate
    HullGive,     // and the moment it stops being sealed
    BroodBurn,
    RigLog,       // a dead man's recorder finding the end of its tape

    ReelHorn,     // the four stings the cutscenes are cut against
    ReelFall,
    ReelClimb,
    ReelDawn,
    ReelMark,
    Count
};

// Loads every sound once and plays them through a small pool of voices, so a
// busy frame can never run out of players or cut an effect short by reusing the
// one object that was already speaking.
class AudioSystem
{
public:
    AudioSystem(); // throws if a sound file is missing

    // The sub own noises: no position, so they sit in the middle of the head
    // wherever the sub happens to be.
    void play(Sfx sound, float volumeScale = 1.0f);

    // Everything that happens somewhere. `occlusion` is how much rock is in
    // the way, 0 to 1: it takes the volume down and, more importantly, takes
    // the top off - a hit around a corner should sound like it is around a
    // corner rather than like a quieter hit.
    void playAt(Sfx sound, sf::Vector2f at, float volumeScale = 1.0f, float occlusion = 0.0f);

    // Where the listener is. Called once a frame; the world plane is mapped
    // flat in front of the listener, because mapping it edge-on leaves the
    // panning degenerate and every sound in the middle.
    void setListener(sf::Vector2f at);

    // How open the room is, 0 for a passage and 1 for a great hall. It is not
    // a filter on anything already playing - it decides how much of each new
    // sound comes back a moment later, which is what a room sounds like.
    void setSpace(float openness);
    void startAmbient();

    // Three loops of the same length over the same root, played together and
    // faded independently. The bed is always on; the caller pushes the other
    // two up and down as the fight gets worse. Nothing here is a tune - a
    // melody would wear out inside one level - so it can sit under a fight for
    // as long as the fight lasts.
    enum class Layer { Bed, Pulse, Dread, Stalk, Reel, Count };

    // Where the listener is standing, which decides which of the two loops is
    // under everything. They crossfade rather than cut, because surfacing is
    // meant to feel like coming up and not like a scene change.
    enum class Ambience { Abyss, Deck, Count };

    void setAmbience(Ambience where);

    void setLayer(Layer layer, float target); // 0..1, faded to over a second
    void updateMusic(float deltaTime);

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

    static constexpr std::size_t LayerCount = static_cast<std::size_t>(Layer::Count);

    static constexpr std::size_t AmbienceCount = static_cast<std::size_t>(Ambience::Count);

    std::array<sf::SoundBuffer, SfxCount> m_buffers;
    std::array<sf::SoundBuffer, AmbienceCount> m_ambientBuffers;

    std::array<sf::SoundBuffer, LayerCount>              m_musicBuffers;
    std::array<std::unique_ptr<sf::Sound>, LayerCount>   m_music;
    std::array<float, LayerCount>                        m_layerLevel{};
    std::array<float, LayerCount>                        m_layerTarget{};

    // One low-pass per voice, and it has to be declared BEFORE the voices:
    // members are destroyed in reverse, and the processor a voice is holding
    // points straight at one of these. Declared after, they would be gone
    // while the sounds were still being torn down.
    //
    // The cutoff is written by the game thread and read by the audio thread,
    // so it is atomic. The filter state itself is only ever touched by the
    // audio thread.
    struct Filter
    {
        std::atomic<float> cutoff{ 0.0f }; // 0 means pass it straight through
        float              history[8]{};
    };

    std::vector<std::unique_ptr<Filter>> m_filters;

    // unique_ptr so the pool never needs sf::Sound to be copyable or movable.
    std::vector<std::unique_ptr<sf::Sound>> m_voices;
    std::array<std::unique_ptr<sf::Sound>, AmbienceCount> m_ambients;

    // Which loop should be at full and how far each one has got. Both are
    // always playing; only their volumes move.
    Ambience m_ambience = Ambience::Abyss;
    std::array<float, AmbienceCount> m_ambienceLevel{ 1.0f, 0.0f };

    // A sound waiting to be played again as its own echo. The pool has no
    // notion of deferred playback otherwise, and a delay line per voice would
    // be two dozen ring buffers for one shared effect.
    struct Echo
    {
        Sfx          sound;
        sf::Vector2f at;
        float        delay;
        float        volume;
        float        cutoff;
    };

    std::size_t takeVoice();

    // The one place a voice is actually started. Everything else works out
    // what to hand it.
    void speak(Sfx sound, float volume, float cutoff, const sf::Vector2f* at);

    std::vector<Echo> m_echoes;
    float             m_openness = 0.0f;

    std::size_t m_next;
    bool        m_muted;

    // The depth theme sets the drone's own level; the player's trim multiplies
    // it, so both are remembered rather than one overwriting the other.
    float m_sfxScale;
    float m_musicScale;
    float m_depthVolume;
};
