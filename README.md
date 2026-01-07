# Wave Simulator

A real-time 2D wave propagation simulator built with OpenGL and C++. Experience classic wave phenomena like interference, diffraction, and reflection through an interactive visualization.

## Features

- **Real-time wave simulation** using the 2D wave equation
- **Interactive wave sources** with customizable frequency and amplitude
- **Dynamic wall drawing** for creating barriers and obstacles
- **Multiple visualization modes** including rainbow, grayscale, and color gradients
- **Built-in presets** demonstrating classic wave phenomena:
  - Double-slit experiment
  - Ripple tank simulation
  - Wave interference patterns
  - Reflection demonstrations
  - Circular wave arena

## Screenshots

![Wave Simulation Demo](WaveSimScreenshots/wave1.png)

![Double Slit](WaveSimScreenshots/wave2.png)

![Interference Pattern](WaveSimScreenshots/wave3.png)

![Wave Reflection](WaveSimScreenshots/wave4.png)

## Requirements

- **macOS** (tested on macOS with Apple Silicon)
- **Xcode Command Line Tools** or equivalent C++ compiler
- **OpenGL 3.3+** support
- **Homebrew** (recommended for dependency management)

## Dependencies

This project uses the following libraries (included in the repository):
- **GLFW 3.4** - Window management and input handling
- **GLAD** - OpenGL function loader
- **Dear ImGui** - Immediate mode GUI for controls
- **GLM** - OpenGL mathematics library

## Quick Start

### 1. Clone the Repository
```bash
git clone https://github.com/Freblyhet/Wave-Simulator.git
cd Wave-Simulator
```

### 2. Install Dependencies (if not already installed)
```bash
# Install GLFW via Homebrew
brew install glfw
```

### 3. Build the Project
The project includes a pre-configured build task. Simply run:
```bash
clang++ src/WaveSim.cpp src/glad.c lib/imgui/imgui.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_widgets.cpp lib/imgui/imgui_tables.cpp lib/imgui/backends/imgui_impl_glfw.cpp lib/imgui/backends/imgui_impl_opengl3.cpp -Iinclude -Iglfw-3.4/include -Ilib/imgui -Ilib/imgui/backends -I/opt/homebrew/include -L/opt/homebrew/lib -lglfw -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL -framework CoreFoundation -std=c++17 -o "Wave Sim"
```

Or if you have VS Code with the provided tasks.json:
```bash
# In VS Code: Cmd+Shift+P -> "Tasks: Run Build Task"
```

### 4. Run the Simulator
```bash
./Wave\ Sim
```

## How to Use

### Basic Controls
- **Left Mouse**: Interact based on selected tool
- **Space**: Pause/Resume simulation
- **R**: Reset entire simulation
- **C**: Clear waves only
- **W**: Clear walls only
- **S**: Clear sources only

### Tools
1. **Add Source**: Click anywhere to add a wave source
2. **Remove Source**: Click on existing sources to remove them
3. **Draw Wall**: Click and drag to draw barrier walls
4. **Erase Wall**: Click and drag to erase walls
5. **Snap Wall**: Click two points to create a straight wall

### GUI Panel
The control panel on the left provides:
- **Simulation controls**: Play/pause, time scaling, reset options
- **Presets**: Quick setups for classic wave experiments
- **Physics parameters**: Wave speed and damping adjustment
- **Visual options**: Color modes, grid overlay, display toggles
- **Source management**: Individual control over each wave source

## Physics

The simulator implements the 2D wave equation:
```
∂²u/∂t² = c²(∂²u/∂x² + ∂²u/∂y²)
```

Where:
- `u(x,y,t)` is the wave amplitude at position (x,y) and time t
- `c` is the wave speed (adjustable in the GUI)
- Damping is applied to simulate energy dissipation

## Project Structure

```
Wave-Simulator/
├── src/
│   ├── WaveSim.cpp          # Main application source
│   └── glad.c               # OpenGL loader
├── include/
│   ├── glad/                # OpenGL headers
│   └── KHR/                 # Khronos headers
├── lib/
│   ├── imgui/               # Dear ImGui library
│   └── libglfw3.a          # Pre-built GLFW library
├── resources/
│   └── shaders/            # GLSL shader files
├── WaveSimScreenshots/     # Demo images
└── glfw-3.4/              # GLFW source (reference)
```

## Example Experiments

### 1. Double Slit Experiment
1. Load the "Double Slit" preset
2. Observe the characteristic interference pattern
3. Try adjusting the frequency to see how wavelength affects the pattern

### 2. Wave Interference
1. Add two sources close to each other
2. Set them to the same frequency
3. Watch constructive and destructive interference patterns form

### 3. Reflection Studies
1. Draw walls at various angles
2. Add a source and observe how waves reflect off barriers
3. Try creating a parabolic reflector shape

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

## License

This project is open source. Feel free to use, modify, and distribute according to your needs.

## Support

If you encounter any issues or have questions:
1. Check the [Issues](https://github.com/Freblyhet/Wave-Simulator/issues) page
2. Create a new issue if your problem isn't already documented
3. Provide details about your system and the steps to reproduce any bugs

## Author

Adam Blyberg  
[GitHub](https://github.com/Freblyhet)

---

**Enjoy exploring the fascinating world of wave physics!**