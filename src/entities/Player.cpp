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
    m_sprite.setPosition(m_position);
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
    const float full = std::max(0.1f, m_stats->get(Stat::DashCooldown, config::DashBaseCooldown));
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

sf::Vector2f Player::readMovementInput()
{
    // Scancodes describe the physical key position, so WASD stays in the same
    // place regardless of the user's keyboard layout (QWERTY, QWERTZ, AZERTY).
    sf::Vector2f direction{ 0.0f, 0.0f };

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::W))
        direction.y -= 1.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::S))
        direction.y += 1.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::A))
        direction.x -= 1.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::D))
        direction.x += 1.0f;

    // Normalise so diagonals are not ~41% faster than straight lines.
    if (direction.lengthSquared() > 0.0f)
        direction = direction.normalized();

    return direction;
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

void Player::update(float deltaTime, sf::Vector2f aimTarget, const Arena& arena)
{
    if (m_fireTimer > 0.0f)
        m_fireTimer -= deltaTime;
    if (m_torpedoTimer > 0.0f)
        m_torpedoTimer -= deltaTime;
    if (m_dashCooldown > 0.0f)
        m_dashCooldown -= deltaTime;

    updateInvulnerability(deltaTime);
    updateShield(deltaTime);

    const sf::Vector2f movement = readMovementInput();
    m_moving = movement.lengthSquared() > 0.0f;

    if (m_dashTimer > 0.0f)
    {
        // A dash overrides steering entirely: you committed to a direction.
        m_dashTimer -= deltaTime;
        m_position += m_dashDirection * config::DashSpeed * deltaTime;
    }
    else
    {
        // Multiplying by deltaTime is what makes the speed framerate-independent:
        // the player always covers this many pixels per second of real time.
        const float engine = isBroken(Hull::Engine) ? config::BrokenEngineSpeed : 1.0f;
        const float speed = m_stats->get(Stat::MoveSpeed, config::PlayerSpeed) * urgency()
                            * m_burden * engine;
        m_position += movement * speed * deltaTime;
    }

    // Water that turns. A dash is strong enough to leave a pit, ordinary
    // thrust is not quite, so a pit is something to be crossed on purpose
    // rather than a place to hold ground in.
    m_position += arena.currentAt(m_position) * deltaTime;

    m_position = arena.clampInside(m_position, config::PlayerRadius);

    aimAt(aimTarget);
    m_sprite.setPosition(m_position);

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
    m_fireTimer = config::PlayerFireCooldown / rate;

    if (isDesperate())
        m_fireTimer *= config::SecondWindFireRate;

    // A cracked gun cycles slowly. Together with the damage it loses, this is
    // what makes the third threshold the one you actually feel.
    if (isBroken(Hull::Gun))
        m_fireTimer *= config::BrokenGunRate;

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

bool Player::isBroken(Hull part) const
{
    return (m_broken & (1u << static_cast<unsigned int>(part))) != 0u;
}

bool Player::breakNextPart()
{
    // A fixed order, least crippling first. Random would be less repetitive
    // and far worse: the player has to be able to predict what the next hit
    // costs, or the escalation is just noise.
    for (unsigned int i = 0; i < static_cast<unsigned int>(Hull::Count); ++i)
    {
        const unsigned int bit = 1u << i;
        if ((m_broken & bit) == 0u)
        {
            m_broken |= bit;
            return true;
        }
    }

    return false; // nothing left to lose
}

void Player::repairHull()
{
    m_broken = 0;
}

bool Player::tryDash()
{
    // A dead engine is a dead engine. The dash is the one thing the player
    // reaches for when things go wrong, so losing it is the last and worst of
    // the four rather than the first.
    if (isBroken(Hull::Engine))
        return false;

    if (!m_stats->has(Trait::Dash) || m_dashCooldown > 0.0f || m_dashTimer > 0.0f)
        return false;

    // Dash along whatever you are holding; if nothing, dash where you are aiming.
    const sf::Vector2f movement = readMovementInput();
    m_dashDirection = movement.lengthSquared() > 0.0f ? movement : m_aimDirection;

    m_dashTimer = config::DashDuration;
    m_dashCooldown = std::max(0.2f, m_stats->get(Stat::DashCooldown, config::DashBaseCooldown));
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
    const int armour = m_stats->getInt(Stat::Armour, 0);
    const int taken = std::max(1, amount - armour);

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
    m_position = position;
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
    m_broken = 0;
    m_shieldTimer = 0.0f;
    m_shieldCharges = getMaxShieldCharges();
    m_moving = false;
    m_ventReady = true;

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
