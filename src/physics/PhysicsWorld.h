#pragma once
#include <memory>
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void initPhysics();
    void stepSimulation(float deltaTime);
    void addGroundPlane();
    void addCube(const glm::vec3& position, const glm::vec3& scale = glm::vec3(1.0f));
    void getTransform(int index, float* matrix); // Get OpenGL matrix for rendering
    int getNumObjects() const { return m_rigidBodies.size(); }

private:
    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_overlappingPairCache;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_dynamicsWorld;
    
    std::vector<std::unique_ptr<btCollisionShape>> m_collisionShapes;
    std::vector<std::unique_ptr<btRigidBody>> m_rigidBodies;
    
    btCollisionShape* createBoxShape(const glm::vec3& scale);
};