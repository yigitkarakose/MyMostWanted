cmake_minimum_required(VERSION 3.10)
project(AssimpModelLoading)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(assimp REQUIRED)

# FetchContent for GLAD
include(FetchContent)
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG v0.1.34
)
FetchContent_MakeAvailable(glad)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${glad_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    glfw
    glad
    OpenGL::GL
    assimp::assimp
)
