# Wave Simulator - Planned Features

## Implementation Status

### ✅ Completed Features
- Modern UI with color-coded buttons
- Multiple visualization modes (Rainbow, Grayscale, Blue-Red, Cyan-Yellow)
- Interactive tools (Add/Remove sources, Draw walls, Interact mode)
- Screenshot functionality
- Preset configurations
- Grid overlay
- Contrast adjustment
- Time scale control
- Source management panel

### 🚧 In Progress

#### Phase 1: Core Enhancements (High Priority)
- [ ] **Movable Sources** - Drag and drop wave sources to reposition them
  - Add MOVE_SOURCE tool mode
  - Mouse drag detection for sources
  - Visual feedback during drag
  
- [ ] **Wall Reflectivity** - Adjustable wall absorption/reflection
  - Add wallReflectivity parameter (0.0-1.0)
  - Modify wave solver to apply reflectivity at boundaries
  - UI slider for adjustment

- [ ] **More Presets** - Additional demo configurations
  - Tsunami simulation
  - Standing waves
  - Doppler effect demo
  - Lens/focusing effect
  - Circular cavity resonance

- [ ] **FPS Limiter** - Power saving and consistent performance
  - Add target FPS setting
  - Frame time regulation
  - VSync toggle option

#### Phase 2: Visual Enhancements (Medium Priority)
- [ ] **3D Height Map View** - Toggle between 2D and 3D visualization
  - Implement perspective camera
  - Height-based vertex displacement
  - Adjustable view angle

- [ ] **Glow Effects** - Bloom/glow for high-amplitude regions
  - Two-pass rendering (blur + combine)
  - Adjustable glow intensity
  - Threshold control

- [ ] **Wave Trails** - Particle effects following wave peaks
  - Particle system for peak tracking
  - Trail fade-out
  - Color based on wave properties

#### Phase 3: Physics Extensions (Medium Priority)
- [ ] **Dispersion** - Frequency-dependent wave speed
  - Implement dispersive wave equation
  - Wavelength calculation
  - Visual demonstration mode

- [ ] **Non-linear Waves** - Amplitude-dependent propagation
  - Modify solver for non-linear terms
  - Soliton formation capability
  - Breaking wave effects

- [ ] **Viscosity** - Frequency-dependent damping
  - Add frequency analysis
  - Differential damping per frequency
  - Physical realism improvement

#### Phase 4: Advanced Tools (Low Priority)
- [ ] **Undo/Redo System**
  - Command pattern implementation
  - History stack (limited size)
  - UI buttons for undo/redo

- [ ] **Copy/Paste Walls** - Pattern duplication
  - Selection rectangle
  - Clipboard for wall patterns
  - Paste with offset

- [ ] **Grid Snapping** - Align sources to grid
  - Toggle grid snap mode
  - Configurable snap distance
  - Visual snap indicators

- [ ] **Measure Tool** - Distance and wavelength measurement
  - Two-point measurement mode
  - Wavelength detection
  - Display in physics units

#### Phase 5: Educational Features (Low Priority)
- [ ] **Wave Properties Display**
  - Calculate and display wavelength
  - Show period and frequency
  - Compute wave speed from data

- [ ] **Interference Pattern Analyzer**
  - Detect and highlight constructive interference
  - Mark destructive interference zones
  - Pattern strength visualization

- [ ] **Phase Visualization**
  - Color-code by wave phase
  - Phase shift indicators
  - Standing wave node detection

#### Phase 6: Performance Optimizations (As Needed)
- [ ] **Multithreading** - Parallel wave computation
  - Split grid into tiles
  - Thread pool for computation
  - Lock-free data structures

- [ ] **SIMD Optimization** - Vectorized math operations
  - AVX/SSE instructions for wave solver
  - Aligned memory allocation
  - Batch processing

- [ ] **Spatial Partitioning** - Efficient wall collision
  - Grid-based wall lookup
  - Reduced boundary checks
  - Faster rendering

### 📋 Backlog
- Recording and playback system
- Wave data export (CSV/JSON)
- Custom configuration save/load
- Wave velocity vector field visualization
- Animation export (GIF/video)
- Touch/multi-touch support
- Custom color schemes
- Preset randomization
- Advanced wall properties (partial transmission)

## Technical Notes

### Movable Sources Implementation
```cpp
// Add to mouse callback
if (g_sim.currentTool == Tool::MOVE_SOURCE) {
    // Check if mouse is over a source
    for (size_t i = 0; i < g_sim.sources.size(); i++) {
        float dx = mouseX - g_sim.sources[i].x;
        float dy = mouseY - g_sim.sources[i].y;
        float dist = sqrt(dx*dx + dy*dy);
        if (dist < SOURCE_RADIUS) {
            g_sim.draggedSourceIndex = i;
            break;
        }
    }
}
```

### Wall Reflectivity Implementation
```cpp
// Modify wave solver boundary conditions
if (walls[idx]) {
    // Apply reflectivity: mix between current value and reflection
    float reflected = -u_prev[idx] * g_sim.wallReflectivity;
    u_next[idx] = reflected + u[idx] * (1.0f - g_sim.wallReflectivity);
}
```

### 3D Height Map View
```cpp
// Add vertex shader with height displacement
"attribute vec3 position;"
"attribute float height;"
"uniform mat4 projection;"
"uniform mat4 view;"
"void main() {"
"    vec3 pos = position;"
"    pos.z = height * heightScale;"
"    gl_Position = projection * view * vec4(pos, 1.0);"
"}"
```

## Priority Ranking

1. **Must Have** (Implement First)
   - Movable sources
   - Wall reflectivity
   - More presets
   - FPS limiter

2. **Should Have** (Implement Next)
   - 3D visualization
   - Glow effects
   - Dispersion

3. **Nice to Have** (If Time Permits)
   - Undo/redo
   - Advanced educational features
   - Performance optimizations

4. **Future Considerations**
   - Recording system
   - Advanced export features
   - Mobile support
