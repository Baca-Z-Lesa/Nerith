#include "entities/Boss.h"

#include "core/Assets.h"
#include "core/Config.h"
#include "core/Random.h"

#include <algorithm>
#include <cmath>

namespace
{
    int scaled(int base, float multiplier)
    {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(base) * multiplier)));
    }

    sf::Vector2f unit(sf::Angle angle)
    {
        return { std::cos(angle.asRadians()), std::sin(angle.asRadians()) };
    }

    sf::Vector2f safeNormal(sf::Vector2f v, sf::Vector2f fallback = { 1.0f, 0.0f })
    {
        return v.lengthSquared() > 0.000001f ? v.normalized() : fallback;
    }
}

Boss::Boss(const Assets& assets, BossId id, sf::Vector2f position, const EnemyScaling& scaling)
    : m_def(&bossDefFor(id))
    , m_id(id)
    , m_heading(1.0f, 0.0f)
    , m_playerPosition(position)
    , m_strikeTarget(position)
    , m_confineCentre(position)
    , m_phase(Phase::Stalk)
    , m_phaseTimer(config::BossStalkTime)
    , m_specialTimer(config::BossSpecialFirst)
    , m_attackTimer(1.0f)
    , m_time(0.0f)
    , m_bellPulse(0.0f)
    , m_visibility(1.0f)
    , m_strafe(rng::range(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f)
    , m_spiralAngle(rng::range(0.0f, 360.0f))
    , m_act(0)
    , m_summonTimer(config::BossSummonInterval)
    , m_closeTimer(config::BossClosingInterval)
    , m_rhythmOpen(false)
    , m_enraged(false)
    , m_hasEntered(false)
    , m_dead(false)
    , m_contactDamage(m_def->contactDamage + scaling.bonusDamage)
    , m_salvage(m_def->salvage)
    , m_speed(m_def->speed * scaling.speed)
    , m_startingOrganHealth(0)
{
    // The body: a head plus however many links trail behind it, each a little
    // smaller than the one in front so the animal tapers.
    const int links = std::max(0, m_def->segmentCount);
    m_segments.reserve(static_cast<std::size_t>(links) + 1);

    for (int i = 0; i <= links; ++i)
    {
        const bool head = i == 0;
        const sf::Texture& texture = assets.boss(head ? m_def->headTexture : m_def->segmentTexture);

        float scale = head ? m_def->headScale : m_def->segmentScale;
        for (int t = 1; t < i; ++t)
            scale *= m_def->segmentTaper;

        Segment segment{ position - m_heading * (m_def->segmentSpacing * static_cast<float>(i)),
                         sf::degrees(0.0f), scale, sf::Sprite(texture) };

        segment.sprite.setOrigin(sf::Vector2f(texture.getSize()) / 2.0f);
        segment.sprite.setScale({ scale, scale });
        m_segments.push_back(std::move(segment));
    }

    // The parts that make it an animal rather than a shape.
    m_parts.reserve(m_def->partCount);
    for (std::size_t i = 0; i < m_def->partCount; ++i)
    {
        const BossPartDef& def = m_def->parts[i];
        const sf::Texture& texture = assets.boss(def.texture);
        const int health = scaled(def.health, scaling.health);

        Part part{ def, health, health, 0.0f, true, position, position,
                   sf::degrees(0.0f), sf::Sprite(texture) };

        const sf::Vector2f size(texture.getSize());
        switch (def.role)
        {
            case PartRole::Tentacle: part.sprite.setOrigin({ size.x / 2.0f, 0.0f }); break;
            case PartRole::Fin:      part.sprite.setOrigin({ 0.0f, size.y / 2.0f }); break;
            default:                 part.sprite.setOrigin(size / 2.0f); break;
        }
        part.sprite.setScale({ def.scale, def.scale });

        if (def.role == PartRole::Organ)
            m_startingOrganHealth += health;

        m_parts.push_back(std::move(part));
    }

    refreshVitals();
    followChain();
    placeParts(0.0f);
}

// ---------------------------------------------------------------------------
//  Body
// ---------------------------------------------------------------------------

void Boss::refreshVitals()
{
    int eyes = 0, eyesAlive = 0, fins = 0, finsAlive = 0;
    m_vitals.emitters = 0;
    m_vitals.organs = 0;

    for (const Part& part : m_parts)
    {
        switch (part.def.role)
        {
            case PartRole::Eye:      ++eyes; if (part.alive) ++eyesAlive; break;
            case PartRole::Fin:      ++fins; if (part.alive) ++finsAlive; break;
            case PartRole::Tentacle: if (part.alive) ++m_vitals.emitters; break;
            case PartRole::Organ:    if (part.alive) ++m_vitals.organs; break;
            case PartRole::Armour:   break;
        }
    }

    // Blind it and it still lunges, it just stops lunging where you are.
    m_vitals.tracking = eyes > 0
                            ? 0.25f + 0.75f * static_cast<float>(eyesAlive) / static_cast<float>(eyes)
                            : 1.0f;
    // Clipped fins cost it speed and, through steer(), its ability to turn.
    m_vitals.mobility = fins > 0
                            ? 0.45f + 0.55f * static_cast<float>(finsAlive) / static_cast<float>(fins)
                            : 1.0f;
}

void Boss::steer(float deltaTime, sf::Vector2f target, float turnScale)
{
    const sf::Vector2f desired = target - m_segments[0].position;
    if (desired.lengthSquared() <= 0.000001f)
        return;

    const sf::Angle wanted = desired.angle();
    const sf::Angle current = m_heading.angle();
    const float delta = (wanted - current).wrapSigned().asDegrees();

    const float allowed = config::BossTurnRate * m_vitals.mobility * turnScale * deltaTime;
    const float step = std::clamp(delta, -allowed, allowed);

    m_heading = m_heading.rotatedBy(sf::degrees(step));
    m_segments[0].angle = m_heading.angle();
}

void Boss::advance(float deltaTime, float speed)
{
    m_segments[0].position += m_heading * speed * deltaTime;
}

void Boss::followChain()
{
    Segment& head = m_segments[0];
    head.sprite.setPosition(head.position);
    head.sprite.setRotation(head.angle);

    for (std::size_t i = 1; i < m_segments.size(); ++i)
    {
        Segment& previous = m_segments[i - 1];
        Segment& segment = m_segments[i];

        const sf::Vector2f delta = previous.position - segment.position;
        const float distance = delta.length();
        const float spacing = m_def->segmentSpacing * segment.scale;

        if (distance > 0.0001f)
        {
            segment.angle = delta.angle();
            // Pulling each link up behind the one ahead is what makes the whole
            // animal swim instead of sliding around as a rigid shape.
            if (distance > spacing)
                segment.position = previous.position - (delta / distance) * spacing;
        }

        segment.sprite.setPosition(segment.position);
        segment.sprite.setRotation(segment.angle);
    }
}

void Boss::placeParts(float deltaTime)
{
    const auto tint = [this](float flash) {
        sf::Color colour = flash > 0.0f ? config::EnemyHitFlashTint : sf::Color::White;
        colour.a = static_cast<std::uint8_t>(std::clamp(m_visibility, 0.0f, 1.0f) * 255.0f);
        return colour;
    };

    for (Segment& segment : m_segments)
    {
        float scale = segment.scale;
        if (m_def->body == BossBody::Bell)
            scale *= 1.0f - 0.16f * m_bellPulse; // the bell visibly clenches

        segment.sprite.setScale({ scale, scale });
        segment.sprite.setColor(tint(0.0f));
    }

    for (Part& part : m_parts)
    {
        if (part.flash > 0.0f)
            part.flash = std::max(0.0f, part.flash - deltaTime);

        const std::size_t index = std::min(static_cast<std::size_t>(std::max(0, part.def.segment)),
                                           m_segments.size() - 1);
        const Segment& segment = m_segments[index];

        const sf::Vector2f local{ part.def.along, part.def.lateral };
        part.anchor = segment.position + local.rotatedBy(segment.angle);
        part.angle = segment.angle + sf::degrees(part.def.baseAngle);

        if (part.def.role == PartRole::Tentacle)
        {
            // Tentacles sway on their own beat and their hitbox sits halfway
            // down the arm rather than at the root nobody aims for.
            const float sway = 11.0f * std::sin(m_time * 1.7f + part.def.baseAngle * 0.05f);
            part.angle += sf::degrees(sway);

            const sf::Vector2f outward = unit(part.angle);
            part.world = part.anchor + outward * config::BossTentacleHitOffset;
            part.sprite.setRotation(part.angle - sf::degrees(90.0f));
        }
        else
        {
            part.world = part.anchor;
            part.sprite.setRotation(part.angle);
        }

        part.sprite.setPosition(part.anchor);

        const float punch = 1.0f + 0.18f * (part.flash / config::BossFlashDuration);
        part.sprite.setScale({ part.def.scale * punch, part.def.scale * punch });
        part.sprite.setColor(tint(part.flash));
    }
}

bool Boss::organExposed(const Part& part) const
{
    // Anything still plated on the same segment keeps that organ sealed.
    for (const Part& other : m_parts)
    {
        if (other.alive && other.def.role == PartRole::Armour && other.def.segment == part.def.segment)
            return false;
    }

    // A bell only opens on its own rhythm, so the window is the fight.
    if (m_def->body == BossBody::Bell)
        return m_rhythmOpen;

    return true;
}

float Boss::aimError() const
{
    const float spread = (1.0f - m_vitals.tracking) * config::BossAimErrorMax;
    return rng::range(-spread, spread);
}

// ---------------------------------------------------------------------------
//  Minds
// ---------------------------------------------------------------------------

void Boss::thinkProwler(float deltaTime, sf::Vector2f player)
{
    const sf::Vector2f head = m_segments[0].position;
    const sf::Vector2f toPlayer = player - head;
    const float distance = toPlayer.length();
    const sf::Vector2f direction = safeNormal(toPlayer, m_heading);

    // Hurt badly enough and it breaks off, circles wide, and comes back angry.
    const bool wasEnraged = m_enraged;
    checkRage();

    if (m_enraged && !wasEnraged && m_phase != Phase::Flee)
    {
        m_phase = Phase::Flee;
        m_phaseTimer = config::BossFleeTime;
    }

    switch (m_phase)
    {
        case Phase::Stalk:
        {
            const sf::Vector2f tangent{ -direction.y * m_strafe, direction.x * m_strafe };
            const float pull = (distance - m_def->preferredDistance) / m_def->preferredDistance;
            // Blinding it does not only spoil the lunge: it can no longer keep
            // its nose on you while it circles either.
            steer(deltaTime, head + tangent * 150.0f + direction * (pull * 200.0f),
                  0.4f + 0.6f * m_vitals.tracking);
            advance(deltaTime, m_speed * m_vitals.mobility);

            if (m_attackTimer <= config::BossTellTime && m_tellTimer <= 0.0f)
                beginTell(std::max(0.05f, m_attackTimer), direction, false);

            m_attackTimer -= deltaTime;
            if (m_attackTimer <= 0.0f)
            {
                m_attackTimer = m_def->attackCooldown * aggression();

                const sf::Vector2f muzzle = head + m_heading * m_def->headRadius;
                const sf::Vector2f aim = direction.rotatedBy(sf::degrees(aimError()));

                // The second half of the fight is the same volley, wider and
                // with more in it. All four numbers come from the creature's
                // own rage row rather than from here.
                const int shots = m_def->attackShots
                                  + (m_enraged ? m_def->rage.extraShots : 0);
                const float spread = m_def->attackSpread
                                     * (m_enraged ? m_def->rage.spreadScale : 1.0f);

                const sf::Angle base = aim.angle();
                for (int i = 0; i < shots; ++i)
                {
                    const float step = spread / static_cast<float>(std::max(1, shots - 1));
                    const sf::Angle angle = base
                                            + sf::degrees(-spread / 2.0f
                                                          + step * static_cast<float>(i));
                    m_pending.push_back({ muzzle, unit(angle) });
                }
            }

            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                m_phase = Phase::Wind;
                m_phaseTimer = config::BossWindTime;
                beginTell(config::BossWindTime, direction, true);

            }
            break;
        }

        case Phase::Wind:
            // It stops and coils. This is the tell, and it is deliberately long
            // enough to read and step out of.
            advance(deltaTime, -m_speed * 0.3f);
            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                // Blind eyes are paid for here: the lunge goes where it thinks
                // you are, which stops being where you are.
                m_strikeTarget = head + toPlayer.rotatedBy(sf::degrees(aimError())) * 2.2f;
                m_phase = Phase::Strike;
                m_phaseTimer = config::BossStrikeTime;
            }
            break;

        case Phase::Strike:
            steer(deltaTime, m_strikeTarget, 0.35f); // committed, barely corrects
            advance(deltaTime, m_speed * config::BossChargeSpeed);
            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                m_phase = Phase::Recover;
                m_phaseTimer = config::BossRecoverTime * aggression();
            }
            break;

        case Phase::Recover:
            advance(deltaTime, m_speed * 0.35f);
            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                m_phase = Phase::Stalk;
                m_phaseTimer = config::BossStalkTime * aggression()
                               * (m_enraged ? 0.55f : 1.0f);
                m_strafe = -m_strafe;
            }
            break;

        case Phase::Flee:
            steer(deltaTime, head - toPlayer, 1.0f);
            advance(deltaTime, m_speed * 2.0f);
            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                m_phase = Phase::Stalk;
                m_phaseTimer = config::BossStalkTime * 0.5f;
            }
            break;

        default:
            m_phase = Phase::Stalk;
            m_phaseTimer = config::BossStalkTime;
            break;
    }
}

void Boss::thinkDrifter(float deltaTime, sf::Vector2f player)
{
    checkRage();

    steer(deltaTime, player, 0.7f);
    advance(deltaTime, m_speed * m_vitals.mobility
                           * (m_enraged ? m_def->rage.speedScale : 1.0f));

    m_attackTimer -= deltaTime;

    if (m_rhythmOpen)
    {
        m_bellPulse = std::min(1.0f, m_bellPulse + deltaTime * 3.0f);

        if (m_attackTimer <= 0.0f)
        {
            m_rhythmOpen = false;
            m_attackTimer = m_def->attackCooldown * aggression()
                            * (getCoreFraction() < 0.4f ? 0.65f : 1.0f);
        }
        return;
    }

    m_bellPulse = std::max(0.0f, m_bellPulse - deltaTime * 2.0f);

    // Announced on the way down rather than on the frame it fires. Sized to
    // whatever is actually left, so a creature whose cooldown is shorter than
    // the tell still announces - it simply announces for less long.
    if (m_attackTimer <= config::BossTellTime && m_tellTimer <= 0.0f)
    {
        beginTell(std::max(0.05f, m_attackTimer),
                  safeNormal(player - m_segments[0].position, m_heading), false);
    }

    if (m_attackTimer > 0.0f)
        return;

    // The burst. Every living arm feeds the ring, so cutting them off is what
    // thins the wall of stingers rather than just shortening a bar.
    const sf::Vector2f centre = m_segments[0].position;
    const int ring = 4 + 2 * m_vitals.emitters
                     + (m_enraged ? m_def->rage.extraShots : 0);

    for (int i = 0; i < ring; ++i)
    {
        const sf::Angle angle = sf::degrees(360.0f * static_cast<float>(i) / static_cast<float>(ring)
                                            + m_time * 24.0f);
        m_pending.push_back({ centre + unit(angle) * (m_def->headRadius * 0.7f), unit(angle) });
    }

    // Enraged, a second ring goes out between the spokes of the first, so the
    // gaps the player had learned to stand in close up.
    if (m_enraged)
    {
        for (int i = 0; i < ring; ++i)
        {
            const sf::Angle angle = sf::degrees(
                360.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(ring)
                - m_time * 18.0f);
            m_pending.push_back({ centre + unit(angle) * (m_def->headRadius * 0.45f),
                                  unit(angle) });
        }
    }

    for (const Part& part : m_parts)
    {
        if (!part.alive || part.def.role != PartRole::Tentacle)
            continue;

        const sf::Vector2f tip = part.anchor + unit(part.angle) * (config::BossTentacleHitOffset * 1.8f);
        m_pending.push_back({ tip, safeNormal(player - tip) });
    }

    // Fewer arms means it has to hang open longer to recharge, which is the
    // window the player is fighting for.
    const float openTime = 1.0f + 0.3f * static_cast<float>(6 - m_vitals.emitters);
    m_rhythmOpen = true;
    m_attackTimer = openTime;
}

void Boss::thinkBurrower(float deltaTime, sf::Vector2f player)
{
    checkRage();

    switch (m_phase)
    {
        case Phase::Stalk:
            m_visibility = std::min(1.0f, m_visibility + deltaTime * 3.0f);

            // A steady turn, so it carves a long arc through the arena rather
            // than beelining at anyone.
            m_heading = m_heading.rotatedBy(sf::degrees(config::BossSweepRate * m_strafe * deltaTime));
            m_segments[0].angle = m_heading.angle();
            advance(deltaTime, m_speed * m_vitals.mobility);

            if (m_attackTimer <= config::BossTellTime && m_tellTimer <= 0.0f)
            {
                beginTell(std::max(0.05f, m_attackTimer),
                          safeNormal(player - m_segments[0].position, m_heading), false);
            }

            m_attackTimer -= deltaTime;
            if (m_attackTimer <= 0.0f)
            {
                m_attackTimer = m_def->attackCooldown
                                * (m_enraged ? m_def->rage.cooldownScale : 1.0f);

                const sf::Vector2f muzzle = m_segments[0].position + m_heading * m_def->headRadius;
                const sf::Vector2f aim = safeNormal(player - muzzle, m_heading)
                                             .rotatedBy(sf::degrees(aimError()));
                const sf::Angle base = aim.angle();

                const int shots = m_def->attackShots + (m_enraged ? m_def->rage.extraShots : 0);
                const float spread = m_def->attackSpread
                                     * (m_enraged ? m_def->rage.spreadScale : 1.0f);

                for (int i = 0; i < shots; ++i)
                {
                    const float step = spread / static_cast<float>(std::max(1, shots - 1));
                    const sf::Angle angle = base
                                            + sf::degrees(-spread / 2.0f
                                                          + step * static_cast<float>(i));
                    m_pending.push_back({ muzzle, unit(angle) });
                }
            }

            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                m_phase = Phase::Submerged;
                m_phaseTimer = config::BossSubmergeTime;
            }
            break;

        case Phase::Submerged:
            m_visibility = std::max(0.0f, m_visibility - deltaTime * 2.4f);
            steer(deltaTime, player, 1.0f);
            advance(deltaTime, m_speed * 0.8f);

            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                // Erupt beside the player, never on top of them.
                const sf::Angle where = sf::degrees(rng::range(0.0f, 360.0f));
                m_segments[0].position = player + unit(where) * config::BossEmergeDistance;
                m_heading = safeNormal(player - m_segments[0].position);
                m_segments[0].angle = m_heading.angle();

                for (std::size_t i = 1; i < m_segments.size(); ++i)
                {
                    m_segments[i].position = m_segments[0].position
                                             - m_heading * (m_def->segmentSpacing * static_cast<float>(i));
                    m_segments[i].angle = m_segments[0].angle;
                }

                m_phase = Phase::Rising;
                m_phaseTimer = config::BossRiseTime;

                // Coming up is itself an attack, and the whole rise is its own
                // warning: the silt lifts before the stingers do.
                beginTell(config::BossRiseTime, m_heading, false);
            }
            break;

        case Phase::Rising:
            m_visibility = std::min(1.0f, m_visibility + deltaTime * 2.4f);
            m_phaseTimer -= deltaTime;
            if (m_phaseTimer <= 0.0f)
            {
                // Coming up throws silt and stingers in every direction.
                const sf::Vector2f centre = m_segments[0].position;
                for (int i = 0; i < 12; ++i)
                {
                    const sf::Angle angle = sf::degrees(360.0f * static_cast<float>(i) / 12.0f);
                    m_pending.push_back({ centre + unit(angle) * m_def->headRadius, unit(angle) });
                }

                m_phase = Phase::Stalk;
                m_phaseTimer = config::BossSurfaceTime * (getCoreFraction() < 0.4f ? 0.7f : 1.0f);
                m_strafe = -m_strafe;
                m_attackTimer = 0.4f;
            }
            break;

        default:
            m_phase = Phase::Stalk;
            m_phaseTimer = config::BossSurfaceTime;
            break;
    }
}

void Boss::thinkSpiral(float deltaTime, sf::Vector2f player)
{
    const sf::Vector2f head = m_segments[0].position;
    const sf::Vector2f toPlayer = player - head;
    const float distance = toPlayer.length();
    const sf::Vector2f direction = safeNormal(toPlayer, m_heading);

    checkRage();
    updateActs();

    // Act two: it starts pulling its own dead off the column and sending them
    // at you. The column is what the creature is made of, so this visibly
    // costs it something.
    if (m_act >= 1)
    {
        m_summonTimer -= deltaTime;
        if (m_summonTimer <= 0.0f)
        {
            m_summonTimer = config::BossSummonInterval;
            m_summons.push_back(config::BossSummonType);
        }
    }

    // Act three: the basin closes. This runs alongside the spiral rather than
    // instead of it, which is what makes the last third of the fight the last
    // third rather than a different fight.
    if (m_act >= 2)
    {
        m_closeTimer -= deltaTime;
        if (m_closeTimer <= 0.0f)
        {
            m_closeTimer = config::BossClosingInterval;
            closeTheBasin(m_confineCentre);
        }
    }

    // It never closes and it never runs. It holds a ring around the player and
    // keeps turning, so the fight is about reading the sweep rather than about
    // dodging one telegraphed lunge.
    const sf::Vector2f tangent{ -direction.y * m_strafe, direction.x * m_strafe };
    const float pull = (distance - m_def->preferredDistance) / m_def->preferredDistance;

    steer(deltaTime, head + tangent * 190.0f + direction * (pull * 260.0f),
          0.5f + 0.5f * m_vitals.tracking);
    advance(deltaTime, m_speed * m_vitals.mobility * (m_enraged ? m_def->rage.speedScale : 1.0f));

    // Enraged, the arms reverse. Everything the player learned about which way
    // to run stops being true, which is the whole of the second phase.
    const float turn = config::BossSpiralTurn * (m_enraged ? -1.6f : 1.0f);
    m_spiralAngle += turn * deltaTime;

    if (m_attackTimer <= config::BossTellTime && m_tellTimer <= 0.0f)
    {
        beginTell(std::max(0.05f, m_attackTimer), direction, false);
    }

    m_attackTimer -= deltaTime;
    if (m_attackTimer > 0.0f)
        return;

    m_attackTimer = m_def->attackCooldown * aggression();

    // One arm per living lamp, so putting a lamp out genuinely thins the
    // spiral rather than only shortening a bar. Blind it completely and it is
    // down to a single arm it can barely aim.
    const int lamps = std::max(1, m_vitals.emitters > 0 ? m_vitals.emitters : lampsAlive());
    const int arms = std::max(1, lamps + (m_enraged ? m_def->rage.extraShots : 0)
                                     + m_act * config::BossActArmBonus);

    for (int i = 0; i < arms; ++i)
    {
        const float spoke = 360.0f * static_cast<float>(i) / static_cast<float>(arms);
        const sf::Angle angle = sf::degrees(m_spiralAngle + spoke + aimError());
        m_pending.push_back({ head + unit(angle) * (m_def->headRadius * 0.55f), unit(angle) });
    }
}

std::vector<BossMove> Boss::takeMoves()
{
    std::vector<BossMove> out;
    out.swap(m_moves);
    return out;
}

float Boss::aggression() const
{
    // Compounding rather than additive: the difference between act two and act
    // three has to be felt, not measured. Rage stacks on top of it, so a
    // cornered enraged creature is genuinely relentless.
    float pressure = 1.0f;
    for (int i = 0; i < m_act; ++i)
        pressure *= 1.0f - config::BossActPressure;

    if (m_enraged)
        pressure *= m_def->rage.cooldownScale;

    return std::max(0.35f, pressure);
}

void Boss::considerSpecial(float deltaTime, sf::Vector2f player)
{
    // Nothing while it is under the silt or running: a special is a commitment
    // and a creature that is not present cannot commit to anything.
    if (m_phase == Phase::Submerged || m_phase == Phase::Rising || m_phase == Phase::Flee)
        return;

    m_specialTimer -= deltaTime;
    m_ultraTimer = std::max(0.0f, m_ultraTimer - deltaTime);
    if (m_specialTimer > 0.0f)
        return;

    m_specialTimer = config::BossSpecialInterval * aggression();

    const sf::Vector2f head = m_segments[0].position;
    const sf::Vector2f toPlayer = safeNormal(player - head, m_heading);

    // Two moves each, alternating, so neither becomes the thing the player
    // waits for and the other becomes a surprise.
    const bool second = (m_specialTurn++ % 2) != 0;

    // Hurt badly enough, it stops repeating itself. Each mind keeps a move it
    // has not shown you, and it only comes out in the second half of the
    // fight - so the turn is something new rather than the same thing faster.
    //
    // All four take several seconds to play out rather than landing in a
    // frame, so the answer is a route through rather than a dodge, which is
    // the difference between a boss that hits harder and a harder boss.
    if (m_act >= config::BossUltraFromAct && m_ultraTimer <= 0.0f)
    {
        m_ultraTimer = config::BossUltraInterval;

        switch (m_def->mind)
        {
            case BossMind::Prowler:
                m_moves.push_back({ BossSpecial::Closing, player, toPlayer, 0.0f });
                break;
            case BossMind::Drifter:
                m_moves.push_back({ BossSpecial::Curtain, head, toPlayer, 0.0f });
                break;
            case BossMind::Burrower:
                m_moves.push_back({ BossSpecial::Fissure, head, toPlayer, 0.0f });
                break;
            case BossMind::Spiral:
                m_moves.push_back({ BossSpecial::Rake, head, toPlayer, 0.0f });
                break;
        }

        beginTell(config::BossTellTime, toPlayer, false);
        return;
    }

    switch (m_def->mind)
    {
        case BossMind::Prowler:
            if (second)
            {
                // The tail. It does not need to be looking at you.
                m_moves.push_back({ BossSpecial::Sweep, head, m_heading, 0.0f });
            }
            else
            {
                // The lunge that follows is the grab. Announced as a charge,
                // because being caught by something you did not see coming is
                // not a fight, it is a punishment.
                m_strikeTarget = head + toPlayer * 2.2f;
                m_phase = Phase::Wind;
                m_phaseTimer = config::BossWindTime;
                beginTell(config::BossWindTime, toPlayer, true);
                m_moves.push_back({ BossSpecial::Grab, head, toPlayer, 1.0f });
            }
            break;

        case BossMind::Drifter:
            m_moves.push_back(second
                                  ? BossMove{ BossSpecial::Tide, head, toPlayer, 0.0f }
                                  : BossMove{ BossSpecial::Seed, head, toPlayer, 0.0f });
            beginTell(config::BossTellTime, toPlayer, false);
            break;

        case BossMind::Burrower:
            if (second)
            {
                m_moves.push_back({ BossSpecial::Silt, head, m_heading, 0.0f });
            }
            else
            {
                // Straight up through the floor under you. The warning is the
                // floor itself, and it is generous, because the alternative is
                // an unavoidable hit.
                m_moves.push_back({ BossSpecial::Erupt, player, toPlayer, 0.0f });
            }
            beginTell(config::BossTellTime, toPlayer, false);
            break;

        case BossMind::Spiral:
            m_moves.push_back(second
                                  ? BossMove{ BossSpecial::Hurl, head, toPlayer, 0.0f }
                                  : BossMove{ BossSpecial::Well, head, toPlayer, 0.0f });
            beginTell(config::BossTellTime, toPlayer, false);
            break;
    }
}

std::vector<EnemyType> Boss::takeSummons()
{
    std::vector<EnemyType> out;
    out.swap(m_summons);
    return out;
}

void Boss::updateActs()
{
    // Two thresholds, so a fight has three shapes rather than two. Crossing one
    // is one-way: a creature that has decided to stop holding back does not
    // start again because it got a moment's peace.
    const float core = getCoreFraction();

    if (m_act < 1 && core < config::BossActTwo)
        m_act = 1;
    if (m_act < 2 && core < config::BossActThree)
        m_act = 2;
}

void Boss::closeTheBasin(sf::Vector2f centre)
{
    // Inward off the rim rather than outward from the body. There is no longer
    // anywhere in the basin that is far enough away, which is the point: the
    // last act of the last creature is not survived by backing off.
    const int arms = config::BossClosingArms;

    for (int i = 0; i < arms; ++i)
    {
        const sf::Angle angle = sf::degrees(360.0f * static_cast<float>(i)
                                                / static_cast<float>(arms)
                                            + m_time * 22.0f);
        const sf::Vector2f out = unit(angle);

        m_pending.push_back({ centre + out * config::BossClosingRadius, -out });
    }
}

int Boss::lampsAlive() const
{
    int count = 0;

    for (const Part& part : m_parts)
    {
        if (part.alive && part.def.role == PartRole::Eye)
            ++count;
    }
    return count;
}

void Boss::checkRage()
{
    // One place, so every creature's second phase starts at the same point in
    // its life rather than at whatever each think function happened to test.
    if (!m_enraged && getCoreFraction() < config::BossEnrageThreshold)
        m_enraged = true;
}

// ---------------------------------------------------------------------------
//  Frame
// ---------------------------------------------------------------------------

void Boss::update(float deltaTime, sf::Vector2f playerPosition)
{
    m_time += deltaTime;
    m_playerPosition = playerPosition;

    // Counted down here rather than in each mind, so a tell cannot be left
    // hanging by whichever branch happened to start it.
    m_tellTimer = std::max(0.0f, m_tellTimer - deltaTime);

    switch (m_def->mind)
    {
        case BossMind::Prowler:  thinkProwler(deltaTime, playerPosition); break;
        case BossMind::Drifter:  thinkDrifter(deltaTime, playerPosition); break;
        case BossMind::Burrower: thinkBurrower(deltaTime, playerPosition); break;
        case BossMind::Spiral:   thinkSpiral(deltaTime, playerPosition); break;
    }

    // After the mind, not before: a special can override the phase the mind
    // just chose, and it has to be the one that wins.
    considerSpecial(deltaTime, playerPosition);

    followChain();
    placeParts(deltaTime);
}

void Boss::beginTell(float seconds, sf::Vector2f aim, bool lunge)
{
    m_tellTimer = seconds;
    m_tellLength = std::max(0.01f, seconds);
    m_tellAim = aim;
    m_tellLunge = lunge;
}

float Boss::getTell() const
{
    if (m_tellTimer <= 0.0f)
        return 0.0f;

    return std::clamp(1.0f - m_tellTimer / m_tellLength, 0.0f, 1.0f);
}

void Boss::confine(sf::Vector2f center, float maxDistance)
{
    // Remembered, because the last act throws shots inward off the rim of the
    // basin and needs to know where the middle of it is.
    m_confineCentre = center;

    const sf::Vector2f delta = m_segments[0].position - center;

    if (delta.lengthSquared() <= maxDistance * maxDistance)
    {
        m_hasEntered = true;
        return;
    }

    if (!m_hasEntered)
        return; // still swimming in from off screen

    m_segments[0].position = center + delta.normalized() * maxDistance;

    // Turn it back inwards too, otherwise it grinds along the boundary trying
    // to keep going the way it was.
    m_heading = safeNormal(center - m_segments[0].position, m_heading);
    m_segments[0].angle = m_heading.angle();

    followChain();
    placeParts(0.0f);
}

// ---------------------------------------------------------------------------
//  Damage
// ---------------------------------------------------------------------------

int Boss::partAt(sf::Vector2f position, float radius) const
{
    if (!isVulnerable())
        return -1;

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const Part& part = m_parts[i];

        if (!part.alive)
            continue;

        if (part.def.role == PartRole::Organ && !organExposed(part))
            continue;

        const float reach = radius + part.def.radius;

        if ((part.world - position).lengthSquared() <= reach * reach)
            return static_cast<int>(i);
    }

    return -1;
}

Boss::Hit Boss::tryHit(sf::Vector2f position, float radius, int damage, float weakPointBonus)
{
    Hit result;

    if (!isVulnerable())
        return result; // nothing lands on something that is under the silt

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        Part& part = m_parts[i];
        if (!part.alive)
            continue;

        if (part.def.role == PartRole::Organ && !organExposed(part))
            continue;

        const float reach = radius + part.def.radius;
        if ((part.world - position).lengthSquared() > reach * reach)
            continue;

        const bool organ = part.def.role == PartRole::Organ;
        const int dealt = organ
                              ? std::max(1, static_cast<int>(std::lround(
                                                static_cast<float>(damage) * (1.0f + weakPointBonus))))
                              : damage;

        part.health -= dealt;
        part.flash = config::BossFlashDuration;

        result.hit = true;
        result.at = part.world;
        result.partIndex = i;
        result.wasOrgan = organ;

        if (part.health <= 0)
        {
            part.alive = false;
            result.destroyedPart = true;
            refreshVitals();

            if (organ && m_vitals.organs == 0)
            {
                m_dead = true;
                result.killedBoss = true;
            }
        }
        return result;
    }

    if (!bodyBlocks(position, radius))
        return result;

    result.hit = true;
    result.at = position;

    // Once nothing is shielding it any more, the whole animal is soft: a shot
    // anywhere on the body chips the nearest reachable organ. While the plating
    // holds, the same shot sparks off and does nothing, which is what teaches
    // the player to break the plate before emptying into the hide.
    const std::size_t organ = nearestExposedOrgan(position);
    if (organ >= m_parts.size())
    {
        result.blocked = true;
        return result;
    }

    Part& part = m_parts[organ];
    const int dealt = std::max(1, static_cast<int>(std::lround(
                                      static_cast<float>(damage) * config::BossBodyHitFactor
                                      * (1.0f + weakPointBonus))));

    part.health -= dealt;
    part.flash = config::BossFlashDuration;

    result.partIndex = organ;
    result.wasOrgan = true;

    if (part.health <= 0)
    {
        part.alive = false;
        result.destroyedPart = true;
        refreshVitals();

        if (m_vitals.organs == 0)
        {
            m_dead = true;
            result.killedBoss = true;
        }
    }

    return result;
}

bool Boss::bodyBlocks(sf::Vector2f position, float radius) const
{
    for (const Segment& segment : m_segments)
    {
        const float reach = radius + m_def->headRadius * segment.scale * config::BossBlockRadiusFactor;
        if ((segment.position - position).lengthSquared() <= reach * reach)
            return true;
    }
    return false;
}

std::size_t Boss::nearestExposedOrgan(sf::Vector2f position) const
{
    std::size_t best = m_parts.size();
    float bestDistance = 0.0f;

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const Part& part = m_parts[i];
        if (!part.alive || part.def.role != PartRole::Organ || !organExposed(part))
            continue;

        const float distance = (part.world - position).lengthSquared();
        if (best >= m_parts.size() || distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }

    return best;
}

void Boss::splashNeighbours(std::size_t partIndex, int damage)
{
    if (partIndex >= m_parts.size())
        return;

    const sf::Vector2f source = m_parts[partIndex].world;

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        if (i == partIndex)
            continue;

        Part& part = m_parts[i];
        if (!part.alive)
            continue;

        if ((part.world - source).lengthSquared() > config::BossChainRadius * config::BossChainRadius)
            continue;

        part.health -= damage;
        part.flash = config::BossFlashDuration;

        if (part.health <= 0)
        {
            part.alive = false;
            refreshVitals();

            if (part.def.role == PartRole::Organ && m_vitals.organs == 0)
                m_dead = true;
        }
    }
}

bool Boss::touches(sf::Vector2f position, float radius) const
{
    for (const Segment& segment : m_segments)
    {
        const float reach = radius + m_def->headRadius * segment.scale;
        if ((segment.position - position).lengthSquared() <= reach * reach)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Queries
// ---------------------------------------------------------------------------

std::vector<BossShot> Boss::takeShots()
{
    std::vector<BossShot> out;
    out.swap(m_pending);
    return out;
}

sf::Vector2f Boss::getPosition() const
{
    return m_segments.empty() ? m_playerPosition : m_segments[0].position;
}

float Boss::getRadius() const
{
    return m_def->headRadius;
}

int Boss::getSalvage() const
{
    return m_salvage;
}

bool Boss::isAlive() const
{
    return !m_dead;
}

bool Boss::isVulnerable() const
{
    return m_visibility > 0.55f;
}

float Boss::getCoreFraction() const
{
    if (m_startingOrganHealth <= 0)
        return m_dead ? 0.0f : 1.0f;

    int left = 0;
    for (const Part& part : m_parts)
    {
        if (part.def.role == PartRole::Organ && part.alive)
            left += std::max(0, part.health);
    }
    return static_cast<float>(left) / static_cast<float>(m_startingOrganHealth);
}

const char* Boss::getName() const { return m_def->name; }
const char* Boss::getBrief() const { return m_def->brief; }
const char* Boss::getShotTexture() const { return m_def->shotTexture; }

std::string Boss::getStateLabel() const
{
    if (!isVulnerable())
        return "BURIED";

    // Whatever is currently stopping the player from hurting it comes first.
    for (const Part& part : m_parts)
    {
        if (part.def.role == PartRole::Organ && part.alive && !organExposed(part))
            return m_def->body == BossBody::Bell ? "BELL CLENCHED" : "PLATING HOLDS";
    }

    switch (m_def->mind)
    {
        case BossMind::Prowler:
            switch (m_phase)
            {
                case Phase::Wind:    return "IT IS COMING";
                case Phase::Strike:  return "CHARGING";
                case Phase::Recover: return "WINDED";
                case Phase::Flee:    return "IT IS RUNNING";
                default:             return m_enraged ? "ENRAGED" : "CIRCLING";
            }
        case BossMind::Drifter: return "NUCLEUS EXPOSED";
        case BossMind::Burrower: return "SURFACED";
        case BossMind::Spiral:
            switch (m_act)
            {
                case 0:  return "WINDING";
                case 1:  return "IT IS CALLING ITS DEAD";
                default: return "THE BASIN IS CLOSING";
            }
    }
    return "";
}

bool Boss::isPartAlive(std::size_t index) const
{
    return index < m_parts.size() && m_parts[index].alive;
}

float Boss::getPartFraction(std::size_t index) const
{
    if (index >= m_parts.size())
        return 0.0f;

    const Part& part = m_parts[index];
    return static_cast<float>(std::max(0, part.health)) / static_cast<float>(std::max(1, part.maxHealth));
}

PartRole Boss::getPartRole(std::size_t index) const
{
    return index < m_parts.size() ? m_parts[index].def.role : PartRole::Armour;
}

sf::Vector2f Boss::getPartPosition(std::size_t index) const
{
    return index < m_parts.size() ? m_parts[index].world : getPosition();
}

// ---------------------------------------------------------------------------
//  Drawing
// ---------------------------------------------------------------------------

void Boss::draw(sf::RenderTarget& target) const
{
    if (m_visibility <= 0.02f)
        return;

    // Arms hang behind the body, the body hides the tail, and the organs sit on
    // top so the thing the player is aiming at is never buried.
    for (const Part& part : m_parts)
    {
        if (part.alive && part.def.role == PartRole::Tentacle)
            target.draw(part.sprite);
    }

    for (std::size_t i = m_segments.size(); i-- > 0;)
        target.draw(m_segments[i].sprite);

    for (const Part& part : m_parts)
    {
        if (part.alive && part.def.role != PartRole::Tentacle && part.def.role != PartRole::Organ)
            target.draw(part.sprite);
    }

    for (const Part& part : m_parts)
    {
        if (part.alive && part.def.role == PartRole::Organ && organExposed(part))
            target.draw(part.sprite);
    }
}
