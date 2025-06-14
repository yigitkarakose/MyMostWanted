#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

// Cube vertex data
const float cubeVertices[] = {
    // Front face
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    // Back face
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
};

// Cube indices
const unsigned int cubeIndices[] = {
    0, 1, 2, 2, 3, 0, // Front
    1, 5, 6, 6, 2, 1, // Right
    5, 4, 7, 7, 6, 5, // Back
    4, 0, 3, 3, 7, 4, // Left
    3, 2, 6, 6, 7, 3, // Top
    4, 5, 1, 1, 0, 4  // Bottom
};

// Physics Cube struct
struct PhysicsCube {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float mass;
    float restitution;
};

// Shader source code
const char* vertexShaderSource = R"(
#version 430 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 430 core
out vec4 FragColor;
uniform vec3 color;
void main() {
    FragColor = vec4(color, 1.0);
}
)";

// Physics update function
void updatePhysics(PhysicsCube& cube1, PhysicsCube& cube2, float deltaTime) {
    // Update positions
    cube1.velocity += cube1.acceleration * deltaTime;
    cube2.velocity += cube2.acceleration * deltaTime;
    cube1.position += cube1.velocity * deltaTime;
    cube2.position += cube2.velocity * deltaTime;

    // Wall collision constants
    const float WALL_X = 2.5f;
    const float WALL_Y = 2.0f;
    const float WALL_Z = 2.5f;
    const float CUBE_HALF_SIZE = 0.5f;

    // Wall collisions for cube1
    if (std::abs(cube1.position.x) > WALL_X - CUBE_HALF_SIZE) {
        cube1.position.x = (WALL_X - CUBE_HALF_SIZE) * (cube1.position.x < 0 ? -1.0f : 1.0f);
        cube1.velocity.x = -cube1.velocity.x * cube1.restitution;
    }
    if (std::abs(cube1.position.y) > WALL_Y - CUBE_HALF_SIZE) {
        cube1.position.y = (WALL_Y - CUBE_HALF_SIZE) * (cube1.position.y < 0 ? -1.0f : 1.0f);
        cube1.velocity.y = -cube1.velocity.y * cube1.restitution;
    }
    if (std::abs(cube1.position.z) > WALL_Z - CUBE_HALF_SIZE) {
        cube1.position.z = (WALL_Z - CUBE_HALF_SIZE) * (cube1.position.z < 0 ? -1.0f : 1.0f);
        cube1.velocity.z = -cube1.velocity.z * cube1.restitution;
    }

    // Wall collisions for cube2
    if (std::abs(cube2.position.x) > WALL_X - CUBE_HALF_SIZE) {
        cube2.position.x = (WALL_X - CUBE_HALF_SIZE) * (cube2.position.x < 0 ? -1.0f : 1.0f);
        cube2.velocity.x = -cube2.velocity.x * cube2.restitution;
    }
    if (std::abs(cube2.position.y) > WALL_Y - CUBE_HALF_SIZE) {
        cube2.position.y = (WALL_Y - CUBE_HALF_SIZE) * (cube2.position.y < 0 ? -1.0f : 1.0f);
        cube2.velocity.y = -cube2.velocity.y * cube2.restitution;
    }
    if (std::abs(cube2.position.z) > WALL_Z - CUBE_HALF_SIZE) {
        cube2.position.z = (WALL_Z - CUBE_HALF_SIZE) * (cube2.position.z < 0 ? -1.0f : 1.0f);
        cube2.velocity.z = -cube2.velocity.z * cube2.restitution;
    }

    // Check for cube collision
    glm::vec3 diff = cube1.position - cube2.position;
    float distance = glm::length(diff);

    if (distance < 1.0f) { // 1.0f is the sum of cube half-widths
        // Collision response
        glm::vec3 normal = glm::normalize(diff);

        // Relative velocity
        glm::vec3 relativeVel = cube1.velocity - cube2.velocity;

        // Calculate impulse
        float impulseStrength = -(1.0f + cube1.restitution) *
            glm::dot(relativeVel, normal) /
            (1.0f / cube1.mass + 1.0f / cube2.mass);

        // Apply impulse
        cube1.velocity += (impulseStrength / cube1.mass) * normal;
        cube2.velocity -= (impulseStrength / cube2.mass) * normal;

        // Separate the cubes
        float overlap = 1.0f - distance;
        glm::vec3 separation = overlap * normal * 0.5f;
        cube1.position += separation;
        cube2.position -= separation;
    }
}

int main() {
    // Initialize GLFW and OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Physics Cubes", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();

    // Create and compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Create VAO and VBO
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Initialize physics cubes
    PhysicsCube cube1 = {
        glm::vec3(-1.0f, 0.0f, 0.0f),  // position
        glm::vec3(5.0f, 0.0f, 0.0f),   // velocity
        glm::vec3(0.0f, 0.0f, 0.0f),   // acceleration
        1.0f,                           // mass
        0.8f                            // restitution
    };

    PhysicsCube cube2 = {
        glm::vec3(1.0f, 0.0f, 0.0f),   // position
        glm::vec3(-5.0f, 0.0f, 0.0f),  // velocity
        glm::vec3(0.0f, 0.0f, 0.0f),   // acceleration
        1.0f,                           // mass
        0.8f                            // restitution
    };

    // Set up camera
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 2.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    float lastFrame = 0.0f;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Update physics
        updatePhysics(cube1, cube2, deltaTime);

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        // Set uniforms
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Draw cube1
        glm::mat4 model1 = glm::translate(glm::mat4(1.0f), cube1.position);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model1));
        glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 0.0f, 0.0f);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Draw cube2
        glm::mat4 model2 = glm::translate(glm::mat4(1.0f), cube2.position);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model2));
        glUniform3f(glGetUniformLocation(shaderProgram, "color"), 0.0f, 0.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}