#pragma once

#include "entities/BossDef.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

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
        float alpha = 1.0f;

        sf::Vector2f look{ 1.0f, 0.0f }; // the direction it is looking in
    };

    // hide is the tiling sheet every shape is surfaced with. It is multiplied
    // by the colour the shape asked for, so one sheet does hide, plate, fin
    // and jaw - which is the same bargain every sprite in the game makes with
    // the relief pass.
    void drawLeviathan(sf::RenderTarget& target, const Body& body, const sf::Texture& hide);
}
