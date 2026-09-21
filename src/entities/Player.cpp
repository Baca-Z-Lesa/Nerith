#include "entities/Player.h"

#include "core/Assets.h"
#include "core/Config.h"
#include "world/Arena.h"

#include <SFML/Window/Keyboard.hpp>

#include <algorithm>
#include <cmath>

Player::Player(const Assets& assets, const Stats& stats)
    : m_stats(&stats)
    , m_sprite(assets.player)
    , m_visuals(assets)
    , m_position(config::WindowWidth / 2.0f, config::WindowHeight / 2.0f)
    , m_aimDirection(1.0f, 0.0f)
    , m_dashDirection(1.0f, 0.0f)
    , m_fireTimer(0.0f)
    , m_torpedoTimer(0.0f)
    , m_dashTimer(0.0f)
    , m_dashCooldown(0.0f)
    , m_shieldTimer(0.0f)
    , m_health(config::PlayerMaxHealth)
    , m_lastMaxHealth(config::PlayerMaxHealth)
    , m_shieldCharges(0)
    , m_invulnerabilityTimer(0.0f)
    , m_moving(false)
    , m_ventReady(true)
{
    // Rotating around the middle of the sprite keeps the sub centred on its
    // position no matter which way it faces.
    m_sprite.setOrigin(sf::Vector2f(assets.player.getSize()) / 2.0f);
    m_sprite.setScale({ config::PlayerScale, config::PlayerScale });

}

int Player::getMaxHealth() const
{
    return std::max(1, m_stats->getInt(Stat::MaxHealth, config::PlayerMaxHealth));
}

int Player::getMaxShieldCharges() const
{
    return m_stats->has(Trait::Shield) ? std::max(0, m_stats->getInt(Stat::ShieldCharges, 0)) : 0;
}

float Player::getDashCooldownFraction() const
{
    const float full =
        std::max(config::DashMinCooldown, m_stats->get(Stat::DashCooldown, config::DashBaseCooldown));
    return std::clamp(1.0f - m_dashCooldown / full, 0.0f, 1.0f);
}

void Player::onStatsChanged()
{
    const int maxHealth = getMaxHealth();

    // A newly bought tank arrives full rather than empty, otherwise buying HP
    // mid run would do nothing until you found a way to heal.
    if (maxHealth > m_lastMaxHealth)
        m_health += maxHealth - m_lastMaxHealth;

    m_health = std::min(m_health, maxHealth);
    m_lastMaxHealth = maxHealth;

    m_shieldCharges = std::min(m_shieldCharges, getMaxShieldCharges());
}

void Player::onLevelStart()
{
    // The vent is a once-per-level save, so a long run cannot lean on it.
    m_ventReady = true;
    m_shieldCharges = getMaxShieldCharges();
}

void Player::present(float alpha)
{
    // Between where it was and where it is. The sprite is the only thing this
    // touches - m_position stays exactly where the simulation left it, because
    // everything that asks a question about the world has to get the same
    // answer no matter how often the picture is being made.
    sf::Vector2f at = m_prevPosition + (m_position - m_prevPosition) * alpha;

    // A hull with water around it and weight in it. It leans into a turn, it
    // stretches when the pumps open up, and it is never quite still. All of it
    // is the picture: the muzzle, the stern and the hitbox are all still
    // exactly where the step put them, so none of this can miss a shot.
    constexpr float Tau = 6.283185307f;

    const float speed = std::min(1.0f, m_velocity.length() / config::PlayerSpeed);

    // Sitting still, it breathes; under way, the breath gives way to the surge
    // of actually moving, because a sub at full ahead is not idling.
    const float breath = config::SubIdleSwell * (1.0f - speed)
                         * std::sin(m_age * config::SubIdleHz * Tau);

    const float surge = config::SubSurge * speed;
    const float narrow = std::abs(m_bank) * config::SubBankSquash;

    const float along = 1.0f + (surge + breath) * config::MotionAmount;
    const float across = 1.0f - (narrow + surge * 0.5f - breath) * config::MotionAmount;

    // And the sway of something being pushed through water rather than slid
    // across it - across the hull, and only while it is not already busy.
    const sf::Vector2f side{ -m_aimDirection.y, m_aimDirection.x };

    at += side * (config::SubWake * (1.0f - speed * 0.7f) * config::MotionAmount
                  * std::sin(m_age * config::SubWakeHz * Tau));

    m_sprite.setScale({ config::PlayerScale * along, config::PlayerScale * across });
    m_sprite.setPosition(at);
    m_visuals.present(at, m_aimDirection.angle());
}

void Player::aimAt(sf::Vector2f target)
{
    const sf::Vector2f delta = target - m_position;

    // Cursor sitting exactly on the sub: there is no direction to derive, so
    // hold the previous heading instead of snapping somewhere arbitrary.
    if (delta.lengthSquared() < 0.0001f)
        return;

    m_aimDirection = delta.normalized();

    // The texture is drawn facing right, which matches an angle of 0.
    m_sprite.setRotation(delta.angle());
}

void Player::updateInvulnerability(float deltaTime)
{
    if (m_invulnerabilityTimer <= 0.0f)
        return;

    m_invulnerabilityTimer = std::max(m_invulnerabilityTimer - deltaTime, 0.0f);

    if (m_invulnerabilityTimer <= 0.0f)
    {
        m_sprite.setColor(sf::Color::White);
        return;
    }

    // Blink while the hit is still being absorbed, so the free window is visible.
    const bool solid = std::fmod(m_invulnerabilityTimer, config::PlayerBlinkInterval)
                       > config::PlayerBlinkInterval * 0.5f;
    m_sprite.setColor(solid ? sf::Color::White : sf::Color(255, 255, 255, 70));
}

void Player::updateShield(float deltaTime)
{
    const int maximum = getMaxShieldCharges();
    if (maximum <= 0)
    {
        m_shieldCharges = 0;
        return;
    }

    if (m_shieldCharges >= maximum)
    {
        m_shieldTimer = 0.0f;
        return;
    }

    m_shieldTimer += deltaTime;
    const float period = std::max(0.5f, m_stats->get(Stat::ShieldRecharge, config::ShieldBaseRecharge));

    if (m_shieldTimer >= period)
    {
        m_shieldTimer = 0.0f;
        ++m_shieldCharges;
    }
}

void Player::update(float deltaTime, sf::Vector2f steering, sf::Vector2f aimTarget,
                    const Arena& arena)
{
    const sf::Vector2f before = m_position;

    if (m_fireTimer > 0.0f)
        m_fireTimer -= deltaTime;
    if (m_torpedoTimer > 0.0f)
        m_torpedoTimer -= deltaTime;
    if (m_dashCooldown > 0.0f)
        m_dashCooldown -= deltaTime;

    updateInvulnerability(deltaTime);
    updateShield(deltaTime);

    m_moving = steering.lengthSquared() > 0.0f;

    if (m_dashTimer > 0.0f)
    {
        // A dash overrides steering entirely: you committed to a direction.
        m_dashTimer -= deltaTime;
        m_position += m_dashDirection * config::DashSpeed * deltaTime;

        // And it leaves you carrying that direction, so the sub swims out of a
        // dash instead of stopping dead at the end of one.
        m_velocity = m_dashDirection * config::PlayerSpeed;
    }
    else
    {
        const float speed =
            m_stats->get(Stat::MoveSpeed, config::PlayerSpeed) * urgency() * m_burden;

        // The sub has mass now. Not much of one - a whole hull is up to speed
        // inside a fortieth of a second, which nobody can feel - but enough
        // that the thing being steered reads as a submarine rather than as a
        // cursor.
        const sf::Vector2f wanted = steering * speed;

        const float rate = m_moving ? config::PlayerAccel : config::PlayerDrag;

        m_velocity += (wanted - m_velocity) * std::min(1.0f, rate * deltaTime);

        m_position += m_velocity * deltaTime;
    }

    // Shoved by something that hit you. Decays rather than running out, so a
    // hit pushes and lets go instead of carrying the sub across the room.
    if (m_impulseTimer > 0.0f)
    {
        m_impulseTimer -= deltaTime;
        m_position += m_impulse * deltaTime;
        m_impulse *= std::exp(-config::KnockbackDecay * deltaTime);
    }

    // Water that turns. A dash is strong enough to leave a pit, ordinary
    // thrust is not quite, so a pit is something to be crossed on purpose
    // rather than a place to hold ground in.
    m_position += arena.currentAt(m_position) * deltaTime;

    m_position = arena.clampInside(m_position, config::PlayerRadius);

    // Measured rather than derived, so it already includes the dash, the
    // current and anything that shoved the sub this step.
    m_velocity = deltaTime > 0.0f ? (m_position - before) / deltaTime
                                  : sf::Vector2f{ 0.0f, 0.0f };

    aimAt(aimTarget);
    m_sprite.setPosition(m_position);

    // How hard the nose is being swung about, which is what the hull leans on.
    // Measured here rather than in aimAt because that one has no clock, and a
    // lean wants degrees per second, not degrees.
    m_age += deltaTime;

    const sf::Angle aim = m_aimDirection.angle();
    const float rate = deltaTime > 0.0f
                           ? (aim - m_lastAim).wrapSigned().asDegrees() / deltaTime
                           : 0.0f;
    m_lastAim = aim;

    const float wanted = std::clamp(rate * config::SubBankDegrees,
                                    -config::SubBankMax, config::SubBankMax);

    m_bank += (wanted - m_bank) * std::min(1.0f, config::SubBankEase * deltaTime);

    m_visuals.setShieldUp(m_shieldCharges > 0);
    m_visuals.update(m_position, m_aimDirection.angle(), *m_stats, deltaTime);
}

float Player::urgency() const
{
    // Second wind: hurt badly enough, the sub moves and fires like it has
    // nothing left to lose. A fraction rather than a flag so it reads on the
    // very hit that takes you under.
    if (!m_stats->has(Trait::SecondWind))
        return 1.0f;

    const int maximum = std::max(1, getMaxHealth());
    const float fraction = static_cast<float>(m_health) / static_cast<float>(maximum);

    return fraction <= config::SecondWindThreshold ? config::SecondWindSpeed : 1.0f;
}

bool Player::isDesperate() const
{
    return urgency() > 1.0f;
}

void Player::heal(int amount)
{
    if (amount <= 0 || m_health <= 0)
        return;

    m_health = std::min(m_health + amount, getMaxHealth());
}

bool Player::tryFire()
{
    if (m_fireTimer > 0.0f)
        return false;

    // Fire rate is a multiplier on shots per second, so the cooldown is its
    // reciprocal. Guarded so a pathological stat cannot divide by zero.
    const float rate = std::max(0.05f, m_stats->get(Stat::FireRate, 1.0f));
    float cooldown = config::PlayerFireCooldown / rate;

    if (isDesperate())
        cooldown *= config::SecondWindFireRate;

    // Added rather than assigned, so the fraction of a step the timer had
    // already run past zero is not thrown away. Assigning it rounds every
    // cooldown up to a whole step, which at the top of the weapon line is
    // most of a tenth of the fire rate the player paid for.
    m_fireTimer += cooldown;

    return true;
}

bool Player::tryFireTorpedo()
{
    if (!m_stats->has(Trait::Torpedoes) || m_torpedoTimer > 0.0f)
        return false;

    const float rate = std::max(0.05f, m_stats->get(Stat::TorpedoRate, 1.0f));
    m_torpedoTimer = config::TorpedoCooldown / rate;
    return true;
}

void Player::applyImpulse(sf::Vector2f push, float seconds)
{
    if (seconds <= 0.0f)
        return;

    // The larger of the two rather than the sum, the same way the shake is
    // taken: four things landing at once should not throw the sub further
    // than the worst of them did.
    if (push.lengthSquared() >= m_impulse.lengthSquared())
        m_impulse = push;

    m_impulseTimer = std::max(m_impulseTimer, seconds);
}

bool Player::tryDash(sf::Vector2f steering)
{
    if (!m_stats->has(Trait::Dash) || m_dashCooldown > 0.0f || m_dashTimer > 0.0f)
        return false;

    // Dash along whatever you are holding; if nothing, dash where you are aiming.
    m_dashDirection = steering.lengthSquared() > 0.0f ? steering : m_aimDirection;

    m_dashTimer = config::DashDuration;
    m_dashCooldown =
        std::max(config::DashMinCooldown, m_stats->get(Stat::DashCooldown, config::DashBaseCooldown));
    return true;
}

Player::DamageResult Player::takeDamage(int amount)
{
    DamageResult result;

    if (m_invulnerabilityTimer > 0.0f || m_health <= 0)
        return result;

    // Cavitation makes the dash itself the dodge.
    if (m_dashTimer > 0.0f && m_stats->has(Trait::DashInvuln))
    {
        result.dodged = true;
        return result;
    }

    // A shield charge eats the whole hit, however big it was.
    if (m_shieldCharges > 0)
    {
        --m_shieldCharges;
        m_shieldTimer = 0.0f;
        m_invulnerabilityTimer =
            m_stats->get(Stat::InvulnerabilityTime, config::PlayerInvulnerabilityDuration);
        result.shieldBroke = true;
        return result;
    }

    // Armour never reduces a hit to nothing: plating is meant to make the sub
    // tough, not immortal.
    //
    // It used to stop at one point of damage, which sounds like the same rule
    // and is not. Almost everything in the water bites for one to three, so a
    // sub with six plates took exactly one point from every creature in the
    // game and one from a boss as well - and with the mercy window stacked on
    // top of that, nothing down there could kill it at any speed. The floor is
    // a fraction of the hit instead, so plating always matters and a heavy hit
    // is always heavy.
    const int armour = m_stats->getInt(Stat::Armour, 0);
    const int floor = std::max(1, static_cast<int>(std::ceil(
                                      static_cast<float>(amount) * config::ArmourFloor)));
    const int taken = std::clamp(amount - armour, floor, amount);

    // What the plating actually ate, so the caller can make something of it.
    // Armour that works silently is armour the player never notices buying.
    result.soaked = std::max(0, amount - taken);

    if (taken >= m_health && m_ventReady && m_stats->has(Trait::EmergencyVent))
    {
        // Blow the ballast: one hit per level cannot finish you.
        m_ventReady = false;
        m_health = 1;
        m_invulnerabilityTimer =
            m_stats->get(Stat::InvulnerabilityTime, config::PlayerInvulnerabilityDuration) * 2.0f;
        result.applied = true;
        result.ventSaved = true;
        return result;
    }

    m_health -= taken;
    m_invulnerabilityTimer =
        m_stats->get(Stat::InvulnerabilityTime, config::PlayerInvulnerabilityDuration);
    result.applied = true;
    return result;
}

void Player::setPosition(sf::Vector2f position)
{
    // Put somewhere rather than swum there, so there is nothing to draw
    // between: a harpoon, a boss move or the start of a dive would otherwise
    // send the sub sailing across the basin over half a frame.
    m_position = position;
    m_prevPosition = position;
    m_sprite.setPosition(m_position);
}

void Player::reset()
{
    m_position = { config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };
    m_lastMaxHealth = getMaxHealth();
    m_health = m_lastMaxHealth;
    m_invulnerabilityTimer = 0.0f;
    m_fireTimer = 0.0f;
    m_torpedoTimer = 0.0f;
    m_dashTimer = 0.0f;
    m_dashCooldown = 0.0f;
    m_burden = 1.0f;
    m_shieldTimer = 0.0f;
    m_shieldCharges = getMaxShieldCharges();
    m_moving = false;
    m_velocity = { 0.0f, 0.0f };
    m_impulse = { 0.0f, 0.0f };
    m_impulseTimer = 0.0f;
    m_ventReady = true;
    m_bank = 0.0f;

    // Nothing to draw between: a fresh sub has not swum anywhere yet.
    m_prevPosition = m_position;

    m_sprite.setColor(sf::Color::White);
    m_sprite.setPosition(m_position);
}

sf::Vector2f Player::getMuzzlePosition() const
{
    return m_position + m_aimDirection * config::PlayerMuzzleOffset;
}

sf::Vector2f Player::getSternPosition() const
{
    return m_position - m_aimDirection * config::PlayerSternOffset;
}

void Player::draw(sf::RenderTarget& target) const
{
    m_visuals.drawUnder(target);
    target.draw(m_sprite);
    m_visuals.drawOver(target);
}
