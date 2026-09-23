#pragma once

#include "entities/BossDef.h"
#include "entities/EnemyType.h"
#include "upgrades/Stats.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <vector>

// A boss drawn rather than stamped.
//
// Every creature in this game is a sprite that gets moved around, and for most
// of them that is the right answer: they are twenty pixels across and there
// are fifteen of them. A boss is two hundred pixels of animal that the player
// stares at for a minute and a half, and a picture of an animal held at the
// right angle for ninety seconds is a picture, not an animal. It does not
// breathe, its jaws do not open before it bites, its fins do not beat, and it
// cannot look at you.
//
// So the big ones are built here out of shapes instead. Nothing in this file
// knows anything about the fight: Boss hands over a pose - where every link of
// the spine is, what each limb is doing, how far the jaw is open, how hard it
// is breathing, where it is looking - and this draws that pose. The split is
// what stops the drawing leaking into the creature and the creature leaking
// into the drawing.
namespace bossart
{
    // One link of the spine, in world space.
    struct Link
    {
        sf::Vector2f at;
        sf::Angle    angle;
        float        girth; // half the body's width here, in pixels
    };

    // One thing bolted to the spine, already placed by the fight.
    struct Limb
    {
        PartRole     role = PartRole::Fin;
        sf::Vector2f at;
        sf::Angle    angle;
        float        size = 24.0f;
        float        flash = 0.0f; // 0..1, white hot just after a hit
        float        side = 1.0f;  // which flank, -1 or +1
        bool         alive = true;
        bool         shown = true; // organs: whether the plating is off it
    };

    struct Body
    {
        std::vector<Link> links; // head first
        std::vector<Limb> limbs;

        float jaw = 0.0f;    // 0 shut, 1 wide open
        float breath = 0.0f; // 0..1, the gills working
        float beat = 0.0f;   // -1..1, where the fins are in their stroke
        float rage = 0.0f;   // 0..1, how far into the fight it is
        float tell = 0.0f;   // 0..1, how far through a wind-up
        float flinch = 0.0f; // 0..1, it has just been hurt
        float pulse = 0.0f;  // 0..1, a bell mid clench
        float shed = 0.0f;   // 0..1, how far through coming apart it is
        float alpha = 1.0f;

        sf::Vector2f look{ 1.0f, 0.0f }; // the direction it is looking in
    };

    // hide is the tiling sheet every shape is surfaced with. It is multiplied
    // by the colour the shape asked for, so one sheet does hide, plate, fin
    // and jaw - which is the same bargain every sprite in the game makes with
    // the relief pass.
    void draw(sf::RenderTarget& target, BossSkin skin, const Body& body,
              const sf::Texture& hide);
}

// The same bargain, for the small ones.
//
// Everything that is not a boss was a picture: one sprite per kind, turned to
// face where it was going. Twelve pictures of twelve things, and because they
// were drawn as whatever shape read clearly at twenty pixels, what came out was
// a set of blobs with eyes on. Nothing in the water had a front and a back for
// any reason, nothing had a mouth where a mouth goes, and the only way to tell
// two of them apart was the colour.
//
// They are built out of shapes now, off the same vocabulary the bosses use. Not
// portraits of real species - at this size a species is unreadable - but honest
// animal anatomy: a body with a head end and a tail end, fins where fins take
// hold, gills behind the head, jaws that open, symmetry that means something.
// A creature that has those reads as alive from a single frame, and one that
// has none of them cannot, however carefully it is shaded.
namespace creatureart
{
    // Everything the drawing is allowed to know. Enemy hands over a pose and
    // this draws it, the same split that keeps the fight out of the bosses'
    // drawing and the drawing out of the fight.
    struct Pose
    {
        sf::Vector2f at{};
        sf::Angle    angle{};        // which way it is pointing
        float        size = 16.0f;   // its own radius, in pixels
        float        beat = 0.0f;    // -1..1, where the fins are in their stroke
        float        breath = 0.0f;  // 0..1, the gills working
        float        jaw = 0.0f;     // 0..1, shut to wide
        float        flinch = 0.0f;  // 0..1, it has just been hit
        float        tell = 0.0f;    // 0..1, how far through a wind-up
        sf::Color    tint{ 255, 255, 255, 255 };

        // What the creature's own swimming motion is doing to its proportions
        // this instant: squeezing along itself and swelling across, which is
        // how a thing that swims by pushing water actually moves.
        float        along = 1.0f;
        float        across = 1.0f;

        // Creatures that tow a body rather than having one. Null for the rest.
        const std::vector<sf::Vector2f>* spine = nullptr;

        // Which way the lamp is, from this creature. The flank it turns toward
        // the light takes more of it, so a thing swimming across the beam is
        // visibly turning in it rather than sliding across a flat picture.
        // Zero leaves it on the fixed corner every sprite in the game is lit
        // from, which is what the bosses and the logbook want.
        sf::Vector2f lightFrom{ 0.0f, 0.0f };
    };

    void draw(sf::RenderTarget& target, EnemyType type, const Pose& pose,
              const sf::Texture& hide);

    // What the creature is made of, for whoever has to throw pieces of it
    // about. Kept here because this is the only place that decides it.
    sf::Color coatOf(EnemyType type);

    // And which sheet its surface comes off. Scale for the fish, plate for the
    // armoured, soft for everything gelatinous, fur for the one animal down
    // there that is not wet. Returned as the asset path, because the same
    // place that decides what a creature is made of may as well say so.
    const char* surfaceOf(EnemyType type);
}

// And the thing the player is driving.
//
// Everything in the water was drawn out of shapes except the one object on
// screen the whole time. The sub stayed a 128 pixel sprite with six more
// sprites bolted to it, so the game had two art styles in every single frame:
// smooth shaded creatures swimming around a pixel-art submarine that was the
// only thing that never turned, never breathed and had no volume at all. It
// was the most looked-at object in the game and the flattest.
//
// Built here instead, off the same vocabulary, with the same light on it - and
// because it is shapes now it can move: the screw turns, the planes bite when
// it manoeuvres, the hull squeezes when it does.
namespace subart
{
    struct Pose
    {
        sf::Vector2f at{};
        sf::Angle    angle{};
        float        size = 32.0f;  // half its length, in pixels
        float        along = 1.0f;  // what the motion is doing to its proportions
        float        across = 1.0f;
        float        spin = 0.0f;   // where the screw is in its turn
        float        thrust = 0.0f; // 0..1, how hard it is driving
        float        flinch = 0.0f; // 0..1, it has just been hit
        sf::Color    tint{ 255, 255, 255, 255 };
        sf::Vector2f lightFrom{ 0.0f, 0.0f };

        // What the tree has bolted on, indexed by ShipPart.
        std::array<bool, static_cast<std::size_t>(ShipPart::Count)> fitted{};
        bool shieldUp = false;
    };

    void draw(sf::RenderTarget& target, const Pose& pose, const sf::Texture& hide);
}
