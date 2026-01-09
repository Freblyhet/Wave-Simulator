#define GL_SILENCE_DEPRECATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

// Window
const int SCREEN_WIDTH = 1400;
const int SCREEN_HEIGHT = 900;
const int SIDEBAR_WIDTH = 350;  // Width of the side panel

static void getSizes(GLFWwindow* window, int& fbW, int& fbH, int& viewportW, int& viewportH) {
    // Use framebuffer size (pixels) so resizing and HiDPI behave correctly.
    glfwGetFramebufferSize(window, &fbW, &fbH);
    viewportW = std::max(1, fbW - SIDEBAR_WIDTH);
    viewportH = std::max(1, fbH);
}

// Grid
const int GRID_SIZE = 512;
const float PI = 3.14159265359f;

// Wave source
struct WaveSource {
    float x, y;
    float frequency;
    float amplitude;
    bool active;
    std::string name;
    
    WaveSource(float x, float y, float freq, float amp, const std::string& n = "Source") 
        : x(x), y(y), frequency(freq), amplitude(amp), active(true), name(n) {}
};

// Tool enum
enum class Tool {
    ADD_SOURCE,
    REMOVE_SOURCE,
    DRAW_WALL,
    ERASE_WALL,
    SNAP_WALL,
    INTERACT
};

// Simulation state
struct Simulation {
    std::vector<float> u;           // Current displacement
    std::vector<float> u_prev;      // Previous displacement
    std::vector<float> u_prev2;     // Two steps back
    std::vector<bool> walls;
    std::vector<WaveSource> sources;
    
    float time = 0.0f;
    // Physics
    // Note: this wave solver implicitly assumes a grid spacing of 1.
    // Keeping waveSpeed moderate (and dt stable) dramatically improves visual quality.
    float waveSpeed = 6.0f;
    float damping = 0.9995f;
    float dt = 1.0f / 60.0f; // base (used as a clamp/target)
    
    // Tools and interaction
    Tool currentTool = Tool::INTERACT;
    float newSourceFreq = 3.0f;
    float newSourceAmp = 1.5f;
    
    bool paused = false;
    float timeScale = 1.5f;
    
    // Mouse
    float mouseX = -10.0f;
    float mouseY = -10.0f;
    bool mousePressed = false;
    int lastMouseX = -1;
    int lastMouseY = -1;
    
    // Snap wall mode
    bool snapWallFirstPoint = true;
    int snapWallX1 = -1;
    int snapWallY1 = -1;
    
    // Visual
    enum ColorMode { BLUE_RED, RAINBOW, GRAYSCALE, CYAN_YELLOW };
    ColorMode colorMode = BLUE_RED;
    bool showSources = true;
    bool showWalls = true;
    bool showGrid = false;
    int gridSpacing = 32;  // Grid lines every N pixels
    int visualMode = 0;  // UI selection: 0=Rainbow, 1=Grayscale, 2=Blue-Red, 3=Cyan-Yellow
    float contrast = 1.5f;
    
    Simulation() {
        u.resize(GRID_SIZE * GRID_SIZE, 0.0f);
        u_prev.resize(GRID_SIZE * GRID_SIZE, 0.0f);
        u_prev2.resize(GRID_SIZE * GRID_SIZE, 0.0f);
        walls.resize(GRID_SIZE * GRID_SIZE, false);
    }
    
    // Screenshot notification system
    bool showScreenshotNotification = false;
    std::chrono::steady_clock::time_point screenshotNotificationTime;
} g_sim;

// OpenGL
GLuint g_shaderProgram = 0;
GLuint g_VAO = 0;
GLuint g_VBO = 0;
GLuint g_waveTexture = 0;
GLuint g_wallTexture = 0;
GLuint g_gridShaderProgram = 0;
GLuint g_gridVAO = 0;
GLuint g_gridVBO = 0;

// Grid to screen coordinates
glm::vec2 gridToScreen(float gx, float gy) {
    return glm::vec2((gx / GRID_SIZE) * 2.0f - 1.0f, (gy / GRID_SIZE) * 2.0f - 1.0f);
}

// Screen to grid coordinates
glm::vec2 screenToGrid(float sx, float sy) {
    float gx = ((sx + 1.0f) / 2.0f) * GRID_SIZE;
    float gy = ((sy + 1.0f) / 2.0f) * GRID_SIZE;
    return glm::vec2(gx, gy);
}

// Add wave source
void addSource(float x, float y, float freq, float amp) {
    g_sim.sources.emplace_back(x, y, freq, amp, "Source " + std::to_string(g_sim.sources.size() + 1));
}

// Remove wave source
void removeSource(float x, float y) {
    g_sim.sources.erase(
        std::remove_if(g_sim.sources.begin(), g_sim.sources.end(),
            [x, y](const WaveSource& src) {
                float dx = src.x - x;
                float dy = src.y - y;
                return std::sqrt(dx*dx + dy*dy) < 25.0f;
            }),
        g_sim.sources.end()
    );
}

// Set wall with brush
void setWall(int x, int y, bool state) {
    if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                    g_sim.walls[ny * GRID_SIZE + nx] = state;
                }
            }
        }
    }
}

// Bresenham line for wall drawing
void drawLine(int x0, int y0, int x1, int y1, bool drawWall) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        setWall(x0, y0, drawWall);
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Clear functions
void clearWaves() {
    std::fill(g_sim.u.begin(), g_sim.u.end(), 0.0f);
    std::fill(g_sim.u_prev.begin(), g_sim.u_prev.end(), 0.0f);
    std::fill(g_sim.u_prev2.begin(), g_sim.u_prev2.end(), 0.0f);
    g_sim.time = 0.0f;
}

void clearWalls() {
    std::fill(g_sim.walls.begin(), g_sim.walls.end(), false);
}

void clearSources() {
    g_sim.sources.clear();
}

// Screenshot functionality
void takeScreenshot() {
    // Create screenshots directory if it doesn't exist
    std::string projectRoot = std::filesystem::current_path().string();
    std::string screenshotsDir = projectRoot + "/WaveSimScreenshots";
    std::filesystem::create_directories(screenshotsDir);
    
    // Generate timestamp for filename
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::ostringstream filename;
    std::tm* tm = std::localtime(&time_t);
    filename << "wave_sim_" 
             << std::put_time(tm, "%Y%m%d_%H%M%S_") 
             << std::setfill('0') << std::setw(3) << ms.count() 
             << ".png";
    
    std::string fullPath = screenshotsDir + "/" + filename.str();
    
    // Use macOS screencapture utility
    std::string command = "screencapture -x " + fullPath;
    int result = std::system(command.c_str());
    
    if (result == 0) {
        std::cout << "📸 Screenshot saved: " << filename.str() << std::endl;
        // Show notification
        g_sim.showScreenshotNotification = true;
        g_sim.screenshotNotificationTime = std::chrono::steady_clock::now();
    } else {
        std::cout << "❌ Screenshot failed" << std::endl;
    }
}
// Load presets
void loadPreset(const std::string& name) {
    clearWaves();
    clearWalls();
    clearSources();
    
    if (name == "Double Slit") {
        // Two sources at top
        addSource(GRID_SIZE * 0.25f, GRID_SIZE * 0.8f, 5.0f, 2.0f);
        addSource(GRID_SIZE * 0.75f, GRID_SIZE * 0.8f, 5.0f, 2.0f);
        
        // Wall with two slits
        for (int y = GRID_SIZE * 0.45f; y < GRID_SIZE * 0.55f; y++) {
            for (int x = GRID_SIZE * 0.1f; x < GRID_SIZE * 0.9f; x++) {
                if (x < GRID_SIZE * 0.35f || 
                    (x > GRID_SIZE * 0.42f && x < GRID_SIZE * 0.58f) ||
                    x > GRID_SIZE * 0.65f) {
                    setWall(x, y, true);
                }
            }
        }
    } else if (name == "Ripple Tank") {
        // Central source
        addSource(GRID_SIZE * 0.5f, GRID_SIZE * 0.5f, 3.0f, 2.0f);
    } else if (name == "Interference") {
        // Two sources creating interference pattern
        addSource(GRID_SIZE * 0.3f, GRID_SIZE * 0.5f, 4.0f, 1.8f);
        addSource(GRID_SIZE * 0.7f, GRID_SIZE * 0.5f, 4.0f, 1.8f);
    } else if (name == "Reflection") {
        // Source on left, wall on right
        addSource(GRID_SIZE * 0.2f, GRID_SIZE * 0.5f, 3.0f, 2.0f);
        
        for (int y = GRID_SIZE * 0.2f; y < GRID_SIZE * 0.8f; y++) {
            for (int x = GRID_SIZE * 0.75f; x < GRID_SIZE * 0.78f; x++) {
                setWall(x, y, true);
            }
        }
    } else if (name == "Circular Arena") {
        // Circular boundary
        float cx = GRID_SIZE * 0.5f;
        float cy = GRID_SIZE * 0.5f;
        float radius = GRID_SIZE * 0.4f;
        
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist > radius && dist < radius + 10) {
                    g_sim.walls[y * GRID_SIZE + x] = true;
                }
            }
        }
        
        // Source in center
        addSource(cx, cy, 3.0f, 1.8f);
    }
    
    std::cout << "Loaded preset: " << name << std::endl;
}

// Update wave simulation
void updateSimulation(float deltaTime) {
    if (g_sim.paused) return;

    // Use a fixed-ish timestep for stability and consistent visuals.
    // We clamp large frame times and sub-step so waves don't "explode" or get mushy.
    float frameDt = std::clamp(deltaTime, 0.0f, 0.05f) * g_sim.timeScale;
    int steps = std::clamp(static_cast<int>(std::ceil(frameDt / g_sim.dt)), 1, 8);
    float dt = (steps > 0) ? (frameDt / steps) : 0.0f;

    // The Verlet update uses (c*dt)^2; this was previously using g_sim.dt regardless
    // of actual frame time, which makes the simulation feel wrong and can look noisy.
    float c2_dt2 = g_sim.waveSpeed * g_sim.waveSpeed * dt * dt;

    for (int s = 0; s < steps; ++s) {
        g_sim.time += dt;
    
        // Swap buffers
        std::swap(g_sim.u_prev2, g_sim.u_prev);
        std::swap(g_sim.u_prev, g_sim.u);

        // Wave equation with Verlet integration
        for (int y = 1; y < GRID_SIZE - 1; y++) {
            for (int x = 1; x < GRID_SIZE - 1; x++) {
                int idx = y * GRID_SIZE + x;

                if (g_sim.walls[idx]) {
                    g_sim.u[idx] = 0.0f;
                    continue;
                }

                // 5-point Laplacian
                float laplacian =
                    g_sim.u_prev[(y-1) * GRID_SIZE + x] +
                    g_sim.u_prev[(y+1) * GRID_SIZE + x] +
                    g_sim.u_prev[y * GRID_SIZE + (x-1)] +
                    g_sim.u_prev[y * GRID_SIZE + (x+1)] -
                    4.0f * g_sim.u_prev[idx];

                // Verlet with damping
                g_sim.u[idx] = 2.0f * g_sim.u_prev[idx] - g_sim.u_prev2[idx] + c2_dt2 * laplacian;
                g_sim.u[idx] *= g_sim.damping;
            }
        }

        // Apply wave sources
        for (const auto& src : g_sim.sources) {
            if (!src.active) continue;

            int sx = static_cast<int>(src.x);
            int sy = static_cast<int>(src.y);

            if (sx >= 5 && sx < GRID_SIZE - 5 && sy >= 5 && sy < GRID_SIZE - 5) {
                float value = src.amplitude * std::sin(2.0f * PI * src.frequency * g_sim.time);

                // Gaussian source
                for (int dy = -4; dy <= 4; dy++) {
                    for (int dx = -4; dx <= 4; dx++) {
                        float dist = std::sqrt(dx*dx + dy*dy);
                        if (dist < 5.0f) {
                            int idx = (sy + dy) * GRID_SIZE + (sx + dx);
                            if (!g_sim.walls[idx]) {
                                float falloff = std::exp(-dist * dist / 12.0f);
                                g_sim.u[idx] += value * falloff;
                            }
                        }
                    }
                }
            }
        }
    }
}

// OpenGL shader
bool initOpenGL() {
    const char* vertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";
    
    const char* fragmentShader = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D waveTex;
        uniform sampler2D wallTex;
        uniform int colorMode;
        uniform float uContrast;
        
        vec3 hsv2rgb(vec3 c) {
            vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }
        
        void main() {
            float isWall = texture(wallTex, TexCoord).r;
            
            if (isWall > 0.5) {
                FragColor = vec4(0.15, 0.15, 0.15, 1.0);
                return;
            }
            
            float h = texture(waveTex, TexCoord).r;
            
            // Wave energy is proportional to amplitude squared
            float energy = h * h;
            
            // Enhanced contrast for displacement (user adjustable)
            h = tanh(h * uContrast) * 0.9;
            
            vec3 color;
            
            if (colorMode == 0) {
                // Energy-based: Red (low) to Blue (high) - like heat map inverted
                float t = clamp(energy * 2.0, 0.0, 1.0);
                
                if (t < 0.5) {
                    // Low energy: dark red to orange
                    color = mix(vec3(0.3, 0.0, 0.0), vec3(1.0, 0.3, 0.0), t * 2.0);
                } else {
                    // High energy: yellow to cyan to blue
                    float t2 = (t - 0.5) * 2.0;
                    color = mix(vec3(1.0, 0.8, 0.0), vec3(0.0, 0.5, 1.0), t2);
                }
                
                // Darken low energy areas
                if (energy < 0.05) {
                    color = mix(vec3(0.05, 0.05, 0.1), color, energy * 20.0);
                }
            } else if (colorMode == 1) {
                // Rainbow with better contrast
                float hue = 0.65 - h * 0.5;
                float sat = 0.85;
                float val = 0.4 + abs(h) * 0.6;
                color = hsv2rgb(vec3(hue, sat, val));
            } else if (colorMode == 2) {
                // Grayscale with better contrast
                float intensity = 0.3 + h * 0.7;
                color = vec3(intensity);
            } else {
                // Cyan-Yellow with better separation
                if (h > 0.05) {
                    float t = clamp(h * 1.8, 0.0, 1.0);
                    color = mix(vec3(0.1, 0.4, 0.6), vec3(1.0, 0.95, 0.2), pow(t, 0.8));
                } else if (h < -0.05) {
                    float t = clamp(-h * 1.8, 0.0, 1.0);
                    color = mix(vec3(0.1, 0.4, 0.6), vec3(0.0, 0.15, 0.3), pow(t, 0.8));
                } else {
                    color = vec3(0.05, 0.3, 0.5);
                }
            }
            
            FragColor = vec4(color, 1.0);
        }
    )";
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShader, nullptr);
    glCompileShader(vs);
    
    GLint success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        std::cerr << "Vertex shader error:\n" << infoLog << std::endl;
        return false;
    }
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShader, nullptr);
    glCompileShader(fs);
    
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        std::cerr << "Fragment shader error:\n" << infoLog << std::endl;
        return false;
    }
    
    g_shaderProgram = glCreateProgram();
    glAttachShader(g_shaderProgram, vs);
    glAttachShader(g_shaderProgram, fs);
    glLinkProgram(g_shaderProgram);
    
    glGetProgramiv(g_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(g_shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader linking error:\n" << infoLog << std::endl;
        return false;
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    // Setup quad
    float vertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    unsigned int indices[] = {0, 1, 2, 0, 2, 3};
    
    GLuint EBO;
    glGenVertexArrays(1, &g_VAO);
    glGenBuffers(1, &g_VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(g_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Create textures
    glGenTextures(1, &g_waveTexture);
    glBindTexture(GL_TEXTURE_2D, g_waveTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    std::vector<float> zeros(GRID_SIZE * GRID_SIZE, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, GRID_SIZE, GRID_SIZE, 0, GL_RED, GL_FLOAT, zeros.data());
    
    glGenTextures(1, &g_wallTexture);
    glBindTexture(GL_TEXTURE_2D, g_wallTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, GRID_SIZE, GRID_SIZE, 0, GL_RED, GL_FLOAT, zeros.data());
    
    // Grid shader
    const char* gridVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";
    
    const char* gridFragmentShader = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(0.3, 0.3, 0.3, 0.4);
        }
    )";
    
    GLuint gridVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(gridVS, 1, &gridVertexShader, nullptr);
    glCompileShader(gridVS);
    
    GLuint gridFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(gridFS, 1, &gridFragmentShader, nullptr);
    glCompileShader(gridFS);
    
    g_gridShaderProgram = glCreateProgram();
    glAttachShader(g_gridShaderProgram, gridVS);
    glAttachShader(g_gridShaderProgram, gridFS);
    glLinkProgram(g_gridShaderProgram);
    
    glDeleteShader(gridVS);
    glDeleteShader(gridFS);
    
    glGenVertexArrays(1, &g_gridVAO);
    glGenBuffers(1, &g_gridVBO);
    
    return true;
}

// Render wave field
void renderWaves() {
    // Update textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_waveTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GRID_SIZE, GRID_SIZE, GL_RED, GL_FLOAT, g_sim.u.data());
    
    std::vector<float> wallData(GRID_SIZE * GRID_SIZE);
    for (size_t i = 0; i < g_sim.walls.size(); i++) {
        wallData[i] = g_sim.walls[i] ? 1.0f : 0.0f;
    }
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_wallTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GRID_SIZE, GRID_SIZE, GL_RED, GL_FLOAT, wallData.data());
    
    // Render
    glUseProgram(g_shaderProgram);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "waveTex"), 0);
    glUniform1i(glGetUniformLocation(g_shaderProgram, "wallTex"), 1);

    // UI uses visualMode, shader uses colorMode.
    // Map the UI selection to the active ColorMode enum used by the shader.
    switch (g_sim.visualMode) {
        case 0: g_sim.colorMode = Simulation::RAINBOW; break;
        case 1: g_sim.colorMode = Simulation::GRAYSCALE; break;
        case 2: g_sim.colorMode = Simulation::BLUE_RED; break;
        case 3: g_sim.colorMode = Simulation::CYAN_YELLOW; break;
        default: g_sim.colorMode = Simulation::BLUE_RED; break;
    }

    glUniform1i(glGetUniformLocation(g_shaderProgram, "colorMode"), (int)g_sim.colorMode);
    glUniform1f(glGetUniformLocation(g_shaderProgram, "uContrast"), g_sim.contrast);
    
    glBindVertexArray(g_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Render grid overlay
void renderGrid() {
    if (!g_sim.showGrid) return;
    
    std::vector<float> gridVertices;
    
    // Vertical lines
    for (int x = g_sim.gridSpacing; x < GRID_SIZE; x += g_sim.gridSpacing) {
        glm::vec2 top = gridToScreen(x, 0);
        glm::vec2 bottom = gridToScreen(x, GRID_SIZE);
        gridVertices.push_back(top.x);
        gridVertices.push_back(top.y);
        gridVertices.push_back(bottom.x);
        gridVertices.push_back(bottom.y);
    }
    
    // Horizontal lines
    for (int y = g_sim.gridSpacing; y < GRID_SIZE; y += g_sim.gridSpacing) {
        glm::vec2 left = gridToScreen(0, y);
        glm::vec2 right = gridToScreen(GRID_SIZE, y);
        gridVertices.push_back(left.x);
        gridVertices.push_back(left.y);
        gridVertices.push_back(right.x);
        gridVertices.push_back(right.y);
    }
    
    if (gridVertices.empty()) return;
    
    glBindVertexArray(g_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(g_gridShaderProgram);
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, gridVertices.size() / 2);
    
    glDisable(GL_BLEND);
}
void renderGUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Create side panel docked to the right, based on current display size.
    ImGuiIO& io = ImGui::GetIO();
    const float sidebarWidth = static_cast<float>(SIDEBAR_WIDTH);
    const float screenW = io.DisplaySize.x;
    const float screenH = io.DisplaySize.y;
    const float viewportW = std::max(1.0f, screenW - sidebarWidth);
    ImGui::SetNextWindowPos(ImVec2(viewportW, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, screenH), ImGuiCond_Always);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
    if (ImGui::Begin("Control Panel", nullptr, window_flags)) {
        
        // Menu bar with quick actions - Particle Life style
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Presets")) {
                if (ImGui::MenuItem("Double Slit")) {
                    loadPreset("Double Slit");
                }
                if (ImGui::MenuItem("Ripple Tank")) {
                    loadPreset("Ripple Tank");
                }
                if (ImGui::MenuItem("Wave Interference")) {
                    loadPreset("Wave Interference");
                }
                if (ImGui::MenuItem("Reflection Demo")) {
                    loadPreset("Reflection Demo");
                }
                if (ImGui::MenuItem("Circular Arena")) {
                    loadPreset("Circular Arena");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Actions")) {
                if (ImGui::MenuItem("Clear All", "R")) {
                    clearWaves();
                    clearWalls();
                    clearSources();
                }
                if (ImGui::MenuItem("Clear Waves", "C")) {
                    clearWaves();
                }
                if (ImGui::MenuItem("Take Screenshot", "P")) {
                    takeScreenshot();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        
        // Status section - modern style
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("STATUS");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        ImGui::Text("Wave Sources: %zu", g_sim.sources.size());
        ImGui::Text("Simulation Time: %.2f s", g_sim.time);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Spacing();
        
        // Quick controls section
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("SIMULATION CONTROLS");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        // Modern button layout
        if (ImGui::Button(g_sim.paused ? "▶ Resume" : "⏸ Pause", ImVec2(100, 0))) {
            g_sim.paused = !g_sim.paused;
        }
        ImGui::SameLine();
        if (ImGui::Button("🔄 Reset All", ImVec2(100, 0))) {
            clearWaves();
            clearWalls();
            clearSources();
        }
        ImGui::SameLine();
        if (ImGui::Button("📸 Screenshot", ImVec2(100, 0))) {
            takeScreenshot();
        }
        
        if (ImGui::Button("🌊 Clear Waves", ImVec2(100, 0))) {
            clearWaves();
        }
        ImGui::SameLine();
        if (ImGui::Button("🧱 Clear Walls", ImVec2(100, 0))) {
            clearWalls();
        }
        ImGui::SameLine();
        if (ImGui::Button("🎯 Clear Sources", ImVec2(100, 0))) {
            clearSources();
        }
        
        ImGui::SliderFloat("Time Scale", &g_sim.timeScale, 0.1f, 5.0f, "%.1fx");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjust simulation speed");
        }
        ImGui::Spacing();
        
        // Interaction tools section
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("INTERACTION TOOLS");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        // Mode selection with better styling
        ImGui::RadioButton("🌊 Interact Mode", (int*)&g_sim.currentTool, (int)Tool::INTERACT);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click anywhere to create ripple effects");
        }
        
        ImGui::RadioButton("➕ Add Source", (int*)&g_sim.currentTool, (int)Tool::ADD_SOURCE);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Place persistent wave sources");
        }
        
        ImGui::RadioButton("❌ Remove Source", (int*)&g_sim.currentTool, (int)Tool::REMOVE_SOURCE);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click to remove wave sources");
        }
        
        ImGui::RadioButton("🖊️ Draw Wall", (int*)&g_sim.currentTool, (int)Tool::DRAW_WALL);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Drag to draw barriers");
        }
        
        ImGui::RadioButton("🧽 Erase Wall", (int*)&g_sim.currentTool, (int)Tool::ERASE_WALL);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Drag to remove barriers");
        }
        
        ImGui::RadioButton("📏 Snap Wall", (int*)&g_sim.currentTool, (int)Tool::SNAP_WALL);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Two-click mode for straight walls");
        }
        
        // Tool-specific controls
        if (g_sim.currentTool == Tool::SNAP_WALL) {
            ImGui::Indent();
            if (g_sim.snapWallFirstPoint) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "📍 Click first point...");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "📍 Click second point...");
                ImGui::Text("First: (%d, %d)", g_sim.snapWallX1, g_sim.snapWallY1);
            }
            ImGui::Unindent();
        }
        
        if (g_sim.currentTool == Tool::ADD_SOURCE) {
            ImGui::Indent();
            ImGui::Text("New Source Parameters:");
            ImGui::SliderFloat("Frequency", &g_sim.newSourceFreq, 0.5f, 10.0f, "%.1f Hz");
            ImGui::SliderFloat("Amplitude", &g_sim.newSourceAmp, 0.5f, 5.0f, "%.2f");
            ImGui::Unindent();
        }
        ImGui::Spacing();
        
        // Physics section
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("PHYSICS PARAMETERS");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        ImGui::SliderFloat("Wave Speed", &g_sim.waveSpeed, 0.5f, 50.0f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Speed of wave propagation");
        }
        
        ImGui::SliderFloat("Damping", &g_sim.damping, 0.98f, 0.9999f, "%.4f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Energy loss factor (higher = less damping)");
        }
        ImGui::Spacing();
        
        // Visual effects section
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("VISUAL EFFECTS");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
    const char* visualModes[] = { "Rainbow", "Grayscale", "Blue-Red", "Cyan-Yellow" };
        ImGui::Combo("Visualization", &g_sim.visualMode, visualModes, IM_ARRAYSIZE(visualModes));

        // Small preview swatch to make it obvious which scheme is active.
        {
            ImVec4 c1, c2, c3;
            switch (g_sim.visualMode) {
                case 0: // Rainbow
                    c1 = ImVec4(0.2f, 0.2f, 1.0f, 1.0f);
                    c2 = ImVec4(0.2f, 1.0f, 0.3f, 1.0f);
                    c3 = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
                    break;
                case 1: // Grayscale
                    c1 = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
                    c2 = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                    c3 = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
                    break;
                case 2: // Blue-Red
                    c1 = ImVec4(0.1f, 0.2f, 0.8f, 1.0f);
                    c2 = ImVec4(0.2f, 0.8f, 1.0f, 1.0f);
                    c3 = ImVec4(1.0f, 0.3f, 0.1f, 1.0f);
                    break;
                default: // Cyan-Yellow
                    c1 = ImVec4(0.05f, 0.3f, 0.6f, 1.0f);
                    c2 = ImVec4(0.2f, 0.8f, 0.9f, 1.0f);
                    c3 = ImVec4(1.0f, 0.95f, 0.2f, 1.0f);
                    break;
            }

            const ImVec2 swatchSize(18.0f, 10.0f);
            ImGui::SameLine();
            ImGui::TextDisabled("Preview:");
            ImGui::SameLine();
            ImGui::ColorButton("##v1", c1, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, swatchSize);
            ImGui::SameLine();
            ImGui::ColorButton("##v2", c2, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, swatchSize);
            ImGui::SameLine();
            ImGui::ColorButton("##v3", c3, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, swatchSize);
        }
        
        ImGui::Checkbox("Show Grid", &g_sim.showGrid);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(G key)");
        
        ImGui::SliderFloat("Contrast", &g_sim.contrast, 0.5f, 5.0f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjust wave visualization contrast");
        }
        ImGui::Spacing();
        
        // Wave sources management
        if (g_sim.sources.size() > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
            ImGui::Text("WAVE SOURCES (%zu)", g_sim.sources.size());
            ImGui::PopStyleColor();
            ImGui::Separator();
            
            ImGui::BeginChild("SourcesList", ImVec2(0, 200), true);
            for (size_t i = 0; i < g_sim.sources.size(); i++) {
                auto& source = g_sim.sources[i];
                
                ImGui::PushID((int)i);
                
                // Source header with status
                ImGui::Text("🌊 %s", source.name.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton(source.active ? "🟢 ON" : "🔴 OFF")) {
                    source.active = !source.active;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("❌ Delete")) {
                    g_sim.sources.erase(g_sim.sources.begin() + i);
                    ImGui::PopID();
                    break;
                }
                
                // Source parameters
                ImGui::Text("Position: (%.0f, %.0f)", source.x, source.y);
                ImGui::SliderFloat("Freq##freq", &source.frequency, 0.5f, 10.0f, "%.1f Hz");
                ImGui::SliderFloat("Amp##amp", &source.amplitude, 0.1f, 5.0f, "%.2f");
                
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        
        // Keyboard shortcuts section  
        if (ImGui::CollapsingHeader("⌨️ Keyboard Shortcuts")) {
            ImGui::BulletText("SPACE - Pause/Resume simulation");
            ImGui::BulletText("P - Take screenshot");
            ImGui::BulletText("R - Reset everything");
            ImGui::BulletText("C - Clear waves only");
            ImGui::BulletText("G - Toggle grid");
            ImGui::BulletText("ESC - Cancel snap wall mode");
        }
    }
    ImGui::End();
    
    // Screenshot notification (same as before)
    if (g_sim.showScreenshotNotification) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_sim.screenshotNotificationTime).count();
        
        if (elapsed < 3000) {  // Show for 3 seconds
            ImGuiIO& io2 = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(10.0f, io2.DisplaySize.y - 60.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, 50), ImGuiCond_Always);
            ImGui::Begin("Screenshot Notification", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "📸 Screenshot saved successfully!");
            ImGui::End();
        } else {
            g_sim.showScreenshotNotification = false;
        }
    }
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Mouse callback
void mouseCallback(GLFWwindow* window, double x, double y) {
    // Important: GLFW cursor positions are in *window* coordinates (points), not framebuffer pixels.
    // On HiDPI displays fbW/fbH can be ~2x, which makes the mapping feel "super slow".
    int winW = 0, winH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    winW = std::max(1, winW);
    winH = std::max(1, winH);

    // Mirror the viewport split in window coordinates.
    // NOTE: The simulation rendering is sized in framebuffer pixels, while cursor positions are
    // in window coords. Use ImGui's framebuffer scale to convert measurements consistently.
    const float fbScaleX = (ImGui::GetIO().DisplayFramebufferScale.x > 0.0f)
                               ? ImGui::GetIO().DisplayFramebufferScale.x
                               : 1.0f;
    const float fbScaleY = (ImGui::GetIO().DisplayFramebufferScale.y > 0.0f)
                               ? ImGui::GetIO().DisplayFramebufferScale.y
                               : 1.0f;

    const float viewportW = std::max(1.0f, (static_cast<float>(winW) * fbScaleX) - static_cast<float>(SIDEBAR_WIDTH));
    const float viewportH = std::max(1.0f, (static_cast<float>(winH) * fbScaleY));

    // If the UI is using the mouse, don't update interaction coords.
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    // Only handle mouse input in the simulation viewport (left side)
    // x/y are in window coords; compare against the viewport split in window coords.
    // Convert the sidebar width to window coords to keep the boundary check consistent.
    const float sidebarWidthWindow = static_cast<float>(SIDEBAR_WIDTH) / fbScaleX;
    const float viewportWWindow = std::max(1.0f, static_cast<float>(winW) - sidebarWidthWindow);

    if (static_cast<float>(x) >= viewportWWindow) {
        g_sim.mousePressed = false; // Disable interaction in side panel area
        return;
    }

    // Store mouse directly in grid coordinates to avoid aspect/viewport mapping issues.
    // x,y are in window coords with origin at top-left.
    const float nx = (static_cast<float>(x) * fbScaleX) / viewportW; // 0..1
    const float ny = (static_cast<float>(y) * fbScaleY) / viewportH; // 0..1 (top->bottom)

    g_sim.mouseX = nx * static_cast<float>(GRID_SIZE);
    g_sim.mouseY = (1.0f - ny) * static_cast<float>(GRID_SIZE); // flip so 0 is bottom
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_sim.mousePressed = true;
        } else {
            g_sim.mousePressed = false;
            // Reset drag state for non-snap tools
            if (g_sim.currentTool != Tool::SNAP_WALL) {
                g_sim.lastMouseX = -1;
                g_sim.lastMouseY = -1;
            }
        }
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_SPACE) {
            g_sim.paused = !g_sim.paused;
        } else if (key == GLFW_KEY_R) {
            clearWaves();
            clearWalls();
            clearSources();
        } else if (key == GLFW_KEY_C) {
            clearWaves();
        } else if (key == GLFW_KEY_G) {
            g_sim.showGrid = !g_sim.showGrid;
        } else if (key == GLFW_KEY_P) {
            takeScreenshot();
        } else if (key == GLFW_KEY_ESCAPE) {
            // Cancel snap wall mode
            if (g_sim.currentTool == Tool::SNAP_WALL && !g_sim.snapWallFirstPoint) {
                g_sim.snapWallFirstPoint = true;
                g_sim.snapWallX1 = -1;
                g_sim.snapWallY1 = -1;
            }
        }
    }
}

// Handle mouse interaction
void handleMouseInput(GLFWwindow* window) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    
    if (!g_sim.mousePressed) return;
    
    // Mouse is already stored in grid coordinates (see mouseCallback).
    int gridX = static_cast<int>(g_sim.mouseX);
    int gridY = static_cast<int>(g_sim.mouseY);
    
    // Bounds check
    if (gridX < 0 || gridX >= GRID_SIZE || gridY < 0 || gridY >= GRID_SIZE) {
        return;
    }
    
    // Handle tools
    if (g_sim.currentTool == Tool::ADD_SOURCE) {
        // Single click to add source
        if (g_sim.lastMouseX == -1) {
            addSource(static_cast<float>(gridX), static_cast<float>(gridY), g_sim.newSourceFreq, g_sim.newSourceAmp);
        }
        g_sim.lastMouseX = gridX;
        g_sim.lastMouseY = gridY;
        
    } else if (g_sim.currentTool == Tool::INTERACT) {
        // Create ripple effect at mouse position
        if (g_sim.lastMouseX == -1) {
            // Create a ripple effect
            int radius = 15;
            float amplitude = 2.0f;
            
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = gridX + dx;
                    int ny = gridY + dy;
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                        float dist = std::sqrt(dx*dx + dy*dy);
                        if (dist <= radius && !g_sim.walls[ny * GRID_SIZE + nx]) {
                            float falloff = 1.0f - (dist / radius);
                            g_sim.u[ny * GRID_SIZE + nx] += amplitude * falloff * falloff;
                        }
                    }
                }
            }
        }
        g_sim.lastMouseX = gridX;
        g_sim.lastMouseY = gridY;
    } else if (g_sim.currentTool == Tool::REMOVE_SOURCE) {
        // Single click to remove source
        if (g_sim.lastMouseX == -1) {
            removeSource(static_cast<float>(gridX), static_cast<float>(gridY));
        }
        g_sim.lastMouseX = gridX;
        g_sim.lastMouseY = gridY;
        
    } else if (g_sim.currentTool == Tool::SNAP_WALL) {
        // Two-click snap wall mode
        if (g_sim.snapWallFirstPoint) {
            // First click - store position
            g_sim.snapWallX1 = gridX;
            g_sim.snapWallY1 = gridY;
            g_sim.snapWallFirstPoint = false;
            std::cout << "Snap wall: First point at (" << gridX << ", " << gridY << ")" << std::endl;
        } else {
            // Second click - draw line from first to second point
            std::cout << "Snap wall: Second point at (" << gridX << ", " << gridY << "), drawing line..." << std::endl;
            drawLine(g_sim.snapWallX1, g_sim.snapWallY1, gridX, gridY, true);
            g_sim.snapWallFirstPoint = true;
            g_sim.snapWallX1 = -1;
            g_sim.snapWallY1 = -1;
        }
        g_sim.mousePressed = false;  // Consume the click
        
    } else if (g_sim.currentTool == Tool::DRAW_WALL || g_sim.currentTool == Tool::ERASE_WALL) {
        bool drawWall = (g_sim.currentTool == Tool::DRAW_WALL);
        
        if (g_sim.lastMouseX >= 0 && g_sim.lastMouseY >= 0) {
            drawLine(g_sim.lastMouseX, g_sim.lastMouseY, gridX, gridY, drawWall);
        } else {
            setWall(gridX, gridY, drawWall);
        }
        
        g_sim.lastMouseX = gridX;
        g_sim.lastMouseY = gridY;
    }
}

// Main
int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, 
                                          "Wave Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
    std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
    
    if (!initOpenGL()) {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        return -1;
    }
    
    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Modern dark theme with improved colors and spacing
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Modern rounded corners
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 6.0f;
    
    // Improved spacing
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 16.0f;
    style.GrabMinSize = 12.0f;
    
    // Modern color scheme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    std::cout << "\n=== Wave Simulator ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  SPACE - Pause/Resume" << std::endl;
    std::cout << "  R - Reset everything" << std::endl;
    std::cout << "  C - Clear waves only" << std::endl;
    std::cout << "  G - Toggle grid" << std::endl;
    std::cout << "  P - Take screenshot" << std::endl;
    std::cout << "  ESC - Cancel snap wall mode" << std::endl;
    std::cout << "  Left Click - Use selected tool" << std::endl;
    std::cout << "\nNew Features:" << std::endl;
    std::cout << "  - Snap Wall: Click 2 points to draw straight walls" << std::endl;
    std::cout << "  - Grid Overlay: Press G or toggle in Visual menu" << std::endl;
    std::cout << "  - Enhanced Contrast: Better wave visualization" << std::endl;
    std::cout << "  - Faster Wave Speed: Increased default propagation" << std::endl;
    std::cout << "\nTry the presets to see wave interference, diffraction, and reflection!" << std::endl;
    
    // Timing
    double lastTime = glfwGetTime();
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Delta time
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        
        // Update
        updateSimulation(deltaTime);
        handleMouseInput(window);
        
        // Render
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

    // Set viewport to simulation area only (left side)
    int fbW = 0, fbH = 0, viewportW = 0, viewportH = 0;
    getSizes(window, fbW, fbH, viewportW, viewportH);
    glViewport(0, 0, viewportW, viewportH);
        
        renderWaves();
        renderGrid();
        
        // Reset viewport for ImGui rendering
    glViewport(0, 0, fbW, fbH);
        renderGUI();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glDeleteVertexArrays(1, &g_VAO);
    glDeleteBuffers(1, &g_VBO);
    glDeleteVertexArrays(1, &g_gridVAO);
    glDeleteBuffers(1, &g_gridVBO);
    glDeleteTextures(1, &g_waveTexture);
    glDeleteTextures(1, &g_wallTexture);
    glDeleteProgram(g_shaderProgram);
    glDeleteProgram(g_gridShaderProgram);
    
    glfwTerminate();
    
    std::cout << "\nSimulation ended" << std::endl;
    return 0;
}