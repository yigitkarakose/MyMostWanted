
cmake_minimum_required(VERSION 3.10)
project(PhysicsGL)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Bullet REQUIRED)

include_directories(${OPENGL_INCLUDE_DIRS} ${BULLET_INCLUDE_DIRS})
add_executable(PhysicsGL main.cpp glad.c)

target_link_libraries(PhysicsGL
    glfw
    ${OPENGL_gl_LIBRARY}
    BulletDynamics
    BulletCollision
    LinearMath
)
