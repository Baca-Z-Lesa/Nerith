#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>

// Geometry that is cut once and then looked at for the rest of the dive.
//
// A vertex array lives in system memory, so drawing one hands the whole thing
// to the driver again on every single frame it appears. The basin is half a
// million vertices of rock and the seabed another hundred thousand grains of
// sand, and none of it has moved since the level was built - so that was ten
// megabytes a frame spent redescribing a picture that was already correct.
//
// Parked on the card instead, the same triangles cost one call apiece. The
// array is still what builds it, because appending to a buffer is not a thing
// you can do, and it stays around because the next depth rebuilds from it.
namespace mesh
{
    // Hands a finished mesh to the graphics card. Returns false on a machine
    // whose driver will not have it, and the caller then goes on drawing the
    // array it built: the same picture, just paid for every frame.
    inline bool parkOnCard(const sf::VertexArray& from, sf::VertexBuffer& to)
    {
        if (!sf::VertexBuffer::isAvailable() || from.getVertexCount() == 0)
            return false;

        to.setPrimitiveType(from.getPrimitiveType());

        if (to.getVertexCount() != from.getVertexCount() && !to.create(from.getVertexCount()))
            return false;

        return to.update(&from[0]);
    }
}
