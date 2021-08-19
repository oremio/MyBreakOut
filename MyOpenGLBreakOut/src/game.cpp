#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include "game_object.h"

// Game-related State data
SpriteRenderer* Renderer;
GameObject* Player;

Game::Game(unsigned int width, unsigned int height)
    : State(GAME_ACTIVE), Keys(), Width(width), Height(height)
{

}

Game::~Game()
{
    delete Renderer;
    delete Player;
}

void Game::init()
{
    // load shaders
    ResourceManager::loadShader("src/shaders/sprite.vs", "src/shaders/sprite.fs", nullptr, "sprite");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
        static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::getShader("sprite").use().setInteger("image", 0);
    ResourceManager::getShader("sprite").setMatrix4("projection", projection);
    // set render-specific controls
    Renderer = new SpriteRenderer(ResourceManager::getShader("sprite"));
    // load textures
    ResourceManager::loadTexture("resources/textures/background.jpg", false, "background");
    ResourceManager::loadTexture("resources/textures/kuku.png", true, "face");
    ResourceManager::loadTexture("resources/textures/block.png", false, "block");
    ResourceManager::loadTexture("resources/textures/block_solid.png", false, "block_solid");
    ResourceManager::loadTexture("resources/textures/paddle.png", true, "paddle");
    // load levels
    GameLevel one; one.load("levels/one.lvl", this->Width, this->Height / 2);
    GameLevel two; two.load("levels/two.lvl", this->Width, this->Height / 2);
    GameLevel three; three.load("levels/three.lvl", this->Width, this->Height / 2);
    GameLevel four; four.load("levels/four.lvl", this->Width, this->Height / 2);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Levels.push_back(three);
    this->Levels.push_back(four);
    this->Level = 0;
    // configure game objects
    glm::vec2 playerPos = glm::vec2(this->Width / 2.0f - PLAYER_SIZE.x / 2.0f, this->Height - PLAYER_SIZE.y);
    Player = new GameObject(playerPos, PLAYER_SIZE, ResourceManager::getTexture("paddle"));
}

void Game::update(float dt)
{

}

void Game::processInput(float dt)
{
    if (this->State == GAME_ACTIVE)
    {
        GLfloat velocity = PLAYER_VELOCITY * dt;
        // ÒÆ¶¯µ²°å
        if (this->Keys[GLFW_KEY_A])
        {
            if (Player->Position.x >= 0)
                Player->Position.x -= velocity;
        }
        if (this->Keys[GLFW_KEY_D])
        {
            if (Player->Position.x <= this->Width - Player->Size.x)
                Player->Position.x += velocity;
        }
    }
}

void Game::render()
{
    if (this->State == GAME_ACTIVE)
    {
        // draw background
        Renderer->drawSprite(ResourceManager::getTexture("background"), glm::vec2(0.0f, 0.0f), glm::vec2(this->Width, this->Height), 0.0f);
        // draw level
        this->Levels[this->Level].draw(*Renderer);
        // draw player
        Player->draw(*Renderer);
    }
}