#include "entities/ShipVisuals.h"

#include "core/Assets.h"
#include "core/Config.h"

#include <cmath>
#include <cstdint>

ShipVisuals::ShipVisuals(const Assets& assets)
    : m_shield(config::ShieldRadius, 28)
    , m_shieldUp(false)
    , m_time(0.0f)
{
    // Order here is draw order within each layer.
    const struct
    {
        ShipPart part;
        sf::Vector2f offset;
        bool under;
    } layout[] = {
        { ShipPart::TorpedoTube,  config::MountTorpedo, true },
        { ShipPart::BigEngine,    config::MountEngine,  true },
        { ShipPart::ArmourPlates, config::MountArmour,  false },
        { ShipPart::HeavyCannon,  config::MountCannon,  false },
        { ShipPart::TwinCannon,   config::MountTwin,    false },
        { ShipPart::SonarDish,    config::MountSonar,   false },
    };

    m_mounts.reserve(std::size(layout));
    for (const auto& entry : layout)
    {
        Mount mount{ entry.part, entry.offset, entry.under,
                     sf::Sprite(assets.forPart(entry.part)), false };

        mount.sprite.setOrigin(sf::Vector2f(assets.forPart(entry.part).getSize()) / 2.0f);
        mount.sprite.setScale({ config::AttachmentScale, config::AttachmentScale });
        m_mounts.push_back(std::move(mount));
    }

    m_shield.setOrigin({ config::ShieldRadius, config::ShieldRadius });
    m_shield.setFillColor(sf::Color::Transparent);
    m_shield.setOutlineColor(sf::Color::Transparent);
    m_shield.setOutlineThickness(2.5f);
}

void ShipVisuals::update(sf::Vector2f position, sf::Angle heading, const Stats& stats, float deltaTime)
{
    m_time += deltaTime;

    for (Mount& mount : m_mounts)
    {
        mount.fitted = stats.fitted(mount.part);
        if (!mount.fitted)
            continue;

        // The offset is in the sub's own frame, so hardware stays where it was
        // bolted no matter which way the sub is pointing.
        mount.sprite.setPosition(position + (mount.offset * config::AttachmentScale).rotatedBy(heading));
        mount.sprite.setRotation(heading);
    }

    if (!stats.fitted(ShipPart::ShieldBubble))
    {
        m_shield.setOutlineColor(sf::Color::Transparent);
        return;
    }

    m_shield.setPosition(position);

    // Breathing slightly is what stops it reading as a static decal. A spent
    // shield stays visible but goes dull, so the player can see it regrowing.
    const float pulse = 0.5f + 0.5f * std::sin(m_time * 3.4f);
    sf::Color colour = config::ShieldColor;
    colour.a = static_cast<std::uint8_t>(static_cast<float>(colour.a) * (0.55f + 0.45f * pulse));
    m_shield.setOutlineColor(m_shieldUp ? colour : sf::Color(60, 90, 100, 60));
}

void ShipVisuals::drawUnder(sf::RenderTarget& target) const
{
    for (const Mount& mount : m_mounts)
    {
        if (mount.fitted && mount.under)
            target.draw(mount.sprite);
    }
}

void ShipVisuals::drawOver(sf::RenderTarget& target) const
{
    for (const Mount& mount : m_mounts)
    {
        if (mount.fitted && !mount.under)
            target.draw(mount.sprite);
    }

    if (m_shield.getOutlineColor().a > 0)
        target.draw(m_shield);
}
