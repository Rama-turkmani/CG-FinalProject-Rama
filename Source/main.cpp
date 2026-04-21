// ============================================================================
//  Final Project - Computer Graphics (Modern OpenGL 3.3 Core)
//  3D Maze Collector - Student : Rama
//  Libraries : GLFW, GLEW, GLM, stb_image
//
//  Gameplay:
//    - First-person player walking through a 3D maze.
//    - Collect all 5 coins (awesomeface texture) before the 90s timer ends.
//    - Walls use container texture. Floor is tiled.
//    - Visual effects: directional diffuse lighting + exponential distance fog.
//    - Collision: axis-aligned (AABB) sweep-and-separate against wall cells.
//    - HUD : 5 coin-status squares + timer bar + Win/Lose full-screen overlay.
//
//  Controls (Pair 5 of the rubric):
//    W / S / A / D : move forward / back / strafe left / right
//    Mouse         : look around (yaw / pitch)
//    R             : restart
//    ESC           : quit
// ============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cmath>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ----------------------------------------------------------------------------
//  Window / game constants
// ----------------------------------------------------------------------------
static unsigned int SCR_W = 1024;
static unsigned int SCR_H = 720;
static const float  CELL  = 2.0f;
static const float  PLAYER_R = 0.35f;
static const float  PLAYER_EYE_Y = 0.9f;
static const float  PLAYER_SPEED = 3.2f;
static const float  MOUSE_SENS = 0.11f;
static const float  GAME_TIME = 90.0f; // seconds

// 10 x 10 maze. 'W' = wall, '.' = floor, 'C' = floor + coin, 'S' = spawn.
// Edge rows/cols must be walls.
static const char* MAZE[10] = {
    "WWWWWWWWWW",
    "WS.....C.W",
    "W.WWW.WW.W",
    "W.W.C..W.W",
    "W.W.WW.W.W",
    "W.W..W.W.W",
    "W.WWWW.W.W",
    "W.C..W.C.W",
    "W.WW.W.W.W",
    "WWWWWWWCWW",
};
static const int MAZE_W = 10;
static const int MAZE_H = 10;

// ----------------------------------------------------------------------------
//  Camera / player state
// ----------------------------------------------------------------------------
struct Player {
    glm::vec3 pos;
    float yaw;   // degrees, 0 = +X, 90 = +Z
    float pitch; // degrees
};

static Player     g_player;
static bool       g_firstMouse = true;
static double     g_lastMouseX = 0.0, g_lastMouseY = 0.0;
static float      g_deltaTime = 0.0f;
static float      g_lastFrame = 0.0f;

// ----------------------------------------------------------------------------
//  Game state
// ----------------------------------------------------------------------------
enum class GameState { Playing, Won, Lost };
struct Coin {
    glm::vec3 pos;
    bool      alive;
    float     spin; // rotation angle (radians)
};

static GameState       g_state = GameState::Playing;
static std::vector<Coin> g_coins;
static std::vector<glm::vec3> g_walls; // center of each wall cube (y=1)
static float            g_timeLeft = GAME_TIME;
static int              g_coinsLeft = 0;
static bool             g_isMoving = false; // for walk-bob animation
static float            g_walkPhase = 0.0f;

// ----------------------------------------------------------------------------
//  File / shader helpers
// ----------------------------------------------------------------------------
static std::string findAsset(const std::string& rel)
{
    // Try the CWD and a few parent directories so the .exe works whether
    // launched from the solution root, the project folder, or x64/Debug.
    const char* prefixes[] = { "", "../", "../../", "../../../" };
    for (const char* p : prefixes) {
        std::string full = std::string(p) + rel;
        std::ifstream f(full.c_str(), std::ios::binary);
        if (f.good()) return full;
    }
    std::cerr << "[WARN] Could not locate asset: " << rel << "\n";
    return rel;
}

static std::string readTextFile(const std::string& path)
{
    std::ifstream f(path.c_str());
    if (!f.good()) {
        std::cerr << "[ERR ] Cannot open file: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static unsigned int compileShader(GLenum type, const std::string& src, const std::string& tag)
{
    unsigned int s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "[ERR ] Shader compile (" << tag << "):\n" << log << "\n";
    }
    return s;
}

static unsigned int makeProgram(const std::string& vsRel, const std::string& fsRel)
{
    std::string vsSrc = readTextFile(findAsset(vsRel));
    std::string fsSrc = readTextFile(findAsset(fsRel));
    unsigned int vs = compileShader(GL_VERTEX_SHADER,   vsSrc, vsRel);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fsSrc, fsRel);
    unsigned int p  = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cerr << "[ERR ] Program link (" << vsRel << " + " << fsRel << "):\n" << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static unsigned int loadTexture(const std::string& relPath)
{
    std::string full = findAsset(relPath);
    stbi_set_flip_vertically_on_load(true);
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(full.c_str(), &w, &h, &n, 0);
    if (!data) {
        std::cerr << "[ERR ] Failed to load texture: " << full << "\n";
        return 0;
    }
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum fmt = (n == 4) ? GL_RGBA : (n == 3) ? GL_RGB : GL_RED;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return tex;
}

// ----------------------------------------------------------------------------
//  Mesh builders
//    Each vertex layout (world meshes) : vec3 pos | vec2 uv | vec3 normal = 8 floats
// ----------------------------------------------------------------------------
static unsigned int makeCubeVAO(unsigned int& outVBO)
{
    // Unit cube centered at origin, side = 1.
    float v[] = {
        // pos               uv         normal
        // -Z
        -0.5f,-0.5f,-0.5f,  0,0,   0, 0,-1,
         0.5f,-0.5f,-0.5f,  1,0,   0, 0,-1,
         0.5f, 0.5f,-0.5f,  1,1,   0, 0,-1,
         0.5f, 0.5f,-0.5f,  1,1,   0, 0,-1,
        -0.5f, 0.5f,-0.5f,  0,1,   0, 0,-1,
        -0.5f,-0.5f,-0.5f,  0,0,   0, 0,-1,
        // +Z
        -0.5f,-0.5f, 0.5f,  0,0,   0, 0, 1,
         0.5f,-0.5f, 0.5f,  1,0,   0, 0, 1,
         0.5f, 0.5f, 0.5f,  1,1,   0, 0, 1,
         0.5f, 0.5f, 0.5f,  1,1,   0, 0, 1,
        -0.5f, 0.5f, 0.5f,  0,1,   0, 0, 1,
        -0.5f,-0.5f, 0.5f,  0,0,   0, 0, 1,
        // -X
        -0.5f, 0.5f, 0.5f,  1,1,  -1, 0, 0,
        -0.5f, 0.5f,-0.5f,  0,1,  -1, 0, 0,
        -0.5f,-0.5f,-0.5f,  0,0,  -1, 0, 0,
        -0.5f,-0.5f,-0.5f,  0,0,  -1, 0, 0,
        -0.5f,-0.5f, 0.5f,  1,0,  -1, 0, 0,
        -0.5f, 0.5f, 0.5f,  1,1,  -1, 0, 0,
        // +X
         0.5f, 0.5f, 0.5f,  1,1,   1, 0, 0,
         0.5f, 0.5f,-0.5f,  0,1,   1, 0, 0,
         0.5f,-0.5f,-0.5f,  0,0,   1, 0, 0,
         0.5f,-0.5f,-0.5f,  0,0,   1, 0, 0,
         0.5f,-0.5f, 0.5f,  1,0,   1, 0, 0,
         0.5f, 0.5f, 0.5f,  1,1,   1, 0, 0,
        // -Y
        -0.5f,-0.5f,-0.5f,  0,1,   0,-1, 0,
         0.5f,-0.5f,-0.5f,  1,1,   0,-1, 0,
         0.5f,-0.5f, 0.5f,  1,0,   0,-1, 0,
         0.5f,-0.5f, 0.5f,  1,0,   0,-1, 0,
        -0.5f,-0.5f, 0.5f,  0,0,   0,-1, 0,
        -0.5f,-0.5f,-0.5f,  0,1,   0,-1, 0,
        // +Y
        -0.5f, 0.5f,-0.5f,  0,1,   0, 1, 0,
         0.5f, 0.5f,-0.5f,  1,1,   0, 1, 0,
         0.5f, 0.5f, 0.5f,  1,0,   0, 1, 0,
         0.5f, 0.5f, 0.5f,  1,0,   0, 1, 0,
        -0.5f, 0.5f, 0.5f,  0,0,   0, 1, 0,
        -0.5f, 0.5f,-0.5f,  0,1,   0, 1, 0,
    };
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));  glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));  glEnableVertexAttribArray(2);
    outVBO = vbo;
    return vao;
}

// Floor is built with VAO + VBO + EBO so the rubric's VAO/VBO/EBO
// requirement is demonstrated. 4 unique corners are indexed into 2 triangles.
static unsigned int makeFloorVAO(unsigned int& outVBO, unsigned int& outEBO, float half, float uvTile)
{
    float y = 0.0f;
    float v[] = {
        // pos                  uv                 normal
        -half, y, -half,        0.0f,   0.0f,      0,1,0,   // 0 : bottom-left
         half, y, -half,        uvTile, 0.0f,      0,1,0,   // 1 : bottom-right
         half, y,  half,        uvTile, uvTile,    0,1,0,   // 2 : top-right
        -half, y,  half,        0.0f,   uvTile,    0,1,0,   // 3 : top-left
    };
    unsigned int i[] = {
        0, 1, 2,   // first  triangle
        2, 3, 0,   // second triangle
    };
    unsigned int vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(i), i, GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));  glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));  glEnableVertexAttribArray(2);
    outVBO = vbo;
    outEBO = ebo;
    return vao;
}

// Coin billboard quad (double-sided) in the XY plane.
static unsigned int makeQuadVAO(unsigned int& outVBO)
{
    float v[] = {
        // pos              uv         normal (+Z)
        -0.35f,-0.35f, 0,   0,0,       0,0,1,
         0.35f,-0.35f, 0,   1,0,       0,0,1,
         0.35f, 0.35f, 0,   1,1,       0,0,1,
         0.35f, 0.35f, 0,   1,1,       0,0,1,
        -0.35f, 0.35f, 0,   0,1,       0,0,1,
        -0.35f,-0.35f, 0,   0,0,       0,0,1,
    };
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));  glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));  glEnableVertexAttribArray(2);
    outVBO = vbo;
    return vao;
}

// HUD : unit quad (position only, 0..1 in both axes)
static unsigned int makeHudQuadVAO(unsigned int& outVBO)
{
    float v[] = {
        0,0,  1,0,  1,1,
        1,1,  0,1,  0,0,
    };
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    outVBO = vbo;
    return vao;
}

// ----------------------------------------------------------------------------
//  Maze helpers
// ----------------------------------------------------------------------------
static glm::vec3 cellToWorld(int cx, int cz)
{
    // Center of cell in world space. Maze grid is centered on origin.
    float wx = (cx - MAZE_W * 0.5f + 0.5f) * CELL;
    float wz = (cz - MAZE_H * 0.5f + 0.5f) * CELL;
    return glm::vec3(wx, 0.0f, wz);
}

static bool cellIsWall(int cx, int cz)
{
    if (cx < 0 || cx >= MAZE_W || cz < 0 || cz >= MAZE_H) return true;
    return MAZE[cz][cx] == 'W';
}

static void buildLevel()
{
    g_walls.clear();
    g_coins.clear();
    for (int z = 0; z < MAZE_H; ++z) {
        for (int x = 0; x < MAZE_W; ++x) {
            char c = MAZE[z][x];
            glm::vec3 center = cellToWorld(x, z);
            if (c == 'W') {
                g_walls.push_back(glm::vec3(center.x, 1.0f, center.z));
            }
            else if (c == 'C') {
                g_coins.push_back({ glm::vec3(center.x, 0.8f, center.z), true, 0.0f });
            }
            else if (c == 'S') {
                g_player.pos   = glm::vec3(center.x, PLAYER_EYE_Y, center.z);
                g_player.yaw   = 0.0f;
                g_player.pitch = 0.0f;
            }
        }
    }
    g_coinsLeft = (int)g_coins.size();
    g_timeLeft  = GAME_TIME;
    g_state     = GameState::Playing;
    g_firstMouse = true;
}

// ----------------------------------------------------------------------------
//  Collision  - AABB vs circle in XZ plane.
//  Each wall cube occupies a CELL x CELL footprint centered on its position.
//  We move on one axis at a time and revert if we end up inside a wall.
// ----------------------------------------------------------------------------
static bool collides(const glm::vec3& p)
{
    const float half = CELL * 0.5f;
    for (const glm::vec3& w : g_walls) {
        float minX = w.x - half, maxX = w.x + half;
        float minZ = w.z - half, maxZ = w.z + half;
        float cx = glm::clamp(p.x, minX, maxX);
        float cz = glm::clamp(p.z, minZ, maxZ);
        float dx = p.x - cx;
        float dz = p.z - cz;
        if (dx * dx + dz * dz < PLAYER_R * PLAYER_R) return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
//  Input
// ----------------------------------------------------------------------------
static void mouseCallback(GLFWwindow*, double xpos, double ypos)
{
    if (g_firstMouse) {
        g_lastMouseX = xpos;
        g_lastMouseY = ypos;
        g_firstMouse = false;
        return;
    }
    float dx = (float)(xpos - g_lastMouseX) * MOUSE_SENS;
    float dy = (float)(g_lastMouseY - ypos) * MOUSE_SENS;
    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    g_player.yaw   += dx;
    g_player.pitch += dy;
    if (g_player.pitch >  85.0f) g_player.pitch =  85.0f;
    if (g_player.pitch < -85.0f) g_player.pitch = -85.0f;
}

static void framebufferSizeCallback(GLFWwindow*, int w, int h)
{
    SCR_W = (unsigned int)w;
    SCR_H = (unsigned int)h;
    glViewport(0, 0, w, h);
}

static void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    static bool rPrev = false;
    bool rNow = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);
    if (rNow && !rPrev) buildLevel();
    rPrev = rNow;

    if (g_state != GameState::Playing) return;

    // Forward vector in the XZ plane (ignore pitch for movement).
    glm::vec3 forward(
        cos(glm::radians(g_player.yaw)),
        0.0f,
        sin(glm::radians(g_player.yaw))
    );
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    glm::vec3 move(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= right;
    if (glm::length(move) > 0.001f) {
        g_isMoving = true;
        move = glm::normalize(move) * PLAYER_SPEED * g_deltaTime;
        // Axis-separated sliding collision.
        glm::vec3 np = g_player.pos;
        np.x += move.x;
        if (!collides(np)) g_player.pos.x = np.x;
        np = g_player.pos;
        np.z += move.z;
        if (!collides(np)) g_player.pos.z = np.z;
    } else {
        g_isMoving = false;
    }
}

// ----------------------------------------------------------------------------
//  Game update
// ----------------------------------------------------------------------------
static void updateGame()
{
    if (g_state != GameState::Playing) return;

    // Pickup check (distance in XZ).
    for (Coin& c : g_coins) {
        if (!c.alive) continue;
        c.spin += g_deltaTime * 2.5f;
        float dx = c.pos.x - g_player.pos.x;
        float dz = c.pos.z - g_player.pos.z;
        if (dx * dx + dz * dz < 0.8f * 0.8f) {
            c.alive = false;
            --g_coinsLeft;
            std::cout << "Coin collected! Remaining: " << g_coinsLeft << "\n";
        }
    }

    // Walk-bob phase used by the held-lantern model below.
    if (g_isMoving) g_walkPhase += g_deltaTime * 8.0f;

    g_timeLeft -= g_deltaTime;
    if (g_coinsLeft <= 0)   g_state = GameState::Won;
    else if (g_timeLeft <= 0.0f) { g_timeLeft = 0.0f; g_state = GameState::Lost; }
}

// ----------------------------------------------------------------------------
//  Rendering
// ----------------------------------------------------------------------------
static void setVec3 (unsigned int p, const char* n, const glm::vec3& v) { glUniform3fv (glGetUniformLocation(p, n), 1, glm::value_ptr(v)); }
static void setVec4 (unsigned int p, const char* n, const glm::vec4& v) { glUniform4fv (glGetUniformLocation(p, n), 1, glm::value_ptr(v)); }
static void setVec2 (unsigned int p, const char* n, const glm::vec2& v) { glUniform2fv (glGetUniformLocation(p, n), 1, glm::value_ptr(v)); }
static void setMat4 (unsigned int p, const char* n, const glm::mat4& m) { glUniformMatrix4fv(glGetUniformLocation(p, n), 1, GL_FALSE, glm::value_ptr(m)); }
static void setInt  (unsigned int p, const char* n, int i)              { glUniform1i (glGetUniformLocation(p, n), i); }
static void setFloat(unsigned int p, const char* n, float f)            { glUniform1f (glGetUniformLocation(p, n), f); }

int main()
{
    // ---- Pair 1 : GLFW window + GL context + main loop ---------------------
    if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "Rama - 3D Maze Collector", nullptr, nullptr);
    if (!window) { std::cerr << "glfwCreateWindow failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "glewInit failed\n"; return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---- Resources ---------------------------------------------------------
    unsigned int worldProg = makeProgram("Shaders/world.vert", "Shaders/world.frag");
    unsigned int hudProg   = makeProgram("Shaders/hud.vert",   "Shaders/hud.frag");

    unsigned int texWall = loadTexture("Assets/container.jpg");
    unsigned int texCoin = loadTexture("Assets/awesomeface.png");

    unsigned int cubeVBO, floorVBO, floorEBO, quadVBO, hudVBO;
    unsigned int cubeVAO  = makeCubeVAO (cubeVBO);
    unsigned int floorVAO = makeFloorVAO(floorVBO, floorEBO, (MAZE_W * CELL) * 0.5f, (float)MAZE_W);
    unsigned int quadVAO  = makeQuadVAO (quadVBO);
    unsigned int hudVAO   = makeHudQuadVAO(hudVBO);

    buildLevel();

    const glm::vec3 FOG_COLOR(0.55f, 0.62f, 0.70f);
    const float     FOG_DENSITY = 0.045f;
    const glm::vec3 LIGHT_DIR(-0.4f, -1.0f, -0.3f);

    // ---- Main loop ---------------------------------------------------------
    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        g_deltaTime = now - g_lastFrame;
        g_lastFrame = now;
        if (g_deltaTime > 0.05f) g_deltaTime = 0.05f; // clamp huge deltas

        glfwPollEvents();
        processInput(window);
        updateGame();

        glClearColor(FOG_COLOR.r, FOG_COLOR.g, FOG_COLOR.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- Build camera matrices (Pair 3) --------------------------------
        glm::vec3 front(
            cos(glm::radians(g_player.yaw)) * cos(glm::radians(g_player.pitch)),
            sin(glm::radians(g_player.pitch)),
            sin(glm::radians(g_player.yaw)) * cos(glm::radians(g_player.pitch))
        );
        glm::vec3 eye  = g_player.pos;
        glm::mat4 view = glm::lookAt(eye, eye + glm::normalize(front), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.0f),
            (float)SCR_W / (float)std::max(1u, SCR_H), 0.1f, 80.0f);

        // ---- World pass ----------------------------------------------------
        glUseProgram(worldProg);
        setMat4 (worldProg, "uView",       view);
        setMat4 (worldProg, "uProjection", proj);
        setVec3 (worldProg, "uLightDir",   LIGHT_DIR);
        setVec3 (worldProg, "uFogColor",   FOG_COLOR);
        setFloat(worldProg, "uFogDensity", FOG_DENSITY);
        setInt  (worldProg, "uTex",        0);
        setInt  (worldProg, "uDiscardAlpha", 0);

        glActiveTexture(GL_TEXTURE0);

        // Floor : drawn with glDrawElements (uses the EBO built in makeFloorVAO)
        glBindTexture(GL_TEXTURE_2D, texWall);
        setInt (worldProg, "uUseTexture", 1);
        setVec3(worldProg, "uTint", glm::vec3(0.85f, 0.85f, 0.85f));
        setMat4(worldProg, "uModel", glm::mat4(1.0f));
        glBindVertexArray(floorVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Walls (Pair 6 : obstacles forming the maze)
        setVec3(worldProg, "uTint", glm::vec3(0.95f, 0.85f, 0.75f));
        glBindVertexArray(cubeVAO);
        for (const glm::vec3& w : g_walls) {
            glm::mat4 m(1.0f);
            m = glm::translate(m, w);
            m = glm::scale(m, glm::vec3(CELL, 2.0f, CELL));
            setMat4(worldProg, "uModel", m);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Held lantern (Pair 5 : visible 3D character model).
        // A small coloured cube floats in front-right of the camera and bobs
        // as the player walks. Built from the cube mesh but with a model
        // matrix derived from the camera vectors so it follows the player.
        {
            glm::vec3 upV(0.0f, 1.0f, 0.0f);
            glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
            glm::vec3 rightV    = glm::normalize(glm::cross(flatFront, upV));
            float     bob       = g_isMoving ? sin(g_walkPhase) * 0.04f : 0.0f;
            glm::vec3 lanternPos = eye
                + flatFront * 0.55f
                + rightV    * 0.35f
                + glm::vec3(0.0f, -0.40f + bob, 0.0f);

            glm::mat4 m(1.0f);
            m = glm::translate(m, lanternPos);
            m = glm::rotate   (m, -glm::radians(g_player.yaw), upV);
            m = glm::rotate   (m,  now * 1.5f, glm::vec3(0, 1, 0));
            m = glm::scale    (m, glm::vec3(0.18f));

            setMat4(worldProg, "uModel", m);
            setInt (worldProg, "uUseTexture", 0);
            setVec3(worldProg, "uTint", glm::vec3(1.0f, 0.72f, 0.25f)); // warm bronze
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Coins (Pair 7 : collectibles)
        glBindTexture(GL_TEXTURE_2D, texCoin);
        setInt  (worldProg, "uUseTexture",   1);
        setInt  (worldProg, "uDiscardAlpha", 1);
        setVec3 (worldProg, "uTint", glm::vec3(1.0f));
        glDisable(GL_CULL_FACE);
        glBindVertexArray(quadVAO);
        for (const Coin& c : g_coins) {
            if (!c.alive) continue;
            glm::mat4 m(1.0f);
            m = glm::translate(m, c.pos);
            m = glm::rotate   (m, c.spin, glm::vec3(0, 1, 0));
            m = glm::scale    (m, glm::vec3(1.5f));
            setMat4(worldProg, "uModel", m);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        setInt(worldProg, "uDiscardAlpha", 0);

        // ---- HUD pass (Pair 7 : on-screen UI) ------------------------------
        glDisable(GL_DEPTH_TEST);
        glUseProgram(hudProg);
        glm::mat4 ortho = glm::ortho(0.0f, (float)SCR_W, 0.0f, (float)SCR_H);
        setMat4(hudProg, "uOrtho", ortho);
        glBindVertexArray(hudVAO);

        auto drawHudQuad = [&](float x, float y, float w, float h, glm::vec4 col) {
            setVec2(hudProg, "uOffset", glm::vec2(x, y));
            setVec2(hudProg, "uSize",   glm::vec2(w, h));
            setVec4(hudProg, "uColor",  col);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        };

        // Coin indicators (top-left corner)
        const float pad = 16.0f, sz = 28.0f, gap = 8.0f;
        for (size_t i = 0; i < g_coins.size(); ++i) {
            float x = pad + (float)i * (sz + gap);
            float y = (float)SCR_H - pad - sz;
            glm::vec4 col = g_coins[i].alive
                ? glm::vec4(1.0f, 0.82f, 0.1f, 1.0f)   // gold
                : glm::vec4(0.25f, 0.25f, 0.25f, 0.6f); // dim grey
            drawHudQuad(x, y, sz, sz, col);
        }

        // Timer bar (top-center)
        {
            float barW = 320.0f, barH = 14.0f;
            float x = ((float)SCR_W - barW) * 0.5f;
            float y = (float)SCR_H - pad - barH;
            drawHudQuad(x, y, barW, barH, glm::vec4(0, 0, 0, 0.55f));
            float frac = glm::clamp(g_timeLeft / GAME_TIME, 0.0f, 1.0f);
            glm::vec4 c = frac > 0.5f ? glm::vec4(0.2f, 0.9f, 0.3f, 0.95f)
                        : frac > 0.2f ? glm::vec4(0.95f, 0.8f, 0.2f, 0.95f)
                                      : glm::vec4(0.95f, 0.25f, 0.2f, 0.95f);
            drawHudQuad(x + 2, y + 2, (barW - 4) * frac, barH - 4, c);
        }

        // Win / Lose full-screen overlay
        if (g_state == GameState::Won) {
            drawHudQuad(0, 0, (float)SCR_W, (float)SCR_H, glm::vec4(0.2f, 0.8f, 0.3f, 0.35f));
            // thick "W" banner in the middle (just a green bar across)
            drawHudQuad((float)SCR_W * 0.25f, (float)SCR_H * 0.45f,
                (float)SCR_W * 0.5f, (float)SCR_H * 0.1f,
                glm::vec4(0.1f, 0.6f, 0.2f, 0.9f));
        }
        else if (g_state == GameState::Lost) {
            drawHudQuad(0, 0, (float)SCR_W, (float)SCR_H, glm::vec4(0.8f, 0.15f, 0.15f, 0.4f));
            drawHudQuad((float)SCR_W * 0.25f, (float)SCR_H * 0.45f,
                (float)SCR_W * 0.5f, (float)SCR_H * 0.1f,
                glm::vec4(0.6f, 0.1f, 0.1f, 0.9f));
        }

        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
    }

    // ---- Cleanup -----------------------------------------------------------
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteVertexArrays(1, &hudVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteBuffers(1, &floorEBO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &hudVBO);
    glDeleteTextures(1, &texWall);
    glDeleteTextures(1, &texCoin);
    glDeleteProgram(worldProg);
    glDeleteProgram(hudProg);

    glfwTerminate();
    return 0;
}
