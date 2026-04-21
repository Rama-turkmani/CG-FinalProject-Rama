# 3D Maze Collector — Final Project (Computer Graphics)

**Student:** Rama  
**Course:** Computer Graphics (بيانيات الحاسوب)  
**Tech:** C++20 · Modern OpenGL 3.3 Core · GLFW · GLEW · GLM · stb_image

---

## 🎮 Game Idea

A first-person 3D maze game.  
The player is dropped into a 10×10 textured maze and must **collect all five gold coins before the 90-second timer runs out**. If the timer reaches zero first, the game is lost. Walls block movement (real AABB collision), and the world is lit by a single directional light and wrapped in distance fog so far-away corridors fade into a blue-grey haze.

Mission is clearly defined:

| Condition | Result |
|-----------|--------|
| All 5 coins collected before timer hits 0 | **WIN** (green overlay) |
| Timer reaches 0 first | **LOSE** (red overlay) |

---

## 🎛 Controls

| Key / Input | Action |
|-------------|--------|
| `W` / `S`   | Walk forward / backward |
| `A` / `D`   | Strafe left / right |
| Mouse       | Look around (yaw + pitch, clamped to ±85°) |
| `R`         | Restart level |
| `ESC`       | Quit |

---

## 📁 Project Layout

```
Rama_Final/
├── Source/                  # C++ source code
│   ├── main.cpp
│   └── stb_image.h
├── Shaders/                 # GLSL shaders loaded at runtime
│   ├── world.vert / world.frag   (lighting + fog)
│   └── hud.vert   / hud.frag     (2D HUD overlay)
├── Assets/                  # Textures (relative paths only)
│   ├── container.jpg        # walls + floor
│   └── awesomeface.png      # collectible coins (alpha-tested)
├── ThirdParty/              # Vendored dependencies (committed to the repo)
│   ├── GLFW/                #   prebuilt GLFW 3.4 (lib-vc2022/glfw3.lib)
│   ├── GLEW/                #   prebuilt GLEW 2.1.0 (lib/Release/x64/glew32s.lib)
│   └── GLM/                 #   header-only GLM 1.0.1
├── project1/                # Visual Studio project (v145 toolset)
│   ├── project1.vcxproj     #   relative include/lib paths → ..\ThirdParty
│   ├── project1.vcxproj.filters
│   └── project1.vcxproj.user      # sets working dir = $(SolutionDir)
├── project1.slnx            # VS solution (slnx format)
├── .gitignore
└── README.md
```

All runtime file lookups go through a tiny helper that tries `./`, `../`, `../../`, `../../../` in turn, so the `.exe` works whether launched from `x64/Debug`, `project1/`, or the solution root.

---

## 🛠 Build Instructions (Visual Studio)

**No dependency setup is required — GLFW, GLEW and GLM are vendored inside `ThirdParty/`** and all include / lib paths in the `.vcxproj` are relative (`$(ProjectDir)..\ThirdParty\...`).

1. **Open** `project1.slnx` in Visual Studio (2022 or newer).

2. If VS complains about the platform toolset, right-click the solution → **Retarget Solution** → pick whatever toolset is installed (e.g. v143 for stock VS 2022, v145 for newer builds).

3. Select **Debug | x64** (or Release | x64) and press **F5**.  
   The `.vcxproj.user` sets the working directory to `$(SolutionDir)` so `Shaders/*.vert` and `Assets/*.jpg` resolve via relative paths from the solution root.

4. If you want to run the `.exe` standalone (without VS), copy the `Shaders/` and `Assets/` folders next to the `.exe` — or just launch it from the solution root; the path-search helper in `main.cpp` will find them either way.

### Vendored library versions

| Library | Version | Role |
|---------|---------|------|
| GLFW    | 3.4 (binary, vc2022) | Windowing + input + GL context |
| GLEW    | 2.1.0 (static, `glew32s.lib`) | Modern OpenGL function loader |
| GLM     | 1.0.1 (header-only) | Vectors / matrices / transforms |
| stb_image | single-header (in `Source/`) | PNG / JPG loading |

---

## 📝 Rubric Mapping (14 marks)

Each pair is **(A) implementation** + **(B) explanation**. Below is a quick guide to where each feature lives in the code so you can explain it during the viva.

### Pair 1 — Environment & main loop
- `main.cpp:` `glfwInit` + `glfwWindowHint(..., 3)` + `GLFW_OPENGL_CORE_PROFILE` → create a 3.3 Core context.
- `glewExperimental = GL_TRUE; glewInit();` loads the modern OpenGL function pointers. **GLEW and GLAD are two interchangeable OpenGL loaders** — both do the same job (resolve function pointers for the driver's GL implementation at runtime); the project uses GLEW because that is what the course template shipped with.
- The render loop is clearly split: **`processInput` → `updateGame` → draw → `glfwSwapBuffers`** with a `deltaTime` clamp, separating game logic from rendering.

### Pair 2 — Scene & geometry (VAO / VBO / EBO)
- Floor, walls, coins and the held lantern are all uploaded to the GPU once via `makeCubeVAO` / `makeFloorVAO` / `makeQuadVAO` / `makeHudQuadVAO` using **VAO + VBO** (interleaved `pos / uv / normal`, stride = 8 floats).
- **The floor specifically uses an EBO** (`GL_ELEMENT_ARRAY_BUFFER`): 4 unique corner vertices are indexed into 2 triangles via `glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0)`, demonstrating the VAO / VBO / **EBO** triad the rubric asks for. Walls and cubes use `glDrawArrays` because each face needs its own UVs / normals (24 vertex variants).
- Each object is placed in the world with its own **Model** matrix (`glm::translate`, `glm::scale`, `glm::rotate`), combined with a shared **View** and **Projection** matrix. All three are multiplied in the vertex shader: `projection * view * model * vec4(aPos, 1.0)`.

### Pair 3 — Camera system
- Perspective projection from `glm::perspective(radians(60°), aspect, 0.1, 80)`.
- Forward vector computed from `yaw`/`pitch` the classic way:
  ```
  front.x = cos(yaw) * cos(pitch)
  front.y = sin(pitch)
  front.z = sin(yaw) * cos(pitch)
  ```
- `glm::lookAt(eye, eye + front, up)` gives a smooth free-look camera with pitch clamped to ±85° to avoid gimbal flip.

### Pair 4 — Textures & visual effects
- `loadTexture()` wraps **stb_image**: set wrap `GL_REPEAT`, filter `GL_LINEAR_MIPMAP_LINEAR` + `glGenerateMipmap`, upload with the detected channel count.
- `world.frag` applies two effects:
  1. **Directional diffuse + ambient lighting** — dot product between the surface normal (transformed by the normal matrix in the vertex shader) and the light direction.
  2. **Exponential distance fog** — `f = 1 − exp(−density · viewDist)` mixing the final colour towards `uFogColor`.
- Coin billboards use an **alpha test** (`discard` when `alpha < 0.15`) so the transparent background of `awesomeface.png` is not drawn.

### Pair 5 — Character & controls
- **Visible 3D character model**: a small bronze cube (the "held lantern") floats in front-right of the camera, rotating slowly and bobbing vertically with a `sin(walkPhase)` curve only while the player is moving. Its model matrix is built from the camera vectors (`flatFront`, `rightV`) so it follows the player exactly as a third-person body would — this is the on-screen evidence of the 3D character.
- `processInput()` reads GLFW keys and builds a movement vector **relative to the player's yaw** (strafe uses `cross(forward, up)`), multiplied by `deltaTime * PLAYER_SPEED` so movement is frame-rate-independent.
- Mouse look is a separate callback that writes to `yaw` / `pitch` only — **input is decoupled from collision / physics** (`updateGame` advances game state, `processInput` only produces the desired move vector, and collision rejection is the final gate).

### Pair 6 — Collision & maze design
- The maze is a 10×10 char array (`'W' / '.' / 'C' / 'S'`) parsed once in `buildLevel()` into two lists: wall centres and coin positions.
- Collision test `collides(p)` does the standard **circle-vs-AABB**:
  ```
  c = clamp(p.xz, wallMin.xz, wallMax.xz)
  if (|p − c|² < r²)  →  collision
  ```
- Movement uses **axis-separated sliding**: first try to apply `move.x`, then `move.z`, reverting whichever axis causes a collision. This makes the player slide along walls instead of sticking.

### Pair 7 — Game logic & UI
- A `GameState` enum (`Playing / Won / Lost`) drives a tiny state machine in `updateGame()`.
- Pickup is a squared-distance check in the XZ plane (`< 0.8²`), avoiding an expensive `sqrt`.
- HUD is a separate shader program (`hud.vert/frag`) drawing 2D quads in orthographic space (`glm::ortho(0, w, 0, h)`): five coin-status squares top-left, a timer bar centred at the top, and a tinted full-screen overlay on win/lose. `GL_DEPTH_TEST` is disabled around the HUD pass and re-enabled before the next frame.

---

## 💡 Notes for the live demo

- **Charger**: bring the laptop charger (battery-saver throttles the GPU and tanks FPS).
- **Clean clone test**: before submitting, clone your own GitHub repo into a new folder and F5 from there — confirms the `Shaders/` and `Assets/` paths still resolve.
- **Backup video**: record a 1-minute capture of the game running, just in case the laptop misbehaves on the day.
