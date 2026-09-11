#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

// Every upgrade wears a drawn mark rather than the first letters of its name,
// because "PV" tells nobody anything. The marks are built out of lines, discs
// and triangles in code instead of being an image: they stay crisp at any zoom,
// they cost no texture, and a new one is a function rather than a new asset to
// keep in step with the table.
enum class Icon
{
    None,

    // --- weapons ---
    Jet,          // a dart of water
    Muzzle,       // a barrel with a flash
    RapidFire,    // three darts abreast
    Pierce,       // one dart through two rings
    Split,        // one line forking into three
    Cannon,       // a heavy barrel
    TwinCannon,
    Crosshair,
    Crit,         // a crosshair with a spark
    Charge,       // a wound spring

    // --- torpedoes ---
    Torpedo,
    Warhead,      // a torpedo with a burst on the nose
    Reload,       // a circular arrow
    Seeker,       // a torpedo with a tracking arc
    Cluster,      // a burst scattering pellets
    Blast,        // concentric shock rings

    // --- hull ---
    Hull,         // a plated arc
    Plate,        // a riveted square
    Heart,        // the health pip, doubled
    Mercy,        // a shield with a clock hand
    Vent,         // an upward burst through a grate
    Repair,       // a wrench

    // --- engine ---
    Propeller,
    Dash,         // a chevron trail
    Shock,        // a ring of spikes
    Ghost,        // a dashed outline

    // --- sonar ---
    Sonar,        // concentric arcs from a point
    Map,          // a grid with a mark on it
    Eye,
    Mark,         // a bracket around a dot

    // --- defense ---
    Shield,
    ShieldTwo,    // two shields
    Reactive,     // a shield throwing sparks
    Orbit,        // motes circling a hull

    // --- mutation ---
    Bleed,        // three falling drops
    Fang,
    Spore,        // a seed with a halo
    Drone,        // a small hull with a ring round it
    Hive,         // three drones
    Predator,     // a slitted eye
    Abyssal,      // a body with too many limbs
    Corpse,       // a burst silhouette

    // --- salvage ---
    Salvage,      // a bolt and a nut
    Scrap,        // a torn plate
    Crate,

    // --- lamp ---
    Lamp,         // a cone of light
    Beacon,       // a lamp with rays
    Blackout,     // a lamp struck through

    // --- the root ---
    Sub,

    Count
};

namespace icons
{
    // Appends the mark centred on `centre`, sized so it fits a box of
    // `size` x `size`. Thickness scales with the size so a zoomed out tree
    // still reads.
    void append(sf::VertexArray& vertices, Icon icon, sf::Vector2f centre, float size,
                sf::Color colour);
}
