#include "entities/Boss.h"

#include "core/Assets.h"
#include "entities/BossArt.h"
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
{
    // The body: a head plus however many links trail behind it, each a little
    // smaller than the one in front so the animal tapers.
    const int links = std::max(0, m_def->segmentCount);
    m_segments.reserve(static_cast<std::size_t>(links) + 1);

    for (int i = 0; i <= links; ++i)
    {
        const bool head = i == 0;

        // The last link is a tail where the animal has one. Swapped rather
        // than added, so the body is exactly as long and exactly as hittable
        // as it was: this is what the end of it looks like, not more of it.
        const bool tail = !head && i == links && m_def->tailTexture != nullptr;

        const char* path = head ? m_def->headTexture
                           : tail ? m_def->tailTexture
                                  : m_def->segmentTexture;

        float scale = head ? m_def->headScale : m_def->segmentScale;
        for (int t = 1; t < i; ++t)
            scale *= m_def->segmentTaper;

        Segment segment{ position - m_heading * (m_def->segmentSpacing * static_cast<float>(i)),
                         sf::degrees(0.0f), scale, std::nullopt };

        if (path != nullptr)
        {
            const sf::Texture& texture = assets.boss(path);

            segment.sprite.emplace(texture);
            segment.sprite->setOrigin(sf::Vector2f(texture.getSize()) / 2.0f);
            segment.sprite->setScale({ scale, scale });
        }

        m_segments.push_back(std::move(segment));
    }

    // The sheet a drawn creature is surfaced with. Held rather than looked up
    // every frame, and only for the ones that need it.
    if (m_def->skin != BossSkin::Sprites)
        m_hide = &assets.boss(config::BossHidePath);

    // The parts that make it an animal rather than a shape.
    m_parts.reserve(m_def->partCount);
    for (std::size_t i = 0; i < m_def->partCount; ++i)
    {
        const BossPartDef& def = m_def->parts[i];
        const int health = scaled(def.health, scaling.health);

        Part part{ def, health, health, 0.0f, true, position, position,
                   sf::degrees(0.0f), std::nullopt };

        // A drawn creature's parts are roles and hitboxes and nothing else.
        if (def.texture != nullptr)
        {
            const sf::Texture& texture = assets.boss(def.texture);

            part.sprite.emplace(texture);

            const sf::Vector2f size(texture.getSize());
            switch (def.role)
            {
                case PartRole::Tentacle: part.sprite->setOrigin({ size.x / 2.0f, 0.0f }); break;
                case PartRole::Fin:      part.sprite->setOrigin({ 0.0f, size.y / 2.0f }); break;
                default:                 part.sprite->setOrigin(size / 2.0f); break;
            }
            part.sprite->setScale({ def.scale, def.scale });
        }

        m_parts.push_back(std::move(part));
    }

    // The pool the whole animal lives off. The pieces above are anatomy: they
    // are drawn, they are what its volleys come out of, and not one of them is
    // a thing you take off it.
    m_coreMax = scaled(std::max(1, m_def->coreHealth), scaling.health);
    m_core = m_coreMax;

    refreshVitals();
    holdShape();
    placeParts(0.0f);
}

// ---------------------------------------------------------------------------
//  Body
// ---------------------------------------------------------------------------

void Boss::refreshVitals()
{
    // Nothing degrades, because nothing comes off: it aims as well as it ever
    // did and swims as fast as it ever did until the moment it stops. An
    // octopus without arms is not an octopus - what a second wind changes is
    // what the animal looks like and how hard it comes at you, not how much of
    // it is left.
    m_vitals.tracking = 1.0f;
    m_vitals.mobility = 1.0f;
    m_vitals.emitters = m_def->emitters + m_fallen * 2;
    m_vitals.organs = m_core > 0 ? 1 : 0;

    for (Part& part : m_parts)
        part.alive = true;
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
    float step = std::clamp(delta, -allowed, allowed);

    // And then the flinch: with its plating off it keeps the open side turned
    // away from the sub, so it is answering the player rather than only
    // following them. Added on top of the steer rather than replacing it, so
    // it still gets where it was going - it just arrives side on.
    step += guardBias(m_playerPosition) * deltaTime;

    m_heading = m_heading.rotatedBy(sf::degrees(step));
    m_segments[0].angle = m_heading.angle();
}

void Boss::advance(float deltaTime, float speed)
{
    m_segments[0].position += m_heading * speed * deltaTime;
}

void Boss::holdShape()
{
    switch (m_def->body)
    {
        case BossBody::Ring:  holdRing(); break;
        case BossBody::Split: holdSplit(); break;
        case BossBody::Tower: holdTower(); break;
        case BossBody::Coil:  holdCoil(); break;
        default:              followChain(); break;
    }
}

void Boss::holdRing()
{
    // A closed circle of body, turning, with nothing in the middle of it.
    //
    // The whale fall's own brief says "IT IS A RING OF BONE. NOTHING LIVES IN
    // THE MIDDLE OF IT" and it was drawn as a chain trailing behind a head,
    // which is the shape of every other creature in the game. The skin draws
    // whatever the links do, so this is the whole change: put the links on a
    // circle instead of in a line, and a spine curls into a ring.
    //
    // segments[0] is still the head and still what everything else reads as
    // the creature's position - it is the centre of the ring now rather than
    // a point on it, and the rest of the body is laid round it.
    const std::size_t links = m_segments.size();
    if (links < 2)
        return;

    Segment& hub = m_segments[0];

    // It tightens as the fight goes on. A ring that closes is the one threat
    // a player cannot answer by backing off, which is what this creature is
    // for - and it is the second act made visible without a colour change.
    const float closing = 1.0f - config::BossRingClose * std::clamp(
                                     static_cast<float>(m_act) * 0.5f, 0.0f, 1.0f);

    const float radius = config::BossRingRadius * closing;

    // And it turns. Faster the further in, so the last act is a millstone.
    m_ringTurn += config::BossRingSpin * (1.0f + 0.7f * static_cast<float>(m_act))
                  * (6.283185307f / 360.0f) * config::MotionAmount;

    const float step = 6.283185307f / static_cast<float>(links - 1);

    for (std::size_t i = 1; i < links; ++i)
    {
        Segment& segment = m_segments[i];

        const float around = m_ringTurn + step * static_cast<float>(i - 1);

        // Breathing in and out along the radius, out of phase round the
        // circle, so the ring is never a perfect wheel - a perfect wheel is a
        // machine and this is an animal.
        const float breathe =
            1.0f + config::BossRingWobble * std::sin(m_time * 1.4f + around * 2.0f);

        segment.position = hub.position
                           + sf::Vector2f{ std::cos(around), std::sin(around) }
                                 * (radius * breathe);

        // Tangential, so the vertebrae lie along the ring rather than all
        // pointing outward like spokes.
        segment.angle = sf::radians(around + 1.5707963f);

        if (segment.sprite)
        {
            segment.sprite->setPosition(segment.position);
            segment.sprite->setRotation(segment.angle);
        }
    }

    // The hub is the animal's own heading, kept so the rest of the fight -
    // which asks what it is looking at - still gets an answer.
    if (hub.sprite)
    {
        hub.sprite->setPosition(hub.position);
        hub.sprite->setRotation(hub.angle);
    }
}

void Boss::holdSplit()
{
    // A body that comes apart. Two arcs of it hunting on their own clocks,
    // drifting away from each other and back together, so there is no one
    // place to stand that is away from all of it and no one silhouette to
    // learn. Every other creature in this game is one shape; this is the only
    // one that is two, and the point of it is that you can never have both
    // halves in front of you.
    //
    // It comes further apart with every act, which is the second phase made
    // visible: by the last one the thing is not an animal any more, it is two
    // things that used to be.
    const std::size_t links = m_segments.size();
    if (links < 3)
    {
        followChain();
        return;
    }

    Segment& hub = m_segments[0];

    const float spread = config::BossSplitGap
                         * (0.45f + 0.55f * std::clamp(static_cast<float>(m_act) * 0.5f,
                                                       0.0f, 1.0f));

    // The two halves swing about the hub on the same slow clock, a half turn
    // out of phase, so when one is in front of the player the other is not.
    const float swing = m_time * config::BossSplitSwing;

    const sf::Vector2f sides[2] = {
        hub.position + sf::Vector2f{ std::cos(swing), std::sin(swing) } * spread,
        hub.position + sf::Vector2f{ std::cos(swing + 3.14159265f),
                                     std::sin(swing + 3.14159265f) } * spread,
    };

    const std::size_t half = (links - 1 + 1) / 2;

    for (std::size_t i = 1; i < links; ++i)
    {
        Segment& segment = m_segments[i];

        const bool second = (i - 1) >= half;
        const std::size_t within = second ? (i - 1) - half : (i - 1);
        const std::size_t count = second ? (links - 1) - half : half;

        const sf::Vector2f anchor = sides[second ? 1 : 0];

        // Each half is a short arc rather than a line: a curl of arm, which
        // is what an octopus does with one when it is not using it.
        const float t = count > 1 ? static_cast<float>(within)
                                        / static_cast<float>(count - 1)
                                  : 0.0f;

        const float bend = (second ? -1.0f : 1.0f) * config::BossSplitCurl;
        const float around = swing + (second ? 3.14159265f : 0.0f) + bend * t;
        const float reach = config::BossSplitArm * (0.35f + 0.65f * t);

        segment.position = anchor
                           + sf::Vector2f{ std::cos(around), std::sin(around) } * reach;
        segment.angle = sf::radians(around);

        if (segment.sprite)
        {
            segment.sprite->setPosition(segment.position);
            segment.sprite->setRotation(segment.angle);
        }
    }

    if (hub.sprite)
    {
        hub.sprite->setPosition(hub.position);
        hub.sprite->setRotation(hub.angle);
    }
}

void Boss::holdTower()
{
    // A chimney. It is sitting on the hole in the floor and it does not leave
    // it: the links are laid in rings at fixed radii round a hub that never
    // travels, widest at the base, so from above it reads as a stack rather
    // than as an animal with limbs.
    //
    // What it does instead of moving is GROW. Each act pushes the rings out,
    // so the room the player has left shrinks without the creature taking a
    // single step - which is the only way a thing that cannot chase you is
    // still a fight in the last minute of it.
    const std::size_t links = m_segments.size();
    if (links < 2)
        return;

    Segment& hub = m_segments[0];

    const float grown = 1.0f + config::BossTowerGrowth
                                   * std::clamp(static_cast<float>(m_act) * 0.5f, 0.0f, 1.0f);

    m_ringTurn += config::BossTowerSpin * (6.283185307f / 360.0f) * config::MotionAmount;

    for (std::size_t i = 1; i < links; ++i)
    {
        Segment& segment = m_segments[i];

        // Two to a ring, so the stack has courses rather than one long spiral.
        const std::size_t course = (i - 1) / config::BossTowerPerCourse;
        const std::size_t within = (i - 1) % config::BossTowerPerCourse;

        const float radius = config::BossTowerBase * grown
                             * (1.0f - config::BossTowerTaper * static_cast<float>(course));

        // Every course turned against the one below it, and the whole stack
        // turning slowly: a vent does not spin, but the water coming off one
        // never stops moving and this is what that looks like from above.
        const float around = m_ringTurn * (course % 2 == 0 ? 1.0f : -0.7f)
                             + 6.283185307f * static_cast<float>(within)
                                   / static_cast<float>(config::BossTowerPerCourse)
                             + static_cast<float>(course) * 0.7f;

        const float boil = 1.0f + config::BossTowerBoil
                                      * std::sin(m_time * 2.1f + static_cast<float>(i));

        segment.position = hub.position
                           + sf::Vector2f{ std::cos(around), std::sin(around) }
                                 * (radius * boil);

        // Pointing out of the stack, because everything this creature does
        // comes out of it rather than round it.
        segment.angle = sf::radians(around);

        if (segment.sprite)
        {
            segment.sprite->setPosition(segment.position);
            segment.sprite->setRotation(segment.angle);
        }
    }

    if (hub.sprite)
    {
        hub.sprite->setPosition(hub.position);
        hub.sprite->setRotation(hub.angle);
    }
}

void Boss::holdCoil()
{
    // Coiled, not swimming. A bobbit worm spends its life in a hole with its
    // body wound under the floor and only the jaws out, and it was drawn as a
    // snake being towed round the basin nose first - which is the leviathan's
    // silhouette with a different skin on it.
    //
    // The links are a spiral tightening inward from the head, and the spiral
    // UNWINDS while it strikes: at rest it is a knot on the floor, and mid
    // lunge it is the longest thing in the game.
    const std::size_t links = m_segments.size();
    if (links < 2)
        return;

    Segment& head = m_segments[0];

    // getTell is 0 at rest and 1 at the top of a wind-up, so the body is
    // visibly loading before the strike lands rather than after it.
    const float open = std::clamp(getTell(), 0.0f, 1.0f);

    const float pitch = config::BossCoilPitch * (1.0f + config::BossCoilOpen * open);
    const float wind = config::BossCoilWind * (1.0f - 0.72f * open);

    const sf::Vector2f back = -unit(head.angle);
    const sf::Vector2f side{ -back.y, back.x };

    float along = 0.0f;
    float around = 0.0f;

    for (std::size_t i = 1; i < links; ++i)
    {
        Segment& segment = m_segments[i];

        along += pitch;
        around += wind;

        // Behind the head along its own axis, and swung further off that axis
        // with every link - which is a spiral drawn in the head's frame
        // rather than in the world's, so the whole coil turns with it.
        const float swing = std::sin(around) * config::BossCoilWidth
                            * (1.0f - 0.55f * open)
                            * (0.35f + 0.65f * static_cast<float>(i)
                                           / static_cast<float>(links - 1));

        segment.position = head.position + back * along + side * swing;

        const sf::Vector2f lead = (i == 1 ? head.position : m_segments[i - 1].position)
                                  - segment.position;

        if (lead.lengthSquared() > 0.0001f)
            segment.angle = lead.angle();

        if (segment.sprite)
        {
            segment.sprite->setPosition(segment.position);
            segment.sprite->setRotation(segment.angle);
        }
    }

    if (head.sprite)
    {
        head.sprite->setPosition(head.position);
        head.sprite->setRotation(head.angle);
    }
}

void Boss::followChain()
{
    // A wave running from the head down the length of it. Applied to the
    // sprite's rotation only - never to segment.position - because a segment
    // is what the shots are tested against and what the fins are bolted to.
    // Turning a link about its own centre moves neither.
    const auto flex = [this](std::size_t index) {
        const float lag = config::BossFlexLag * static_cast<float>(index);
        return sf::degrees(config::BossFlexDegrees * config::MotionAmount
                           * std::sin(m_time * config::BossFlexHz * 6.283185307f - lag));
    };

    Segment& head = m_segments[0];

    if (head.sprite)
    {
        head.sprite->setPosition(head.position);
        head.sprite->setRotation(head.angle + flex(0) * 0.4f);
    }

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

            // Held at exactly one spacing behind the link ahead, in whichever
            // direction it already lies. Pulling only - which is what this used
            // to do - meant a boss that stopped moving collapsed into a pile of
            // its own segments, and a pile is what a lot of them looked like:
            // no length, no tail, just a heap with a head on it. Keeping the
            // distance both ways holds the shape of the animal without making
            // the body rigid, because the direction is still its own.
            segment.position = previous.position - (delta / distance) * spacing;
        }

        if (segment.sprite)
        {
            segment.sprite->setPosition(segment.position);
            segment.sprite->setRotation(segment.angle + flex(i));
        }
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
        if (!segment.sprite)
            continue;

        float scale = segment.scale;
        if (m_def->body == BossBody::Bell)
            scale *= 1.0f - 0.16f * m_bellPulse; // the bell visibly clenches

        segment.sprite->setScale({ scale, scale });
        segment.sprite->setColor(tint(0.0f));
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
        }
        else
        {
            part.world = part.anchor;
        }

        if (!part.sprite)
            continue;

        part.sprite->setRotation(part.def.role == PartRole::Tentacle
                                     ? part.angle - sf::degrees(90.0f)
                                     : part.angle);
        part.sprite->setPosition(part.anchor);

        const float punch = 1.0f + 0.18f * (part.flash / config::BossFlashDuration);
        part.sprite->setScale({ part.def.scale * punch, part.def.scale * punch });
        part.sprite->setColor(tint(part.flash));
    }
}

bool Boss::organExposed(const Part& part) const
{
    // Nothing seals anything any more: the organs are anatomy and the bell
    // opens because a bell opens, not because that is where the damage gets
    // in. Kept as one question rather than deleted, because it is still what
    // decides whether the soft part of the animal is drawn.
    (void)part;
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
            advance(deltaTime, m_speed * m_vitals.mobility * rebornSpeed());

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
                                  + (m_enraged ? m_def->rage.extraShots : 0)
                                  + (m_fallen * config::BossRebornShots);
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
    advance(deltaTime, m_speed * m_vitals.mobility * rebornSpeed()
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
                     + (m_enraged ? m_def->rage.extraShots : 0)
                                  + (m_fallen * config::BossRebornShots);

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
            advance(deltaTime, m_speed * m_vitals.mobility * rebornSpeed());

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

                const int shots = m_def->attackShots + (m_enraged ? m_def->rage.extraShots : 0)
                                  + (m_fallen * config::BossRebornShots);
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
    advance(deltaTime, m_speed * m_vitals.mobility * rebornSpeed()
                           * (m_enraged ? m_def->rage.speedScale : 1.0f));

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
                                  + (m_fallen * config::BossRebornShots)
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

    for (int i = 0; i < m_fallen; ++i)
        pressure *= config::BossRebornPressure;

    return std::max(0.22f, pressure);
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
    const int turn = m_specialTurn++;
    const bool second = (turn % 2) != 0;

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
        {
            // Two moves while it is whole, three once it is hurt, four once it
            // is cornered. A creature that has been backed into the last third
            // of its health does not do the same two things faster, it starts
            // doing things it was not willing to do.
            const int owned = 2 + std::min(2, m_act);

            switch (turn % owned)
            {
                case 0:
                    // The lunge that follows is the grab. Announced as a
                    // charge, because being caught by something you did not
                    // see coming is not a fight, it is a punishment.
                    m_strikeTarget = head + toPlayer * 2.2f;
                    m_phase = Phase::Wind;
                    m_phaseTimer = config::BossWindTime;
                    beginTell(config::BossWindTime, toPlayer, true);
                    m_moves.push_back({ BossSpecial::Grab, head, toPlayer, 1.0f });
                    break;

                case 1:
                    // The tail. It does not need to be looking at you.
                    m_moves.push_back({ BossSpecial::Sweep, head, m_heading, 0.0f });
                    break;

                case 2:
                    // The whole animal comes round at once, from the tail
                    // forward. Behind it has been the safe place for the whole
                    // fight up to here, and this is the move that takes it.
                    m_moves.push_back({ BossSpecial::Thresh,
                                        m_segments.back().position, m_heading, 0.0f });
                    beginTell(config::BossTellTime, toPlayer, false);
                    break;

                default:
                    // It stops, opens all the way, and the water in front of
                    // it goes. Nothing to dodge sideways: the answer is to not
                    // be on the line at all.
                    m_moves.push_back({ BossSpecial::Sound, head, toPlayer, 0.0f });
                    beginTell(config::BossTellTime * 1.3f, toPlayer, false);
                    break;
            }
            break;
        }

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

    const int was = m_act;

    if (m_act < 1 && core < config::BossActTwo)
        m_act = 1;
    if (m_act < 2 && core < config::BossActThree)
        m_act = 2;

    // Reported once, for the frame it happens on. The only sign a creature
    // had changed its mind used to be that it went a different colour, which
    // on a screen this dark and this busy is no sign at all - and the bodies
    // that visibly change shape at an act, the ring tightening and the coil
    // throwing itself out, still need the moment marked.
    m_turned = m_act > was;
}

bool Boss::tookTurn()
{
    const bool turned = m_turned;
    m_turned = false;
    return turned;
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

float Boss::guardBias(sf::Vector2f player) const
{
    // With the plating off, the soft part is on one flank and the creature
    // knows it. It rolls that side away from the sub, which turns the second
    // half of the fight into a chase round the outside of an animal rather
    // than more of the first half - and the moment the last plate is gone it
    // has nothing left to protect and stops giving ground.
    for (const Part& part : m_parts)
    {
        if (!part.alive || part.def.role != PartRole::Organ || !organExposed(part))
            continue;

        const sf::Vector2f toward = player - part.world;
        if (toward.lengthSquared() < 1.0f)
            continue;

        const sf::Vector2f flank{ -unit(m_segments[0].angle).y, unit(m_segments[0].angle).x };
        const float facing = toward.normalized().dot(flank);

        // Only while the sub is actually on the open side, so it does not spin
        // for no reason the player can see.
        return -facing * config::BossGuardTurn;
    }

    return 0.0f;
}

void Boss::updateLife(float deltaTime, sf::Vector2f player)
{
    // None of this is the fight. It is what a thing that is alive does while
    // it fights, and the fight cannot read any of it back.
    m_flinch = std::max(0.0f, m_flinch - deltaTime * config::BossFlinchFade);

    // Coming apart, over a second or so rather than between two frames.
    // Each wind takes it further from what it was, so the third one is visibly
    // past the second rather than only redder than the first.
    const float wanted = std::min(1.0f, static_cast<float>(m_fallen) * 0.62f);
    if (m_shed < wanted)
        m_shed = std::min(wanted, m_shed + deltaTime / config::BossShedTime);

    // Breathing, and faster the worse it is doing: at full health it is a slow
    // swell and by the last act it is panting.
    m_breath += deltaTime * (config::BossBreathRate
                             + config::BossBreathRage * static_cast<float>(m_act));
    if (m_breath > 1.0f)
        m_breath -= std::floor(m_breath);

    // The fins beat in time with how hard it is actually swimming.
    m_beat += deltaTime * config::BossBeatRate
              * (0.4f + 0.6f * m_vitals.mobility)
              * (m_phase == Phase::Strike ? 2.4f : 1.0f);
    if (m_beat > 1.0f)
        m_beat -= std::floor(m_beat);

    // The jaw. Open on the wind-up and all the way through the strike, shut
    // the rest of the time, so the bite is announced by the mouth rather than
    // only by a ring on the floor.
    const float jaw = m_phase == Phase::Strike ? 1.0f
                         : m_tellTimer > 0.0f ? 0.30f + 0.70f * getTell()
                         : m_act >= 2 ? 0.22f  // by the last act it cannot shut it
                                      : 0.0f;

    m_jaw += std::clamp(jaw - m_jaw, -deltaTime * 3.0f, deltaTime * 7.0f);

    // And where it is looking. It watches the sub, but only as far round as a
    // head can turn - past that it has to turn the whole animal, which is the
    // whole reason getting behind one is worth anything.
    const sf::Vector2f nose = unit(m_segments[0].angle);
    const sf::Vector2f toward = player - m_segments[0].position;

    sf::Vector2f wantedLook = nose;
    if (toward.lengthSquared() > 1.0f)
    {
        const sf::Vector2f aim = toward.normalized();
        const float off = (aim.angle() - nose.angle()).wrapSigned().asDegrees();

        wantedLook = nose.rotatedBy(sf::degrees(std::clamp(off, -config::BossLookLimit,
                                                           config::BossLookLimit)));
    }

    m_look += (wantedLook - m_look) * std::min(1.0f, deltaTime * config::BossLookEase);

    if (m_look.lengthSquared() > 0.0001f)
        m_look = m_look.normalized();
}

void Boss::fall(Hit& result)
{
    // The bar emptying is not the end of the fight until it has emptied as
    // many times as the creature has winds in it.
    if (m_fallen < m_def->lives)
    {
        beginSecondPhase();
        result.destroyedPart = true; // the caller's word for "that was big"
        return;
    }

    m_dead = true;
    result.killedBoss = true;
}

void Boss::beginSecondPhase()
{
    ++m_fallen;

    // Half again of what it just lost. This is the last thirty seconds of a
    // fight and not a second fight: a creature that comes back with as much as
    // it started with is a creature the player has to kill twice, which is
    // twice as long and not once as good.
    //
    // The leviathan reaches for this pool for the first time here, because up
    // to now it has been living off its organs.
    m_coreMax = std::max(1, static_cast<int>(std::lround(
                                static_cast<float>(m_coreMax) * config::BossRebornHealth)));
    m_core = m_coreMax;

    // Everything it was holding back, all at once and from here on.
    m_act = 2;
    m_enraged = true;
    m_specialTimer = 0.0f;
    m_ultraTimer = 0.0f;
    m_attackTimer = 0.0f;

    // And it comes apart. The shed is what the drawing reads; the fight does
    // not look at it.
    m_shed = 0.0f;

    refreshVitals();
}

void Boss::updateBellRhythm(float deltaTime)
{
    // Clenched, then open, on a clock of its own. Hurt, it has to hang open
    // longer to recharge and it does it more often, so the window the player
    // is fighting for widens exactly as the fight goes on.
    m_rhythmTimer -= deltaTime;

    if (m_rhythmTimer > 0.0f)
    {
        const float target = m_rhythmOpen ? 1.0f : 0.0f;
        m_bellPulse += std::clamp(target - m_bellPulse, -deltaTime * 2.0f, deltaTime * 3.0f);
        return;
    }

    m_rhythmOpen = !m_rhythmOpen;

    m_rhythmTimer = m_rhythmOpen
                        ? config::BossBellOpen * (1.0f + 0.5f * static_cast<float>(m_act))
                        : config::BossBellShut * aggression();
}

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

    // What the body is doing, as opposed to what it has decided. Run after the
    // mind so the jaw and the eyes are reacting to the phase the mind has just
    // picked rather than to the one before it.
    updateLife(deltaTime, playerPosition);

    // A bell is sealed except while it is open, and until there were bells
    // that were not drifters, the only thing that ever opened one was the
    // drifter's own ring burst. A bell that never opens is a boss that cannot
    // be killed, so the window belongs to the body rather than to one mind.
    // The drifter still runs its own, because its window is the recoil of the
    // burst and that is the whole shape of that fight.
    if (m_def->body == BossBody::Bell && m_def->mind != BossMind::Drifter)
        updateBellRhythm(deltaTime);

    // After the mind, not before: a special can override the phase the mind
    // just chose, and it has to be the one that wins.
    considerSpecial(deltaTime, playerPosition);

    holdShape();
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

    holdShape();
    placeParts(0.0f);
}

Boss::Hit Boss::tryHit(sf::Vector2f position, float radius, int damage, float weakPointBonus)
{
    Hit result;

    if (!isVulnerable())
        return result; // nothing lands on something that is under the silt

    // One animal, one pool. Anywhere on it is somewhere worth shooting, which
    // is the whole reason the pieces went: an aiming puzzle is a good fight to
    // have once and it was being had six times.
    if (!bodyBlocks(position, radius))
        return result;

    result.hit = true;
    result.at = position;
    result.wasOrgan = true;

    m_core -= std::max(1, static_cast<int>(std::lround(
                              static_cast<float>(damage) * (1.0f + weakPointBonus))));
    m_flinch = 1.0f;

    // The whole creature flinches white rather than one piece of it, because
    // there is no longer a piece of it that is the one you hit.
    for (Part& part : m_parts)
        part.flash = config::BossFlashDuration;

    if (m_core <= 0)
        fall(result);

    return result;
}

float Boss::segmentReach(std::size_t index) const
{
    const float scale = m_segments[index].scale;

    // A chain is one animal: every link is as thick as the head and the hit
    // radius follows the head's.
    if (index == 0 || m_def->body == BossBody::Serpent || m_def->body == BossBody::Bell)
        return m_def->headRadius * scale;

    // The plans that put their links AWAY from the body cannot use that. A
    // bell's headRadius is ninety-six because it is one dome; give its
    // lobes the same and the creature has a contact footprint half a screen
    // across, most of it over open water. What the player can be touched by
    // has to be what the player can see, so these measure off the girth the
    // art is actually drawn at.
    return config::BossDrawnGirth * scale * config::BossLimbHitFactor;
}

bool Boss::bodyBlocks(sf::Vector2f position, float radius) const
{
    for (std::size_t i = 0; i < m_segments.size(); ++i)
    {
        const float reach = radius + segmentReach(i) * config::BossBlockRadiusFactor;
        if ((m_segments[i].position - position).lengthSquared() <= reach * reach)
            return true;
    }
    return false;
}

bool Boss::touches(sf::Vector2f position, float radius) const
{
    for (std::size_t i = 0; i < m_segments.size(); ++i)
    {
        const float reach = radius + segmentReach(i);
        if ((m_segments[i].position - position).lengthSquared() <= reach * reach)
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
    if (m_coreMax <= 0)
        return m_dead ? 0.0f : 1.0f;

    return std::clamp(static_cast<float>(m_core) / static_cast<float>(m_coreMax), 0.0f, 1.0f);
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

    if (m_def->skin != BossSkin::Sprites)
    {
        bossart::Body pose;

        pose.links.reserve(m_segments.size());
        for (const Segment& segment : m_segments)
        {
            // Girth rather than a sprite scale: what the drawing wants to know
            // is how wide the animal is here, and the taper down the chain is
            // already in the scale the chain was built with.
            pose.links.push_back({ segment.position, segment.angle,
                                   config::BossDrawnGirth * segment.scale });
        }

        pose.limbs.reserve(m_parts.size());
        for (const Part& part : m_parts)
        {
            pose.limbs.push_back({ part.def.role, part.anchor, part.angle, part.def.radius,
                                   part.flash / config::BossFlashDuration,
                                   part.def.lateral < 0.0f ? -1.0f : 1.0f,
                                   part.alive,
                                   part.def.role != PartRole::Organ || organExposed(part) });
        }

        pose.jaw = m_jaw;
        pose.breath = m_breath;
        pose.beat = std::sin(m_beat * 6.283185307f);
        pose.rage = std::clamp(static_cast<float>(m_act) * 0.5f, 0.0f, 1.0f);
        pose.tell = getTell();
        pose.flinch = m_flinch;
        pose.pulse = m_bellPulse;
        pose.shed = m_shed;
        pose.look = m_look;
        pose.alpha = std::clamp(m_visibility, 0.0f, 1.0f);

        bossart::draw(target, m_def->skin, pose, *m_hide);
        return;
    }

    // Arms hang behind the body, the body hides the tail, and the organs sit on
    // top so the thing the player is aiming at is never buried.
    for (const Part& part : m_parts)
    {
        if (part.alive && part.sprite && part.def.role == PartRole::Tentacle)
            target.draw(*part.sprite);
    }

    for (std::size_t i = m_segments.size(); i-- > 0;)
    {
        if (m_segments[i].sprite)
            target.draw(*m_segments[i].sprite);
    }

    for (const Part& part : m_parts)
    {
        if (part.alive && part.sprite
            && part.def.role != PartRole::Tentacle && part.def.role != PartRole::Organ)
        {
            target.draw(*part.sprite);
        }
    }

    for (const Part& part : m_parts)
    {
        if (part.alive && part.sprite && part.def.role == PartRole::Organ && organExposed(part))
            target.draw(*part.sprite);
    }
}
