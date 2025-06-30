# Scripted Car Chase Simulation in OpenGL

Video Link: https://youtu.be/WnE-WcrP03s
This project is a scripted 3D car chase simulation built with OpenGL 4.3, featuring real-time rendering, camera control, animation logic, and decision branches.

## Features

- **Dynamic Car Chase Logic**: A scripted scenario where a car runs a red light, gets pursued by the police, and is forced to choose between different escape routes.
- **Scene Components**:
  - Forest, ocean, and city terrain
  - Animated police car and escaping car
  - Traffic lights, barricades, and a train
- **Smooth Camera System**:
  - Free-fly mode
  - Overhead camera and POV camera with key toggles
- **Real-time Decision Points**:
  - Player decides direction (left/right) using arrow keys
  - Train animation triggers on correct escape path

## Controls

- `W/A/S/D`: Move camera
- `Q/E`: Move camera up/down
- `Mouse`: Look around
- `SPACE`: Confirm red light pass
- `←/→`: Choose direction at junction
- `1`: Free camera
- `2`: Overhead chase camera
- `3`: First-person POV camera

## Requirements

- OpenGL 4.3+
- GLFW
- GLM
- ASSIMP
- stb_image
- C++17 compatible compiler

## Build Instructions

```bash
mkdir build && cd build
cmake ..
make
./CarChaseSimulation
