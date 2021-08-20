#include <algorithm>

#include <irrklang/irrKlang.h>
using namespace irrklang;

#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include "game_object.h"
#include "ball_object.h"
#include "particle_generator.h"
#include "post_processor.h"

// Game-related State data
SpriteRenderer* Renderer;
GameObject* Player;
BallObject* Ball;
ParticleGenerator* Particles;
PostProcessor* Effects;
ISoundEngine* SoundEngine = createIrrKlangDevice();

float ShakeTime = 0.0f;

Game::Game(unsigned int width, unsigned int height)
    : State(GAME_ACTIVE), Keys(), Width(width), Height(height)
{

}

Game::~Game()
{
    delete Renderer;
    delete Player;
    delete Ball;
    delete Particles;
    delete Effects;
    SoundEngine->drop();
}

void Game::init()
{
    // load shaders
    ResourceManager::loadShader("src/shaders/sprite.vs", "src/shaders/sprite.fs", nullptr, "sprite");
    ResourceManager::loadShader("src/shaders/particle.vs", "src/shaders/particle.fs", nullptr, "particle");
    ResourceManager::loadShader("src/shaders/post_processing.vs", "src/shaders/post_processing.fs", nullptr, "postprocessing");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::getShader("sprite").use().setInteger("image", 0);
    ResourceManager::getShader("sprite").setMatrix4("projection", projection);
    ResourceManager::getShader("particle").use().setInteger("sprite", 0);
    ResourceManager::getShader("particle").setMatrix4("projection", projection);
    // load textures
    ResourceManager::loadTexture("resources/textures/background.jpg", false, "background");
    ResourceManager::loadTexture("resources/textures/kuku.png", true, "face");
    ResourceManager::loadTexture("resources/textures/block.png", false, "block");
    ResourceManager::loadTexture("resources/textures/block_solid.png", false, "block_solid");
    ResourceManager::loadTexture("resources/textures/paddle.png", true, "paddle");
    ResourceManager::loadTexture("resources/textures/particle.png", true, "particle");
    ResourceManager::loadTexture("resources/textures/powerup_speed.png", true, "powerup_speed");
    ResourceManager::loadTexture("resources/textures/powerup_sticky.png", true, "powerup_sticky");
    ResourceManager::loadTexture("resources/textures/powerup_increase.png", true, "powerup_increase");
    ResourceManager::loadTexture("resources/textures/powerup_confuse.png", true, "powerup_confuse");
    ResourceManager::loadTexture("resources/textures/powerup_chaos.png", true, "powerup_chaos");
    ResourceManager::loadTexture("resources/textures/powerup_passthrough.png", true, "powerup_passthrough");
    // set render-specific controls
    Renderer = new SpriteRenderer(ResourceManager::getShader("sprite"));
    Particles = new ParticleGenerator(ResourceManager::getShader("particle"), ResourceManager::getTexture("particle"), 500);
    Effects = new PostProcessor(ResourceManager::getShader("postprocessing"), this->Width, this->Height);
    // load levels
    GameLevel one; one.load("resources/levels/one.lvl", this->Width, this->Height / 2);
    GameLevel two; two.load("resources/levels/two.lvl", this->Width, this->Height / 2);
    GameLevel three; three.load("resources/levels/three.lvl", this->Width, this->Height / 2);
    GameLevel four; four.load("resources/levels/four.lvl", this->Width, this->Height / 2);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Levels.push_back(three);
    this->Levels.push_back(four);
    this->Level = 0;
    // configure game objects
    glm::vec2 playerPos = glm::vec2(this->Width / 2.0f - PLAYER_SIZE.x / 2.0f, this->Height - PLAYER_SIZE.y);
    Player = new GameObject(playerPos, PLAYER_SIZE, ResourceManager::getTexture("paddle"));
    glm::vec2 ballPos = playerPos + glm::vec2(PLAYER_SIZE.x / 2.0f - BALL_RADIUS, -BALL_RADIUS * 2.0f);
    Ball = new BallObject(ballPos, BALL_RADIUS, INITIAL_BALL_VELOCITY, ResourceManager::getTexture("face"));
    // audio
    SoundEngine->play2D("resources/audio/future_like_wind.mp3", true);
}

void Game::update(float dt)
{
    // update objects
    Ball->move(dt, this->Width);
    // check for collisions
    this->doCollisions();
    // update particles
    Particles->update(dt, *Ball, 2, glm::vec2(Ball->Radius / 2.0f));
    // update PowerUps
    this->updatePowerUps(dt);
    // reduce shake time
    if (ShakeTime > 0.0f)
    {
        ShakeTime -= dt;
        if (ShakeTime <= 0.0f)
            Effects->Shake = false;
    }
    // check loss condition
    if (Ball->Position.y >= this->Height) // did ball reach bottom edge?
    {
        this->resetLevel();
        this->resetPlayer();
    }
}

void Game::processInput(float dt)
{
    if (this->State == GAME_ACTIVE)
    {
        GLfloat velocity = PLAYER_VELOCITY * dt;
        // 移动挡板
        if (this->Keys[GLFW_KEY_A])
        {
            if (Player->Position.x >= 0.0f)
            {
                Player->Position.x -= velocity;
                if (Ball->Stuck)
                    Ball->Position.x -= velocity;
            }
        }
        if (this->Keys[GLFW_KEY_D])
        {
            if (Player->Position.x <= this->Width - Player->Size.x)
            {
                Player->Position.x += velocity;
                if (Ball->Stuck)
                    Ball->Position.x += velocity;
            }
        }
        if (this->Keys[GLFW_KEY_SPACE])
            Ball->Stuck = false;
    }
}

void Game::render()
{
    if (this->State == GAME_ACTIVE)
    {
        // begin rendering to postprocessing framebuffer
        Effects->beginRender();
            // draw background
            Renderer->drawSprite(ResourceManager::getTexture("background"), glm::vec2(0.0f, 0.0f), glm::vec2(this->Width, this->Height), 0.0f);
            // draw level
            this->Levels[this->Level].draw(*Renderer);
            // draw player
            Player->draw(*Renderer);
            // draw PowerUps
            for (PowerUp& powerUp : this->PowerUps)
                if (!powerUp.Destroyed)
                    powerUp.draw(*Renderer);
            // draw particles
            Particles->draw();
            // draw ball
            Ball->draw(*Renderer);
        // end rendering to postprocessing framebuffer
        Effects->endRender();
        // render postprocessing quad
        Effects->render(glfwGetTime());
    }
}

void Game::resetLevel()
{
    if (this->Level == 0)
        this->Levels[0].load("resources/levels/one.lvl", this->Width, this->Height / 2);
    else if (this->Level == 1)
        this->Levels[1].load("resources/levels/two.lvl", this->Width, this->Height / 2);
    else if (this->Level == 2)
        this->Levels[2].load("resources/levels/three.lvl", this->Width, this->Height / 2);
    else if (this->Level == 3)
        this->Levels[3].load("resources/levels/four.lvl", this->Width, this->Height / 2);
}

void Game::resetPlayer()
{
    // reset player/ball stats
    Player->Size = PLAYER_SIZE;
    Player->Position = glm::vec2(this->Width / 2.0f - PLAYER_SIZE.x / 2.0f, this->Height - PLAYER_SIZE.y);
    Ball->reset(Player->Position + glm::vec2(PLAYER_SIZE.x / 2.0f - BALL_RADIUS, -(BALL_RADIUS * 2.0f)), INITIAL_BALL_VELOCITY);
}

// powerups
bool isOtherPowerUpActive(std::vector<PowerUp>& powerUps, std::string type);

void Game::updatePowerUps(float dt)
{
    for (PowerUp& powerUp : this->PowerUps)
    {
        powerUp.Position += powerUp.Velocity * dt;
        if (powerUp.Activated)
        {
            powerUp.Duration -= dt;

            if (powerUp.Duration <= 0.0f)
            {
                // remove powerup from list (will later be removed)
                powerUp.Activated = false;
                // deactivate effects
                if (powerUp.Type == "sticky")
                {
                    if (!isOtherPowerUpActive(this->PowerUps, "sticky"))
                    {	// only reset if no other PowerUp of type sticky is active
                        Ball->Sticky = false;
                        Player->Color = glm::vec3(1.0f);
                    }
                }
                else if (powerUp.Type == "pass-through")
                {
                    if (!isOtherPowerUpActive(this->PowerUps, "pass-through"))
                    {	// only reset if no other PowerUp of type pass-through is active
                        Ball->PassThrough = false;
                        Ball->Color = glm::vec3(1.0f);
                    }
                }
                else if (powerUp.Type == "confuse")
                {
                    if (!isOtherPowerUpActive(this->PowerUps, "confuse"))
                    {	// only reset if no other PowerUp of type confuse is active
                        Effects->Confuse = false;
                    }
                }
                else if (powerUp.Type == "chaos")
                {
                    if (!isOtherPowerUpActive(this->PowerUps, "chaos"))
                    {	// only reset if no other PowerUp of type chaos is active
                        Effects->Chaos = false;
                    }
                }
            }
        }
    }
    // Remove all PowerUps from vector that are destroyed AND !activated (thus either off the map or finished)
    // Note we use a lambda expression to remove each PowerUp which is destroyed and not activated
    this->PowerUps.erase(std::remove_if(this->PowerUps.begin(), this->PowerUps.end(),
        [](const PowerUp& powerUp) { return powerUp.Destroyed && !powerUp.Activated; }
    ), this->PowerUps.end());
}

bool shouldSpawn(unsigned int chance)
{
    unsigned int random = rand() % chance;
    return random == 0;
}

void Game::spawnPowerUps(GameObject& block)
{
    if (shouldSpawn(75)) // 1 in 75 chance
        this->PowerUps.push_back(PowerUp("speed", glm::vec3(0.5f, 0.5f, 1.0f), 0.0f, block.Position, ResourceManager::getTexture("powerup_speed")));
    if (shouldSpawn(75))
        this->PowerUps.push_back(PowerUp("sticky", glm::vec3(1.0f, 0.5f, 1.0f), 20.0f, block.Position, ResourceManager::getTexture("powerup_sticky")));
    if (shouldSpawn(75))
        this->PowerUps.push_back(PowerUp("pass-through", glm::vec3(0.5f, 1.0f, 0.5f), 10.0f, block.Position, ResourceManager::getTexture("powerup_passthrough")));
    if (shouldSpawn(75))
        this->PowerUps.push_back(PowerUp("pad-size-increase", glm::vec3(1.0f, 0.6f, 0.4), 0.0f, block.Position, ResourceManager::getTexture("powerup_increase")));
    if (shouldSpawn(15)) // Negative powerups should spawn more often
        this->PowerUps.push_back(PowerUp("confuse", glm::vec3(1.0f, 0.3f, 0.3f), 7.5f, block.Position, ResourceManager::getTexture("powerup_confuse")));
    if (shouldSpawn(15))
        this->PowerUps.push_back(PowerUp("chaos", glm::vec3(0.9f, 0.25f, 0.25f), 7.5f, block.Position, ResourceManager::getTexture("powerup_chaos")));
}

void activatePowerUp(PowerUp& powerUp)
{
    if (powerUp.Type == "speed")
    {
        Ball->Velocity *= 1.2;
    }
    else if (powerUp.Type == "sticky")
    {
        Ball->Sticky = true;
        Player->Color = glm::vec3(1.0f, 0.5f, 1.0f);
    }
    else if (powerUp.Type == "pass-through")
    {
        Ball->PassThrough = true;
        Ball->Color = glm::vec3(1.0f, 0.5f, 0.5f);
    }
    else if (powerUp.Type == "pad-size-increase")
    {
        Player->Size.x += 50;
    }
    else if (powerUp.Type == "confuse")
    {
        if (!Effects->Chaos)
            Effects->Confuse = true; // only activate if chaos wasn't already active
    }
    else if (powerUp.Type == "chaos")
    {
        if (!Effects->Confuse)
            Effects->Chaos = true;
    }
}

bool isOtherPowerUpActive(std::vector<PowerUp>& powerUps, std::string type)
{
    // Check if another PowerUp of the same type is still active
    // in which case we don't disable its effect (yet)
    for (const PowerUp& powerUp : powerUps)
    {
        if (powerUp.Activated)
            if (powerUp.Type == type)
                return true;
    }
    return false;
}

// collision detection
bool checkCollision(GameObject& one, GameObject& two);
Collision checkCollision(BallObject& one, GameObject& two);
Direction vectorDirection(glm::vec2 closest);

void Game::doCollisions()
{
    for (GameObject& box : this->Levels[this->Level].Bricks)
    {
        if (!box.Destroyed)
        {
            Collision collision = checkCollision(*Ball, box);
            if (std::get<0>(collision)) // if collision is true
            {
                // destroy block if not solid
                if (!box.IsSolid)
                {
                    box.Destroyed = GL_TRUE;
                    this->spawnPowerUps(box);
                    SoundEngine->play2D("resources/audio/bleep.mp3", false);
                }
                else
                {   // if block is solid, enable shake effect
                    ShakeTime = 0.05f;
                    Effects->Shake = true;
                    SoundEngine->play2D("resources/audio/solid.wav", false);
                }
                // collision resolution
                Direction dir = std::get<1>(collision);
                glm::vec2 diff_vector = std::get<2>(collision);
                if (!(Ball->PassThrough && !box.IsSolid)) // 球是passthrough且撞到了non-solid ← 不是这种情况就做碰撞检测
                {
                    if (dir == LEFT || dir == RIGHT) // horizontal collision
                    {
                        Ball->Velocity.x = -Ball->Velocity.x; // reverse horizontal velocity
                        // relocate
                        float penetration = Ball->Radius - std::abs(diff_vector.x);
                        if (dir == LEFT)
                            Ball->Position.x += penetration; // move ball to right
                        else
                            Ball->Position.x -= penetration; // move ball to left;
                    }
                    else // vertical collision
                    {
                        Ball->Velocity.y = -Ball->Velocity.y; // reverse vertical velocity
                        // relocate
                        float penetration = Ball->Radius - std::abs(diff_vector.y);
                        if (dir == UP)
                            Ball->Position.y -= penetration; // move ball back up
                        else
                            Ball->Position.y += penetration; // move ball back down
                    }
                }
            }
        }
    }

    // also check collisions on PowerUps and if so, activate them
    for (PowerUp& powerUp : this->PowerUps)
    {
        if (!powerUp.Destroyed)
        {
            // first check if powerup passed bottom edge, if so: keep as inactive and destroy
            if (powerUp.Position.y >= this->Height)
                powerUp.Destroyed = true;

            if (checkCollision(*Player, powerUp))
            {	// collided with player, now activate powerup
                activatePowerUp(powerUp);
                powerUp.Destroyed = true;
                powerUp.Activated = true;
                SoundEngine->play2D("resources/audio/powerup.wav", false);
            }
        }
    }

    // check collisions for player pad (unless stuck)
    Collision result = checkCollision(*Ball, *Player);
    if (!Ball->Stuck && std::get<0>(result))
    {
        // check where it hit the board, and change velocity based on where it hit the board
        float centerBoard = Player->Position.x + Player->Size.x / 2.0f;
        float distance = (Ball->Position.x + Ball->Radius) - centerBoard;
        float percentage = distance / (Player->Size.x / 2.0f);
        // then move accordingly
        float strength = 2.0f;
        glm::vec2 oldVelocity = Ball->Velocity;
        Ball->Velocity.x = INITIAL_BALL_VELOCITY.x * percentage * strength;
        //Ball->Velocity.y = -Ball->Velocity.y;
        Ball->Velocity = glm::normalize(Ball->Velocity) * glm::length(oldVelocity); // keep speed consistent over both axes (multiply by length of old velocity, so total strength is not changed)
        // fix sticky paddle
        Ball->Velocity.y = -1.0f * abs(Ball->Velocity.y);
        // if Sticky powerup is activated, also stick ball to paddle once new velocity vectors were calculated
        Ball->Stuck = Ball->Sticky;
        SoundEngine->play2D("resources/audio/bleep.wav", false);
    }
}

bool checkCollision(GameObject& one, GameObject& two) // AABB - AABB collision
{
    // collision x-axis?
    bool collisionX = one.Position.x + one.Size.x >= two.Position.x &&
        two.Position.x + two.Size.x >= one.Position.x;
    // collision y-axis?
    bool collisionY = one.Position.y + one.Size.y >= two.Position.y &&
        two.Position.y + two.Size.y >= one.Position.y;
    // collision only if on both axes
    return collisionX && collisionY;
}

Collision checkCollision(BallObject& one, GameObject& two) // AABB - Circle collision
{
    // get center point circle first
    glm::vec2 center(one.Position + one.Radius);
    // calculate AABB info (center, half-extents)
    glm::vec2 aabb_half_extents(two.Size.x / 2.0f, two.Size.y / 2.0f);
    glm::vec2 aabb_center(two.Position.x + aabb_half_extents.x, two.Position.y + aabb_half_extents.y);
    // get difference vector between both centers
    glm::vec2 difference = center - aabb_center;
    glm::vec2 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
    // now that we know the the clamped values, add this to AABB_center and we get the value of box closest to circle
    glm::vec2 closest = aabb_center + clamped;
    // now retrieve vector between center circle and closest point AABB and check if length < radius
    // difference是撞击方向
    difference = closest - center;

    if (glm::length(difference) < one.Radius) // not <= since in that case a collision also occurs when object one exactly touches object two, which they are at the end of each collision resolution stage.
        return std::make_tuple(true, vectorDirection(difference), difference);
    else
        return std::make_tuple(false, UP, glm::vec2(0.0f, 0.0f));
}

// calculates which direction a vector is facing (N,E,S or W)
Direction vectorDirection(glm::vec2 target)
{
    glm::vec2 compass[] = {
        glm::vec2(0.0f, 1.0f),	// up
        glm::vec2(1.0f, 0.0f),	// right
        glm::vec2(0.0f, -1.0f),	// down
        glm::vec2(-1.0f, 0.0f)	// left
    };
    float max = 0.0f;
    unsigned int best_match = -1;
    for (unsigned int i = 0; i < 4; i++)
    {
        float dot_product = glm::dot(glm::normalize(target), compass[i]);
        if (dot_product > max)
        {
            max = dot_product;
            best_match = i;
        }
    }
    return (Direction)best_match;
}