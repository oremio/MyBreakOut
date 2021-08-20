#ifndef POWER_UP_H
#define POWER_UP_H
#include <string>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "game_object.h"

// The size of a PowerUp block
const glm::vec2 POWERUP_SIZE(60.0f, 20.0f);
// Velocity a PowerUp block has when spawned
const glm::vec2 VELOCITY(0.0f, 150.0f);

class PowerUp : public GameObject
{
public:
    // powerup state
    std::string Type;
    float       Duration;
    bool        Activated;
    // constructor
    PowerUp(std::string type, glm::vec3 color, float duration, glm::vec2 position, Texture2D texture)
        : GameObject(position, POWERUP_SIZE, texture, color, VELOCITY), Type(type), Duration(duration), Activated() { }
};

#endif
