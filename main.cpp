#include <iostream>
#include <vector>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Vertex structure
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
};
static GLFWwindow* gWindow = nullptr;


// Simple Mesh class
class Mesh {
public:
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    unsigned int              VAO;

    Mesh(const std::vector<Vertex>& verts, const std::vector<unsigned int>& inds)
        : vertices(verts), indices(inds) {
        setupMesh();
    }

    void Draw() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES,
            static_cast<GLsizei>(indices.size()),
            GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

private:
    unsigned int VBO, EBO;
    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Vertex),
            vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(), GL_STATIC_DRAW);

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, Position)));
        // Normal attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, Normal)));

        glBindVertexArray(0);
    }
};

// Model loader using 
class Model {
public:
    std::vector<Mesh> meshes;

    Model(const std::string& path) {
        loadModel(path);
    }

    void Draw() const {
        for (const auto& mesh : meshes)
            mesh.Draw();
    }

private:
    void loadModel(const std::string& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace);
        if (!scene ||
            scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
            !scene->mRootNode) {
            std::cerr << "ERROR::ASSIMP::"
                << importer.GetErrorString()
                << std::endl;
            return;
        }
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        // Process all meshes in node
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* ai_mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(ai_mesh));
        }
        // Recursively process children
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            processNode(node->mChildren[i], scene);
    }

    Mesh processMesh(aiMesh* mesh) {
        std::vector<Vertex> verts;
        std::vector<unsigned int> inds;

        // Vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v;
            v.Position = {
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            };
            if (mesh->HasNormals())
                v.Normal = {
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z
            };
            else
                v.Normal = glm::vec3(0.0f);
            verts.push_back(v);
        }
        // Faces (indices)
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
                inds.push_back(face.mIndices[j]);
        }
        return Mesh(verts, inds);
    }
};

struct SceneObject {
    Model* model;      // işaretçi, Draw() çağrısı için
    glm::vec3   position;   // dünya uzayında konum
    glm::vec3   rotation;   // Euler açıları (x,y,z) derece cinsinden
    glm::vec3   scale;      // x,y,z ölçek
	glm::vec3   color; // Renk (isteğe bağlı, varsayılan beyaz)
   

    glm::mat4 getModelMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        m = glm::scale(m, scale);
        return m;
    }
};
static SceneObject carObj;
static SceneObject policeObj;
static SceneObject trainObj;

// ----------------- Chase State Machine -----------------
enum class ChaseState {
    IdleAtStart,
    WaitAtRed,
    RedDecision,
    ChaseBegin,
    TurnLeftAtJunction,
    ChoicePoint,
    BranchLeft,
    BranchStraight,
    FinalStraight,    // tren ve araç düz gidiyor
    FinalCarTurn,     // araç sola son dönüşü yapıyor
    Finished
};

ChaseState chaseState = ChaseState::IdleAtStart;
bool goLeft = false;
float chaseTimer = 0.0f;
const float chaseDuration = 5.0f;    // her segmentin süresi
const float fixedY = 1.5f;           // araba Y’si sabit
// XZ boyunca lineer karışım, Y hep fixedY
glm::vec3 lerpXZ(const glm::vec3& A,
    const glm::vec3& B,
    float t)
{
    return glm::vec3(
        glm::mix(A.x, B.x, t),
        fixedY,
        glm::mix(A.z, B.z, t)
    );
}

// Waypoint’ler (XZ hareket, Y=fixedY)
glm::vec3 P_start = { 100.201f, fixedY,  -48.5576f };
glm::vec3 P_fullLeft = { 181.093f, fixedY, -128.3000f };
glm::vec3 P_redLight = { 110.891f, fixedY, -227.8700f };
glm::vec3 P_chase0 = { -6.7516f, fixedY, -330.1660f };
glm::vec3 P_junction = { -92.7421f, fixedY, -249.6840f };
glm::vec3 P_barricade = { -36.4241f, fixedY, -179.7300f };
glm::vec3 P_train = { -177.2250f,fixedY, -161.1990f };
// trenin ileri hareketi için
enum class CameraMode {
    Free,       // hâlihazırdaki serbest kamera (WASDQE)
    Overhead,   // üstten
    FrontPOV    // ön POV
};

static CameraMode camMode = CameraMode::Free;


// Camera & input globals
float lastX = 800.0f, lastY = 450.0f;
bool firstMouse = true;
float yaw = -90.0f, pitch = 0.0f;
float fov = 45.0f;

glm::vec3 cameraPos = { 0.0f, 1.5f, 5.0f };
glm::vec3 cameraFront = { 0.0f, 0.0f, -1.0f };
glm::vec3 cameraUp = { 0.0f, 1.0f,  0.0f };
extern glm::vec3 cameraPos;

double deltaTime = 0.0, lastFrame = 0.0;
static glm::vec3 prevCarPos = P_start;            // arabanın bir önceki konumu
static glm::vec3 prevDir = glm::vec3(0.0f, 0.0f, 1.0f);  // önceki yön (Z+ başlangıç)
const float policeYOffset = 2.0f;          // polis aracı Y ekseninde +2 yukarıda
const float fixedYPol = fixedY + policeYOffset;  // sabitY (1.5) + 2 = 3.5
static bool   choiceActive = false;
static double choiceStartTime = 0.0;
const  double choiceTimeWindow = 5.0;
glm::vec3 P_trainStart = { -244.204f, fixedYPol, -174.878f };
glm::vec3 P_trainEnd = { -166.147f, fixedYPol, -125.873f };

// arabanın son sola dönüşü için
glm::vec3 P_carTurnStart = P_trainEnd;  // tam tren bittiği yerde başlasın
glm::vec3 P_carTurnEnd = { -113.102f, fixedY,  -71.7312f };

// Space tuşuna basıldığında çağrılacak
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        std::cout
            << "Camera Position: ("
            << cameraPos.x << ", "
            << cameraPos.y << ", "
            << cameraPos.z << ")"
            << std::endl;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos; lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    yaw += xoffset;
    pitch += yoffset;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}

void processInput(GLFWwindow* window) {
    // C tuşuna basınca mod değiştir
    static bool cPressedLast = false;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        
        if (!cPressedLast) {
            // döngüsel geçiş
            camMode = CameraMode((int(camMode) + 1) % 3);
            cPressedLast = true;
        }
    }
    else {
        cPressedLast = false;
    }
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    float speed = 2000.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos += speed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos -= speed * cameraUp;
}

// Her frame chase logic’i
void updateChase(GLFWwindow* window, float dt) {
    // 1) Sayaç
    chaseTimer = glm::clamp(chaseTimer + dt / chaseDuration, 0.0f, 1.0f);

    // 2) carTime / policeTime
    float carTime = chaseTimer;
    float absTime = carTime * chaseDuration;
    float policeTime = glm::clamp((absTime - /*delay*/1.0f) / chaseDuration, 0.0f, 1.0f);

    // 3) State‐machine
    switch (chaseState) {
    case ChaseState::IdleAtStart: {
        // araba
        carObj.position = lerpXZ(P_start, P_fullLeft, carTime);
        // polis (XZ lerp, Y = fixedYPol)
        {
            auto p = lerpXZ(P_start, P_fullLeft, policeTime);
            policeObj.position = { p.x, fixedYPol, p.z };
        }
        if (carTime >= 1.0f) {
            chaseState = ChaseState::WaitAtRed;
            chaseTimer = 0.0f;
        }
        break;
    }
    case ChaseState::WaitAtRed: {
        carObj.position = lerpXZ(P_fullLeft, P_redLight, carTime);
        auto p = lerpXZ(P_fullLeft, P_redLight, policeTime);
        policeObj.position = { p.x, fixedYPol, p.z };
        if (carTime >= 1.0f) {
            chaseState = ChaseState::RedDecision;
            chaseTimer = 0.0f;
        }
        break;
    }
    case ChaseState::RedDecision: {
        // kesin pozisyonu kilitle
        carObj.position = P_redLight;
        policeObj.position = { P_redLight.x, fixedYPol, P_redLight.z };
        // Space ile geçiş
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            chaseState = ChaseState::ChaseBegin;
            chaseTimer = 0.0f;
        }
        break;
    }
    case ChaseState::ChaseBegin: {
        carObj.position = lerpXZ(P_redLight, P_chase0, carTime);
        auto p = lerpXZ(P_redLight, P_chase0, policeTime);
        policeObj.position = { p.x, fixedYPol, p.z };
        if (carTime >= 1.0f) {
            chaseState = ChaseState::TurnLeftAtJunction;
            chaseTimer = 0.0f;
        }
        break;
    }
    case ChaseState::TurnLeftAtJunction: {
        carObj.position = lerpXZ(P_chase0, P_junction, carTime);
        auto p = lerpXZ(P_chase0, P_junction, policeTime);
        policeObj.position = { p.x, fixedYPol, p.z };
        if (carTime >= 1.0f) {
            chaseState = ChaseState::ChoicePoint;
            chaseTimer = 0.0f;
        }
        break;
    }
    case ChaseState::ChoicePoint: {
        // İlk defa giriyorsak seçim zamanını başlat
        if (!choiceActive) {
            choiceActive = true;
            choiceStartTime = glfwGetTime();
        }

        // Arabayı ve polisi köşede sabitle
        carObj.position = P_junction;
        policeObj.position = { P_junction.x - 10.0f, fixedYPol, P_junction.z - 10.0f };

        // Kullanıcı LEFT/RIGHT ile seçecek
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            goLeft = true;
            chaseState = ChaseState::BranchLeft;
            chaseTimer = 0.0f;
            choiceActive = false;             // seçim tamamlandı
        }
        else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            goLeft = false;
            chaseState = ChaseState::BranchStraight;
            chaseTimer = 0.0f;
            choiceActive = false;
        }
        else {
            // zaman aşımı kontrolü
            double now = glfwGetTime();
            if (now - choiceStartTime >= choiceTimeWindow) {
                // Süre doldu → varsayılan branch (örneğin düz devam)
                goLeft = false;
                chaseState = ChaseState::BranchStraight;
                chaseTimer = 0.0f;
                choiceActive = false;
            }
        }
        break;
    }
    case ChaseState::BranchLeft: {
        carObj.position = lerpXZ(P_junction, P_barricade, carTime);
        auto p = lerpXZ(P_junction, P_barricade, policeTime);
        policeObj.position = { p.x, fixedYPol, p.z };
        if (carTime >= 1.0f) chaseState = ChaseState::Finished;
        break;
    }
    case ChaseState::BranchStraight: {
        // 1) araba branchStraight
        carObj.position = lerpXZ(P_junction, P_trainStart, carTime)
            + (P_trainEnd - P_trainStart) * carTime;
        // veya doğrudan:
        // carObj.position = lerpXZ(P_junction, P_train, carTime);

        // 2) polis yine gecikmeli XZ, Y=fixedYPol
        {
            glm::vec3 p = lerpXZ(P_junction, P_trainStart, policeTime)
                + (P_trainEnd - P_trainStart) * policeTime;
            policeObj.position = { p.x, fixedYPol, p.z };
        }

        // 3) tren objesini hareket ettir
        {
            glm::vec3 tpos = glm::mix(P_trainStart, P_trainEnd, carTime);
            trainObj.position = tpos;
        }

        if (carTime >= 1.0f) {
            chaseState = ChaseState::FinalCarTurn;
            chaseTimer = 0.0f;
        }
        break;
    }

    case ChaseState::FinalCarTurn: {
        // araba son sola dönüşü: P_carTurnStart -> P_carTurnEnd
        carObj.position = glm::mix(P_carTurnStart, P_carTurnEnd, carTime);
        {
            glm::vec3 tpos = glm::mix(P_trainStart, P_trainEnd, carTime);
            trainObj.position = tpos;
        }

        if (carTime >= 1.0f) {
            chaseState = ChaseState::Finished;
        }
        break;
    }
    
    case ChaseState::Finished:
        // buradan reset/loop eklenebilir
        break;
    }

    // 4) Bank (roll) + yaw hesapla
    glm::vec3 velocity = carObj.position - prevCarPos;
    glm::vec3 dir = glm::normalize(velocity);
    float     yawAng = glm::degrees(atan2(dir.x, dir.z));
    float     turnAngle = glm::degrees(acos(glm::clamp(glm::dot(prevDir, dir), -1.0f, 1.0f)));
    float     bank = glm::clamp(turnAngle * 2.0f, -15.0f, 15.0f);

    prevCarPos = carObj.position;
    prevDir = dir;

    carObj.rotation = { 0.0f, yawAng, bank };
    policeObj.rotation = { 0.0f, yawAng, 0.0f };
}

// Shader utilities
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(id, 512, nullptr, info);
        std::cerr << "ERROR::SHADER_COMPILATION_FAILED\n" << info << std::endl;
    }
    return id;
}

unsigned int createShaderProgram(const char* vertSrc, const char* fragSrc) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        std::cerr << "ERROR::PROGRAM_LINKING_FAILED\n" << info << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Shaders
const char* vertexShaderSource = R"GLSL(
#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos,1.0));
    Normal  = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos,1.0);
}
)GLSL";

const char* fragmentShaderSource = R"GLSL(
#version 430 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;


void main() {
    vec3 ambient = 0.2 * lightColor;
    vec3 norm    = normalize(Normal);
    vec3 lightDir= normalize(lightPos - FragPos);
    float diff   = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir= reflect(-lightDir, norm);
    float spec   = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular= 0.5 * spec * lightColor;
    vec3 result  = (ambient + diffuse + specular) * objectColor;
    FragColor    = vec4(result,1.0);
}
)GLSL";

// GLFW callbacks
void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
    glViewport(0, 0, w, h);
}

void updateChase(GLFWwindow* window, float dt);
int main() {
    // Init GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 900,
        "MyMostWanter",
        nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    gWindow = window;


    // Compile & link shaders
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Load model
   // 1) Birden fazla Model örneği
    Model carModel("models/Datsun_280Z.obj");
    Model traficlightModel("models/trafficlight.obj");
	Model cityModel("models/city.obj");
    Model barricadeModel("models/Concrete_Barricade.obj");
    Model trainModel("models/electrictrain.obj");
    Model mondeoModel("models/Mondeo_NYPD.obj");
    Model policecarModel("models/policecar.obj");
    carObj = { &carModel,     P_start,    glm::vec3(0.0f), glm::vec3(3.0f), glm::vec3(0.8f,0.7f,0.0f) };
    policeObj = { &policecarModel,{113.545f,fixedY+2.0f,-257.034f}, glm::vec3(0.0f), glm::vec3(5.0f), glm::vec3(0.0f,0.0f,0.5f) };
    trainObj = { &trainModel,
              P_trainStart,           // yukarıda tanımladığın waypoint
              glm::vec3(0.0f),        // rotation = 0
              glm::vec3(1.3f),        // modeline uygun scale
              glm::vec3(0.6f,0.3f,0.1f) // kahverengi tonu
    };
    int uModelLoc = glGetUniformLocation(shaderProgram, "model");
    int uColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    // 2) Sahne objelerini tutacak vektör
    std::vector<SceneObject> scene;

    // 3) Örnek objeler ekle
   // Şehir (beyaz-gri)
    scene.push_back({ &cityModel,
        glm::vec3(0.0f,0.0f,0.0f),
        glm::vec3(0.0f),
        glm::vec3(0.008f),
        glm::vec3(0.9f,0.9f,0.9f)
        });


    // Kaçan araba (koyu sarı)
    //scene.push_back({ &carModel,
    //    glm::vec3(100.201f, 1.558f, -48.558f),  // başlangıç poz
    //   glm::vec3(-0.33f, 137.11f, 0.0f),
    //    glm::vec3(2.0f),
    //    glm::vec3(0.8f,0.7f,0.0f)
    //    });

    // Trafik lambası (gri)
    scene.push_back({ &traficlightModel,
        glm::vec3(101.805f,0.18172f,-218.785),
        glm::vec3(0.0f, 60.0f, 0.0f),
        glm::vec3(1.0f),
        glm::vec3(0.5f,0.5f,0.5f)
        });

    // Barikatlar (3 adet, koyu gri)
    scene.push_back({ &barricadeModel,
        glm::vec3(-42.8973f, 2.56963f, -186.734f),
        glm::vec3(0.0f), glm::vec3(1.655f), glm::vec3(0.2f)
        });
    scene.push_back({ &barricadeModel,
        glm::vec3(-47.2592f, 2.32627f, -182.464f),
        glm::vec3(0.0f), glm::vec3(1.655f), glm::vec3(0.2f)
        });
    scene.push_back({ &barricadeModel,
        glm::vec3(-37.2264f, 2.12073f, -189.842f),
        glm::vec3(0.0f), glm::vec3(1.655f), glm::vec3(0.2f)
        });

    // Tren (kahverengi)
    /*scene.push_back({ &trainModel,
        glm::vec3(-244.204f,1.31191,-174.878f),
        glm::vec3(0.0f,60.0f,0.0f),
        glm::vec3(1.3f),
        glm::vec3(0.6f,0.3f,0.1f)
        });*/

    // Mondeo (mavi) — barikat civarında 2 adet
    scene.push_back({ &mondeoModel,
        glm::vec3(-42.3781f, 1.87126f, -173.417f),
        glm::vec3(0.0f,0.0f,0.0f),
        glm::vec3(0.061f),
        glm::vec3(0.0f,0.0f,1.0f)
        });
    scene.push_back({ &mondeoModel,
        glm::vec3(-29.6428f, 2.08468f, -182.543f),
        glm::vec3(0.0f,270.0f,0.0f),
        glm::vec3(0.061f),
        glm::vec3(0.0f,0.0f,1.0f)
        });

    // Polis arabaları (koyu mavi) 
   /* glm::vec3 pStart(113.545f, 3.1282f, -257.034f);
    scene.push_back({ &policecarModel,
        pStart,
        glm::vec3(0.0f),
        glm::vec3(5.0f),
        glm::vec3(0.0f,0.0f,0.5f)
        });*/
    lastFrame = (float)glfwGetTime();
    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float current = (float)glfwGetTime();
        float dt = current - lastFrame;
        lastFrame = current;

        // 1) Event’leri hemen çek
        glfwPollEvents();

        // 2) Kamera / input kontrolü
        processInput(window);

        // 3) Chase mantığını güncelle
        updateChase(window, dt);

        // 4) Temizle ve shader’ı seç
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        // 5) Kamera matrislerini set et
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glm::mat4 proj = glm::perspective(glm::radians(fov), (float)w / h, 0.1f, 1000.0f);
        // view hesaplama:
        glm::mat4 view;
        switch (camMode) {
        case CameraMode::Free:
            view = glm::lookAt(cameraPos,
                cameraPos + cameraFront,
                cameraUp);
            break;

        case CameraMode::Overhead: {
            // Arabanın ileri yönü (XZ düzleminde)
            glm::vec3 fw = glm::normalize(glm::vec3(
                sin(glm::radians(carObj.rotation.y)),
                0.0f,
                cos(glm::radians(carObj.rotation.y))
            ));
            glm::vec3 back = -fw;

            // 1) Göz noktası: arabanın 5 birim gerisinde, 10 birim yukarıda
            glm::vec3 eye = carObj.position + back * 10.0f + glm::vec3(0.0f, 5.0f, 0.0f);
            // 2) Bakış noktası: arabanın 10 birim ilerisini göster (yere değil, ileri doğru)
            glm::vec3 center = carObj.position + fw * 5.0f;

            view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));
            break;
        }

        case CameraMode::FrontPOV: {
            // Arabanın ileri yönü
            glm::vec3 fw = glm::normalize(glm::vec3(
                sin(glm::radians(carObj.rotation.y)),
                0.0f,
                cos(glm::radians(carObj.rotation.y))
            ));

            // 1) Göz noktası: arabanın 5 birim önünde, 2 birim yukarıda
            glm::vec3 eye = carObj.position + fw * 5.0f + glm::vec3(0.0f, 2.0f, 0.0f);
            // 2) Bakış noktası: aynı yönde biraz daha ileri
            glm::vec3 center = eye + fw * 10.0f;

            view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));
            break;
        }
        };

       
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
            1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),
            1, GL_FALSE, glm::value_ptr(view));

        // 6) Işık ve genel color (statik)
        glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 5.0f, 5.0f, 5.0f);
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);

        // 7) Dinamik chase objeler
        for (auto* dyn : { &carObj, &policeObj, &trainObj }) {
            glm::mat4 M = dyn->getModelMatrix();
            glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(M));
            glUniform3fv(uColorLoc, 1, glm::value_ptr(dyn->color));
            dyn->model->Draw();
        }

        // 8) Statik sahne objeleri (statik listeye araba/polis eklemeyin)
        for (const auto& obj : scene) {
            glm::mat4 M = obj.getModelMatrix();
            glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(M));
            glUniform3fv(uColorLoc, 1, glm::value_ptr(obj.color));
            obj.model->Draw();
        }

        // 9) Swap
        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}
