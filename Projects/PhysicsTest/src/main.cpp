#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include "btBulletDynamicsCommon.h"
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

// ============================================
// Global Physics State (Bullet style)
// ============================================

enum class ShapeType { Sphere, Box };

struct PhysicsObject {
	btRigidBody* body;
	btCollisionShape* shape;
	btMotionState* motionState;
	ShapeType type;
	btVector3 halfExtents;
	int colorIndex;
};

// Camera
struct Camera {
	btVector3 position;
	btVector3 target;
	btVector3 up;
	float yaw;
	float pitch;
	float distance;
};

// Global variables (Bullet examples use global handles)
Camera g_camera = { btVector3(15, 10, 15), btVector3(0, 0, 0), btVector3(0, 1, 0), -45.0f, 35.0f, 20.0f };
bool g_mouseLeft = false;
bool g_mouseRight = false;
bool g_mouseMiddle = false;
double g_lastMouseX = 0;
double g_lastMouseY = 0;

// Physics objects
btDiscreteDynamicsWorld* g_dynamicsWorld = nullptr;
std::vector<std::unique_ptr<PhysicsObject>>* g_physicsObjects = nullptr;
std::vector<btRigidBody*> g_groundObjects;
btRigidBody* g_groundBody = nullptr;

// Mouse picking
PhysicsObject* g_selectedObject = nullptr;
btPoint2PointConstraint* g_mouseConstraint = nullptr;
btRigidBody* g_mousePickerBody = nullptr;

// Ground tilt parameters (Bullet style - using radians like Bullet)
btScalar g_groundTiltX = 0.0f;  // radians
btScalar g_groundTiltZ = 0.0f;  // radians

// UI parameters
bool g_enableGroundTilt = false;
btScalar g_sphereRadius = 1.0f;
btScalar g_sphereMass = 1.0f;
btScalar g_sphereHeight = 10.0f;
btVector3 g_boxHalfExtents(1.0f, 1.0f, 1.0f);
btScalar g_boxMass = 1.0f;
btScalar g_boxHeight = 10.0f;
int g_objIndex = 0;

// ============================================
// Lighting Parameters
// ============================================
// 主光 (Key Light) - 暖白光，从右上方照射，提供主要照明和高光
bool  g_light0Enabled = true;
float g_light0Pos[3]   = { 15.0f, 25.0f, 15.0f };
float g_light0Ambient  = 0.15f;
float g_light0Diffuse  = 0.85f;
float g_light0Specular = 0.9f;

// 补光 (Fill Light) - 冷色光，从左上方照射，柔化阴影
bool  g_light1Enabled = true;
float g_light1Pos[3]   = { -15.0f, 15.0f, -10.0f };
float g_light1Diffuse  = 0.35f;
float g_light1Specular = 0.15f;

// 背光 (Rim Light) - 从背后照射，勾勒物体轮廓
bool  g_light2Enabled = true;
float g_light2Pos[3]   = { 0.0f, 12.0f, -25.0f };
float g_light2Diffuse  = 0.25f;
float g_light2Specular = 0.2f;

// 全局环境光强度
float g_globalAmbient = 0.18f;

// 物体材质高光参数
float g_materialShininess = 64.0f;
float g_materialSpecular  = 0.6f;

// 是否绘制光源位置标记
bool g_showLightMarkers = true;

// Colors
btVector3 g_colors[] = {
	btVector3(1.0f, 0.3f, 0.3f),
	btVector3(0.3f, 1.0f, 0.3f),
	btVector3(0.3f, 0.3f, 1.0f),
	btVector3(1.0f, 1.0f, 0.3f),
	btVector3(1.0f, 0.3f, 1.0f),
	btVector3(0.3f, 1.0f, 1.0f),
};

// ============================================
// Camera Functions
// ============================================

void UpdateCameraPosition() {
	g_camera.position.setX(g_camera.distance * cos(g_camera.yaw * 0.0174533f) * cos(g_camera.pitch * 0.0174533f));
	g_camera.position.setY(g_camera.distance * sin(g_camera.pitch * 0.0174533f));
	g_camera.position.setZ(g_camera.distance * sin(g_camera.yaw * 0.0174533f) * cos(g_camera.pitch * 0.0174533f));
	g_camera.position += g_camera.target;
}

// ============================================
// Mouse Callbacks
// ============================================

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse) {
		g_mouseLeft = true;
		glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
	}
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		g_mouseLeft = false;
		if (g_mouseConstraint && g_dynamicsWorld) {
			g_dynamicsWorld->removeConstraint(g_mouseConstraint);
			delete g_mouseConstraint;
			g_mouseConstraint = nullptr;
			g_selectedObject = nullptr;
			if (g_mousePickerBody) {
				g_dynamicsWorld->removeRigidBody(g_mousePickerBody);
			}
		}
	}
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse) {
		g_mouseRight = true;
		glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
	}
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
		g_mouseRight = false;
	}
	if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse) {
		g_mouseMiddle = true;
		glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
	}
	if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE) {
		g_mouseMiddle = false;
	}
}

void MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	if (!ImGui::GetIO().WantCaptureMouse) {
		g_camera.distance -= (float)yoffset * 0.5f;
		g_camera.distance = fmaxf(2.0f, g_camera.distance);
		UpdateCameraPosition();
	}
}

// ============================================
// Ground Tilt Function (Bullet style)
// ============================================

void UpdateGroundTilt() {
	if (!g_groundBody || !g_dynamicsWorld) return;

	// Clamp to +/- 30 degrees
	const btScalar maxTilt = SIMD_PI / 6.0f;  // 30 degrees in radians
	g_groundTiltX = fmaxf(-maxTilt, fminf(maxTilt, g_groundTiltX));
	g_groundTiltZ = fmaxf(-maxTilt, fminf(maxTilt, g_groundTiltZ));

	// Create transform with tilt
	btTransform groundTrans;
	groundTrans.setIdentity();
	groundTrans.setOrigin(btVector3(0, -0.5f, 0));

	btQuaternion tiltX, tiltZ, totalTilt;
	tiltX.setRotation(btVector3(1, 0, 0), g_groundTiltX);
	tiltZ.setRotation(btVector3(0, 0, 1), g_groundTiltZ);
	totalTilt = tiltZ * tiltX;
	groundTrans.setRotation(totalTilt);

	// Update ground
	g_groundBody->setWorldTransform(groundTrans);
	g_groundBody->getMotionState()->setWorldTransform(groundTrans);

	// Recalculate collision broadphase proxy for ground
	btVector3 groundMin, groundMax;
	g_groundBody->getCollisionShape()->getAabb(groundTrans, groundMin, groundMax);
	g_dynamicsWorld->getBroadphase()->setAabb(g_groundBody->getBroadphaseHandle(), groundMin, groundMax, g_dynamicsWorld->getDispatcher());

	// Update walls - recalculate collision proxy for each wall
	float groundSize = 20.0f;
	float wallHeight = 3.0f;
	float wallThickness = 1.0f;

	btVector3 wallPositions[4] = {
		btVector3(0, wallHeight/2, -groundSize - wallThickness/2),
		btVector3(0, wallHeight/2, groundSize + wallThickness/2),
		btVector3(groundSize + wallThickness/2, wallHeight/2, 0),
		btVector3(-groundSize - wallThickness/2, wallHeight/2, 0)
	};

	for (size_t i = 0; i < 4 && (i + 1) < g_groundObjects.size(); i++) {
		btTransform wallTrans;
		wallTrans.setIdentity();

		btTransform tiltTransform;
		tiltTransform.setIdentity();
		tiltTransform.setRotation(totalTilt);
		btVector3 rotatedPos = tiltTransform(wallPositions[i]);
		wallTrans.setOrigin(rotatedPos);
		wallTrans.setRotation(totalTilt);

		g_groundObjects[i + 1]->setWorldTransform(wallTrans);
		g_groundObjects[i + 1]->getMotionState()->setWorldTransform(wallTrans);

		// Recalculate collision proxy for this wall
		btVector3 wallMin, wallMax;
		g_groundObjects[i + 1]->getCollisionShape()->getAabb(wallTrans, wallMin, wallMax);
		g_dynamicsWorld->getBroadphase()->setAabb(g_groundObjects[i + 1]->getBroadphaseHandle(), wallMin, wallMax, g_dynamicsWorld->getDispatcher());
	}

	// Activate all dynamic objects
	for (int i = 0; i < g_dynamicsWorld->getNumCollisionObjects(); i++) {
		btCollisionObject* obj = g_dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMass() > 0) {
			body->activate();
		}
	}
}

// ============================================
// Raycasting (Bullet style)
// ============================================

btVector3 GetRayDirectionFromMouse(GLFWwindow* window, double mouseX, double mouseY, int width, int height) {
	double x = (2.0 * mouseX) / width - 1.0;
	double y = 1.0 - (2.0 * mouseY) / height;

	GLdouble modelView[16], projection[16];
	GLint viewport[4] = { 0, 0, width, height };
	glGetDoublev(GL_MODELVIEW_MATRIX, modelView);
	glGetDoublev(GL_PROJECTION_MATRIX, projection);

	GLdouble nearX, nearY, nearZ;
	GLdouble farX, farY, farZ;
	gluUnProject(x, y, 0.0, modelView, projection, viewport, &nearX, &nearY, &nearZ);
	gluUnProject(x, y, 1.0, modelView, projection, viewport, &farX, &farY, &farZ);

	btVector3 rayFrom(nearX, nearY, nearZ);
	btVector3 rayTo(farX, farY, farZ);

	btVector3 rayDir = rayTo - rayFrom;
	rayDir.normalize();

	return rayDir;
}

PhysicsObject* Raycast(btDiscreteDynamicsWorld* world, const btVector3& rayFrom, const btVector3& rayDir, btVector3& hitPoint) {
	btCollisionWorld::ClosestRayResultCallback rayCallback(rayFrom, rayFrom + rayDir * 1000);
	world->rayTest(rayFrom, rayFrom + rayDir * 1000, rayCallback);

	if (rayCallback.hasHit()) {
		hitPoint = rayCallback.m_hitPointWorld;
		const btCollisionObject* obj = rayCallback.m_collisionObject;

		for (auto& phyObj : *g_physicsObjects) {
			if (phyObj->body == obj) {
				return phyObj.get();
			}
		}
	}
	return nullptr;
}

// ============================================
// Object Creation Functions (Bullet style)
// ============================================

void CreateSphere(const btVector3& pos, btScalar radius, btScalar mass) {
	btCollisionShape* shape = new btSphereShape(radius);
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(pos);
	btDefaultMotionState* motionState = new btDefaultMotionState(transform);

	btVector3 inertia(0, 0, 0);
	shape->calculateLocalInertia(mass, inertia);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	// Set physical properties
	body->setFriction(0.5f);
	body->setRestitution(0.3f);
	body->setRollingFriction(0.1f);
	body->setDamping(0.01f, 0.01f);
	body->forceActivationState(DISABLE_DEACTIVATION);

	g_dynamicsWorld->addRigidBody(body);

	auto obj = std::make_unique<PhysicsObject>();
	obj->body = body;
	obj->shape = shape;
	obj->motionState = motionState;
	obj->type = ShapeType::Sphere;
	obj->colorIndex = g_objIndex++ % 6;
	g_physicsObjects->push_back(std::move(obj));
}

void CreateBox(const btVector3& pos, const btVector3& halfExtents, btScalar mass) {
	btCollisionShape* shape = new btBoxShape(halfExtents);
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(pos);
	btDefaultMotionState* motionState = new btDefaultMotionState(transform);

	btVector3 inertia(0, 0, 0);
	shape->calculateLocalInertia(mass, inertia);

	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	// Set physical properties
	body->setFriction(0.7f);
	body->setRestitution(0.2f);
	body->setRollingFriction(0.1f);
	body->setDamping(0.01f, 0.01f);
	body->forceActivationState(DISABLE_DEACTIVATION);

	g_dynamicsWorld->addRigidBody(body);

	auto obj = std::make_unique<PhysicsObject>();
	obj->body = body;
	obj->shape = shape;
	obj->motionState = motionState;
	obj->type = ShapeType::Box;
	obj->halfExtents = halfExtents;
	obj->colorIndex = g_objIndex++ % 6;
	g_physicsObjects->push_back(std::move(obj));
}

// ============================================
// Rendering Functions
// ============================================

void DrawSphere(float radius, int segments = 16) {
	GLUquadric* quad = gluNewQuadric();
	gluSphere(quad, radius, segments, segments);
	gluDeleteQuadric(quad);
}

void DrawWireSphere(float radius, int segments = 12) {
	for (int i = 0; i < segments; i++) {
		float angle = (float)i / segments * 3.14159f * 2.0f;
		glBegin(GL_LINE_STRIP);
		for (int j = 0; j <= segments; j++) {
			float a = (float)j / segments * 3.14159f;
			glVertex3f(radius * sinf(a) * cosf(angle), radius * cosf(a), radius * sinf(a) * sinf(angle));
		}
		glEnd();
	}
	for (int i = 0; i < segments; i++) {
		float angle = (float)i / segments * 3.14159f * 2.0f;
		glBegin(GL_LINE_LOOP);
		for (int j = 0; j <= segments; j++) {
			float a = (float)j / segments * 3.14159f * 2.0f;
			glVertex3f(radius * cosf(a), radius * sinf(a) * sinf(angle), radius * sinf(a) * cosf(angle));
		}
		glEnd();
	}
}

void DrawBox(const btVector3& halfExtents) {
	glBegin(GL_QUADS);

	glNormal3f(0, 0, 1);
	glVertex3f(-halfExtents.x(), -halfExtents.y(), halfExtents.z());
	glVertex3f(halfExtents.x(), -halfExtents.y(), halfExtents.z());
	glVertex3f(halfExtents.x(), halfExtents.y(), halfExtents.z());
	glVertex3f(-halfExtents.x(), halfExtents.y(), halfExtents.z());

	glNormal3f(0, 0, -1);
	glVertex3f(-halfExtents.x(), -halfExtents.y(), -halfExtents.z());
	glVertex3f(-halfExtents.x(), halfExtents.y(), -halfExtents.z());
	glVertex3f(halfExtents.x(), halfExtents.y(), -halfExtents.z());
	glVertex3f(halfExtents.x(), -halfExtents.y(), -halfExtents.z());

	glNormal3f(0, 1, 0);
	glVertex3f(-halfExtents.x(), halfExtents.y(), -halfExtents.z());
	glVertex3f(-halfExtents.x(), halfExtents.y(), halfExtents.z());
	glVertex3f(halfExtents.x(), halfExtents.y(), halfExtents.z());
	glVertex3f(halfExtents.x(), halfExtents.y(), -halfExtents.z());

	glNormal3f(0, -1, 0);
	glVertex3f(-halfExtents.x(), -halfExtents.y(), -halfExtents.z());
	glVertex3f(halfExtents.x(), -halfExtents.y(), -halfExtents.z());
	glVertex3f(halfExtents.x(), -halfExtents.y(), halfExtents.z());
	glVertex3f(-halfExtents.x(), -halfExtents.y(), halfExtents.z());

	glNormal3f(1, 0, 0);
	glVertex3f(halfExtents.x(), -halfExtents.y(), -halfExtents.z());
	glVertex3f(halfExtents.x(), halfExtents.y(), -halfExtents.z());
	glVertex3f(halfExtents.x(), halfExtents.y(), halfExtents.z());
	glVertex3f(halfExtents.x(), -halfExtents.y(), halfExtents.z());

	glNormal3f(-1, 0, 0);
	glVertex3f(-halfExtents.x(), -halfExtents.y(), -halfExtents.z());
	glVertex3f(-halfExtents.x(), -halfExtents.y(), halfExtents.z());
	glVertex3f(-halfExtents.x(), halfExtents.y(), halfExtents.z());
	glVertex3f(-halfExtents.x(), halfExtents.y(), -halfExtents.z());

	glEnd();
}

void DrawGround(float size) {
	float groundY = 0.01f;

	// Draw only grid lines
	glBegin(GL_LINES);
	glColor3f(0.3f, 0.3f, 0.3f);
	for (float i = -size; i <= size; i += 1.0f) {
		glVertex3f(-size, groundY, i);
		glVertex3f(size, groundY, i);
		glVertex3f(i, groundY, -size);
		glVertex3f(i, groundY, size);
	}
	glEnd();
}

// ============================================
// Lighting Setup
// ============================================

// 配置场景中的所有光源、全局环境光和材质属性
// 注意：调用前应已设置好模型视图矩阵（相机变换），
// 因为 glLight(GL_POSITION) 会受到当前模型视图矩阵的影响。
void SetupSceneLights() {
	glEnable(GL_LIGHTING);
	glEnable(GL_NORMALIZE);  // 法线自动归一化（物体经过缩放时尤为重要）

	// 全局环境光 (GL_LIGHT_MODEL_AMBIENT) - 提升暗部可见性
	float globalAmbient[4] = {
		g_globalAmbient, g_globalAmbient, g_globalAmbient, 1.0f
	};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

	// 使用双面光照，让墙体内部等也能正确受光
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	// --- 主光 (Key Light) GL_LIGHT0 ---
	if (g_light0Enabled) {
		glEnable(GL_LIGHT0);
		float pos[4]     = { g_light0Pos[0], g_light0Pos[1], g_light0Pos[2], 1.0f };
		float ambient[4] = { g_light0Ambient,  g_light0Ambient,  g_light0Ambient,  1.0f };
		// 暖白色 (略带黄色) 主光
		float diffuse[4] = { g_light0Diffuse,   g_light0Diffuse * 0.97f, g_light0Diffuse * 0.9f, 1.0f };
		float specular[4]= { g_light0Specular,  g_light0Specular, g_light0Specular, 1.0f };
		glLightfv(GL_LIGHT0, GL_POSITION, pos);
		glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
		glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	} else {
		glDisable(GL_LIGHT0);
	}

	// --- 补光 (Fill Light) GL_LIGHT1 ---
	if (g_light1Enabled) {
		glEnable(GL_LIGHT1);
		float pos[4]      = { g_light1Pos[0], g_light1Pos[1], g_light1Pos[2], 1.0f };
		// 冷蓝色补光，模拟天光散射
		float diffuse[4]  = { g_light1Diffuse * 0.75f, g_light1Diffuse * 0.82f, g_light1Diffuse, 1.0f };
		float specular[4] = { g_light1Specular, g_light1Specular, g_light1Specular, 1.0f };
		glLightfv(GL_LIGHT1, GL_POSITION, pos);
		glLightfv(GL_LIGHT1, GL_DIFFUSE,  diffuse);
		glLightfv(GL_LIGHT1, GL_SPECULAR, specular);
		// 补光不投射环境光
		float zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		glLightfv(GL_LIGHT1, GL_AMBIENT, zero);
	} else {
		glDisable(GL_LIGHT1);
	}

	// --- 背光 (Rim Light) GL_LIGHT2 ---
	if (g_light2Enabled) {
		glEnable(GL_LIGHT2);
		float pos[4]      = { g_light2Pos[0], g_light2Pos[1], g_light2Pos[2], 1.0f };
		// 淡紫白色背光，增强物体轮廓
		float diffuse[4]  = { g_light2Diffuse * 0.95f, g_light2Diffuse * 0.95f, g_light2Diffuse, 1.0f };
		float specular[4] = { g_light2Specular, g_light2Specular, g_light2Specular, 1.0f };
		glLightfv(GL_LIGHT2, GL_POSITION, pos);
		glLightfv(GL_LIGHT2, GL_DIFFUSE,  diffuse);
		glLightfv(GL_LIGHT2, GL_SPECULAR, specular);
		float zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		glLightfv(GL_LIGHT2, GL_AMBIENT, zero);
	} else {
		glDisable(GL_LIGHT2);
	}

	// --- 材质设置 ---
	// 启用颜色材质，让 glColor 同时控制环境/漫反射颜色，
	// 同时单独指定高光（GL_SPECULAR）和光泽度（GL_SHININESS）。
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	float matSpecular[4] = {
		g_materialSpecular, g_materialSpecular, g_materialSpecular, 1.0f
	};
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, g_materialShininess);
}

// 绘制光源位置的小球标记（在禁用光照状态下绘制）
void DrawLightMarkers() {
	if (!g_showLightMarkers) return;

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	// 主光标记 - 黄色
	if (g_light0Enabled) {
		glPushMatrix();
		glTranslatef(g_light0Pos[0], g_light0Pos[1], g_light0Pos[2]);
		glColor3f(1.0f, 1.0f, 0.4f);
		DrawSphere(0.5f, 12);
		glPopMatrix();
	}
	// 补光标记 - 浅蓝
	if (g_light1Enabled) {
		glPushMatrix();
		glTranslatef(g_light1Pos[0], g_light1Pos[1], g_light1Pos[2]);
		glColor3f(0.4f, 0.6f, 1.0f);
		DrawSphere(0.4f, 12);
		glPopMatrix();
	}
	// 背光标记 - 浅紫
	if (g_light2Enabled) {
		glPushMatrix();
		glTranslatef(g_light2Pos[0], g_light2Pos[1], g_light2Pos[2]);
		glColor3f(0.9f, 0.5f, 1.0f);
		DrawSphere(0.4f, 12);
		glPopMatrix();
	}

	glEnable(GL_DEPTH_TEST);
}

// ============================================
// Main
// ============================================

int main()
{
	// Initialize Bullet Physics World (Bullet style)
	btBroadphaseInterface* broadphase = new btDbvtBroadphase();
	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();

	g_dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
	g_dynamicsWorld->setGravity(btVector3(0, -10, 0));

	g_physicsObjects = new std::vector<std::unique_ptr<PhysicsObject>>();
	g_dynamicsWorld->setWorldUserInfo(g_physicsObjects);

	// Setup Axis (Bullet style: Y is up)
	const int upAxis = 1;

	// Create Ground (Bullet style using btBoxShape)
	float groundSize = 20.0f;
	btCollisionShape* groundShape = new btBoxShape(btVector3(groundSize, 0.5f, groundSize));

	btTransform groundTrans;
	groundTrans.setIdentity();
	groundTrans.setOrigin(btVector3(0, -0.5f, 0));

	btRigidBody::btRigidBodyConstructionInfo groundCI(0, new btDefaultMotionState(groundTrans), groundShape, btVector3(0,0,0));
	g_groundBody = new btRigidBody(groundCI);
	g_groundBody->setFriction(1.0f);
	g_groundBody->setRestitution(0.1f);
	g_dynamicsWorld->addRigidBody(g_groundBody);
	g_groundObjects.push_back(g_groundBody);

	// Create Walls
	float wallHeight = 3.0f;
	float wallThickness = 1.0f;

	btVector3 wallPositions[4] = {
		btVector3(0, wallHeight/2, -groundSize - wallThickness/2),
		btVector3(0, wallHeight/2, groundSize + wallThickness/2),
		btVector3(groundSize + wallThickness/2, wallHeight/2, 0),
		btVector3(-groundSize - wallThickness/2, wallHeight/2, 0)
	};

	btVector3 wallHalfExtents[4] = {
		btVector3(groundSize + wallThickness*2, wallHeight/2, wallThickness),
		btVector3(groundSize + wallThickness*2, wallHeight/2, wallThickness),
		btVector3(wallThickness, wallHeight/2, groundSize),
		btVector3(wallThickness, wallHeight/2, groundSize)
	};

	for (int i = 0; i < 4; i++) {
		btCollisionShape* wallShape = new btBoxShape(wallHalfExtents[i]);
		btTransform wallTrans;
		wallTrans.setIdentity();
		wallTrans.setOrigin(wallPositions[i]);

		btRigidBody::btRigidBodyConstructionInfo wallCI(0, new btDefaultMotionState(wallTrans), wallShape, btVector3(0,0,0));
		btRigidBody* wallBody = new btRigidBody(wallCI);
		wallBody->setFriction(0.8f);
		wallBody->setRestitution(0.1f);
		// Make walls kinematic so they can be moved programmatically
		wallBody->setCollisionFlags(wallBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		g_dynamicsWorld->addRigidBody(wallBody);
		g_groundObjects.push_back(wallBody);
	}

	// Create Mouse Picker Body
	btCollisionShape* pickerShape = new btSphereShape(0.1f);
	btTransform pickerTrans;
	pickerTrans.setIdentity();
	btRigidBody::btRigidBodyConstructionInfo pickerCI(0, new btDefaultMotionState(pickerTrans), pickerShape, btVector3(0,0,0));
	g_mousePickerBody = new btRigidBody(pickerCI);
	g_mousePickerBody->setCollisionFlags(g_mousePickerBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	g_mousePickerBody->forceActivationState(DISABLE_DEACTIVATION);

	// Initialize GLFW
	if (!glfwInit()) {
		std::cerr << "Failed to init GLFW" << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_SAMPLES, 4);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "PhysicsTest - Bullet Style", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetScrollCallback(window, MouseScrollCallback);

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	UpdateCameraPosition();

	bool paused = false;
	bool step = false;
	bool showDemo = false;

	// Main Loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Handle Mouse Input
		double mouseX, mouseY;
		glfwGetCursorPos(window, &mouseX, &mouseY);

		// Camera rotation (right mouse)
		if (g_mouseRight) {
			double dx = mouseX - g_lastMouseX;
			double dy = mouseY - g_lastMouseY;
			g_camera.yaw += (float)dx * 0.5f;
			g_camera.pitch += (float)dy * 0.5f;
			g_camera.pitch = fmaxf(-89.0f, fminf(89.0f, g_camera.pitch));
			UpdateCameraPosition();
		}

		// Ground tilt (middle mouse)
		if (g_mouseMiddle && g_enableGroundTilt) {
			double dx = mouseX - g_lastMouseX;
			double dy = mouseY - g_lastMouseY;
			g_groundTiltZ += (float)dx * 0.01f;
			g_groundTiltX -= (float)dy * 0.01f;
			UpdateGroundTilt();
		}

		g_lastMouseX = mouseX;
		g_lastMouseY = mouseY;

		// Mouse Picking (left mouse)
		if (g_mouseLeft && !g_mouseConstraint && !g_selectedObject) {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			btVector3 rayFrom(g_camera.position.x(), g_camera.position.y(), g_camera.position.z());
			btVector3 rayDir = GetRayDirectionFromMouse(window, mouseX, mouseY, width, height);
			btVector3 hitPoint;

			g_selectedObject = Raycast(g_dynamicsWorld, rayFrom, rayFrom + rayDir * 100.0f, hitPoint);

			if (g_selectedObject) {
				g_dynamicsWorld->addRigidBody(g_mousePickerBody);

				btTransform pickerTrans;
				pickerTrans.setIdentity();
				pickerTrans.setOrigin(hitPoint);
				g_mousePickerBody->setWorldTransform(pickerTrans);

				btVector3 localPivot = g_selectedObject->body->getWorldTransform().inverse() * hitPoint;
				g_mouseConstraint = new btPoint2PointConstraint(*g_selectedObject->body, *g_mousePickerBody, localPivot, btVector3(0,0,0));
				g_dynamicsWorld->addConstraint(g_mouseConstraint);
			}
		}

		// Update drag
		if (g_mouseLeft && g_mouseConstraint && g_selectedObject) {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			btVector3 rayFrom(g_camera.position.x(), g_camera.position.y(), g_camera.position.z());
			btVector3 rayDir = GetRayDirectionFromMouse(window, mouseX, mouseY, width, height);

			btVector3 constraintPos = g_mousePickerBody->getWorldTransform().getOrigin();
			btVector3 cameraToConstraint = constraintPos - rayFrom;
			float distance = cameraToConstraint.length();

			btVector3 targetPos = rayFrom + rayDir * distance;

			btTransform pickerTrans;
			pickerTrans.setIdentity();
			pickerTrans.setOrigin(targetPos);
			g_mousePickerBody->setWorldTransform(pickerTrans);
		}

		// ImGui Frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Physics Controls");
		ImGui::Text("Mouse Controls:");
		ImGui::BulletText("Right drag: Rotate camera");
		ImGui::BulletText("Scroll: Zoom in/out");
		ImGui::BulletText("Left drag: Drag/Pick object");
		ImGui::BulletText("Middle drag: Tilt ground (when enabled)");
		ImGui::Checkbox("Enable Ground Tilt", &g_enableGroundTilt);
		ImGui::Separator();

		if (ImGui::Button(paused ? "Resume" : "Pause")) paused = !paused;
		ImGui::SameLine();
		if (ImGui::Button("Step")) step = true;
		ImGui::Separator();

		// Ground Tilt Sliders (display in degrees)
		if (g_enableGroundTilt) {
			ImGui::Text("Ground Tilt (degrees):");
			float tiltXDeg = g_groundTiltX * 57.2958f;
			float tiltZDeg = g_groundTiltZ * 57.2958f;

			if (ImGui::SliderFloat("Tilt X", &tiltXDeg, -30.0f, 30.0f, "%.1f")) {
				g_groundTiltX = tiltXDeg * 0.0174533f;
				UpdateGroundTilt();
			}
			if (ImGui::SliderFloat("Tilt Z", &tiltZDeg, -30.0f, 30.0f, "%.1f")) {
				g_groundTiltZ = tiltZDeg * 0.0174533f;
				UpdateGroundTilt();
			}
			if (ImGui::Button("Reset Tilt")) {
				g_groundTiltX = 0.0f;
				g_groundTiltZ = 0.0f;
				UpdateGroundTilt();
			}
			ImGui::Separator();
		}

		// Gravity Control
		static float gravityArr[3] = { 0, -10, 0 };
		ImGui::Text("Gravity:");
		if (ImGui::InputFloat3("##Gravity", gravityArr, "%.2f")) {
			g_dynamicsWorld->setGravity(btVector3(gravityArr[0], gravityArr[1], gravityArr[2]));
		}
		ImGui::Separator();

		// Add Sphere
		ImGui::Text("Add Sphere:");
		ImGui::SliderFloat("Radius", &g_sphereRadius, 0.1f, 3.0f);
		ImGui::SliderFloat("Mass", &g_sphereMass, 0.1f, 10.0f);
		ImGui::SliderFloat("Height", &g_sphereHeight, 1.0f, 20.0f);
		if (ImGui::Button("Add Sphere")) {
			CreateSphere(btVector3(0, g_sphereHeight, 0), g_sphereRadius, g_sphereMass);
		}
		ImGui::Separator();

		// Add Box
		ImGui::Text("Add Box:");
		float boxExtents[3] = { g_boxHalfExtents.x(), g_boxHalfExtents.y(), g_boxHalfExtents.z() };
		ImGui::SliderFloat3("Half Size", boxExtents, 0.1f, 3.0f);
		g_boxHalfExtents.setValue(boxExtents[0], boxExtents[1], boxExtents[2]);
		ImGui::SliderFloat("Box Mass", &g_boxMass, 0.1f, 10.0f);
		ImGui::SliderFloat("Box Height", &g_boxHeight, 1.0f, 20.0f);
		if (ImGui::Button("Add Box")) {
			CreateBox(btVector3(0, g_boxHeight, 0), g_boxHalfExtents, g_boxMass);
		}
		ImGui::Separator();

		// Clear All
		if (ImGui::Button("Clear All Objects")) {
			if (g_mouseConstraint) {
				g_dynamicsWorld->removeConstraint(g_mouseConstraint);
				delete g_mouseConstraint;
				g_mouseConstraint = nullptr;
				g_selectedObject = nullptr;
			}

			for (auto& obj : *g_physicsObjects) {
				g_dynamicsWorld->removeRigidBody(obj->body);
				delete obj->body;
				delete obj->motionState;
				delete obj->shape;
			}
			g_physicsObjects->clear();
			g_objIndex = 0;
		}

		ImGui::Separator();
		ImGui::Checkbox("Show ImGui Demo", &showDemo);
		ImGui::Text("Physics objects: %d", (int)g_physicsObjects->size());
		ImGui::End();

		// 光照控制面板
		ImGui::Begin("Lighting Controls");

		ImGui::Checkbox("Show Light Markers", &g_showLightMarkers);
		ImGui::SliderFloat("Global Ambient", &g_globalAmbient, 0.0f, 0.5f);
		ImGui::Separator();

		// 主光
		ImGui::Text("Key Light (GL_LIGHT0) - Warm");
		ImGui::Checkbox("Enable##L0", &g_light0Enabled);
		if (g_light0Enabled) {
			ImGui::SliderFloat3("Position##L0", g_light0Pos, -40.0f, 40.0f);
			ImGui::SliderFloat("Ambient##L0",  &g_light0Ambient,  0.0f, 1.0f);
			ImGui::SliderFloat("Diffuse##L0",  &g_light0Diffuse,  0.0f, 1.5f);
			ImGui::SliderFloat("Specular##L0", &g_light0Specular, 0.0f, 1.5f);
		}
		ImGui::Separator();

		// 补光
		ImGui::Text("Fill Light (GL_LIGHT1) - Cool Blue");
		ImGui::Checkbox("Enable##L1", &g_light1Enabled);
		if (g_light1Enabled) {
			ImGui::SliderFloat3("Position##L1", g_light1Pos, -40.0f, 40.0f);
			ImGui::SliderFloat("Diffuse##L1",  &g_light1Diffuse,  0.0f, 1.0f);
			ImGui::SliderFloat("Specular##L1", &g_light1Specular, 0.0f, 1.0f);
		}
		ImGui::Separator();

		// 背光
		ImGui::Text("Rim Light (GL_LIGHT2) - Back");
		ImGui::Checkbox("Enable##L2", &g_light2Enabled);
		if (g_light2Enabled) {
			ImGui::SliderFloat3("Position##L2", g_light2Pos, -40.0f, 40.0f);
			ImGui::SliderFloat("Diffuse##L2",  &g_light2Diffuse,  0.0f, 1.0f);
			ImGui::SliderFloat("Specular##L2", &g_light2Specular, 0.0f, 1.0f);
		}
		ImGui::Separator();

		// 材质
		ImGui::Text("Material");
		ImGui::SliderFloat("Specular",   &g_materialSpecular,   0.0f, 1.0f);
		ImGui::SliderFloat("Shininess",  &g_materialShininess,  1.0f, 128.0f);

		ImGui::Separator();
		if (ImGui::Button("Reset Lighting")) {
			g_light0Enabled = true;
			g_light0Pos[0] = 15.0f;  g_light0Pos[1] = 25.0f; g_light0Pos[2] = 15.0f;
			g_light0Ambient = 0.15f;  g_light0Diffuse = 0.85f; g_light0Specular = 0.9f;
			g_light1Enabled = true;
			g_light1Pos[0] = -15.0f;  g_light1Pos[1] = 15.0f; g_light1Pos[2] = -10.0f;
			g_light1Diffuse = 0.35f;  g_light1Specular = 0.15f;
			g_light2Enabled = true;
			g_light2Pos[0] = 0.0f;    g_light2Pos[1] = 12.0f; g_light2Pos[2] = -25.0f;
			g_light2Diffuse = 0.25f;  g_light2Specular = 0.2f;
			g_globalAmbient = 0.18f;
			g_materialShininess = 64.0f;
			g_materialSpecular = 0.6f;
		}
		ImGui::End();

		if (showDemo) ImGui::ShowDemoWindow(&showDemo);

		// Step Simulation
		if (!paused || step) {
			g_dynamicsWorld->stepSimulation(1.f/60.f, 10);
			step = false;
		}

		// Rendering
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0f, 1.0f);

		// Projection
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45.0f, (float)display_w / (float)display_h, 0.1f, 1000.0f);

		// View
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		gluLookAt(
			g_camera.position.x(), g_camera.position.y(), g_camera.position.z(),
			g_camera.target.x(), g_camera.target.y(), g_camera.target.z(),
			g_camera.up.x(), g_camera.up.y(), g_camera.up.z()
		);

		// Lighting - 配置多光源、全局环境光和材质
		SetupSceneLights();

		// Draw Ground (with tilt) - use physics transform for accuracy
		glDisable(GL_LIGHTING);
		glDisable(GL_POLYGON_OFFSET_FILL);

		// Draw ground using its actual physics transform
		btTransform groundPhysTrans;
		g_groundBody->getMotionState()->getWorldTransform(groundPhysTrans);

		glPushMatrix();
		btScalar groundMatrix[16];
		groundPhysTrans.getOpenGLMatrix(groundMatrix);
		glMultMatrixf(groundMatrix);

		// Draw ground plane at its local origin
		glColor4f(0.2f, 0.5f, 0.2f, 0.9f);
		glBegin(GL_QUADS);
		glNormal3f(0, 1, 0);
		glVertex3f(-20.0f, 0, -20.0f);
		glVertex3f(20.0f, 0, -20.0f);
		glVertex3f(20.0f, 0, 20.0f);
		glVertex3f(-20.0f, 0, 20.0f);
		glEnd();

		// Draw grid on ground
		glColor3f(0.3f, 0.3f, 0.3f);
		glBegin(GL_LINES);
		for (float i = -20.0f; i <= 20.0f; i += 1.0f) {
			glVertex3f(-20.0f, 0.01f, i);
			glVertex3f(20.0f, 0.01f, i);
			glVertex3f(i, 0.01f, -20.0f);
			glVertex3f(i, 0.01f, 20.0f);
		}
		glEnd();

		glPopMatrix();

		// Draw walls using their actual physics transforms
		// 墙体已定义法线，启用光照让它们也受场景灯光影响
		glEnable(GL_LIGHTING);
		glColor4f(0.25f, 0.35f, 0.25f, 0.9f);
		for (size_t i = 1; i < g_groundObjects.size(); i++) {
			btTransform wallPhysTrans;
			g_groundObjects[i]->getMotionState()->getWorldTransform(wallPhysTrans);

			glPushMatrix();
			btScalar wallMatrix[16];
			wallPhysTrans.getOpenGLMatrix(wallMatrix);
			glMultMatrixf(wallMatrix);

			// Draw wall box at local origin (since transform handles position/rotation)
			btBoxShape* wallShape = static_cast<btBoxShape*>(g_groundObjects[i]->getCollisionShape());
			btVector3 halfExtents = wallShape->getHalfExtentsWithMargin();

			// Draw simple box for wall
			glBegin(GL_QUADS);
			// Front
			glNormal3f(0, 0, 1);
			glVertex3f(-halfExtents.x(), -halfExtents.y(), halfExtents.z());
			glVertex3f(halfExtents.x(), -halfExtents.y(), halfExtents.z());
			glVertex3f(halfExtents.x(), halfExtents.y(), halfExtents.z());
			glVertex3f(-halfExtents.x(), halfExtents.y(), halfExtents.z());
			// Back
			glNormal3f(0, 0, -1);
			glVertex3f(-halfExtents.x(), -halfExtents.y(), -halfExtents.z());
			glVertex3f(-halfExtents.x(), halfExtents.y(), -halfExtents.z());
			glVertex3f(halfExtents.x(), halfExtents.y(), -halfExtents.z());
			glVertex3f(halfExtents.x(), -halfExtents.y(), -halfExtents.z());
			// Top
			glNormal3f(0, 1, 0);
			glVertex3f(-halfExtents.x(), halfExtents.y(), -halfExtents.z());
			glVertex3f(-halfExtents.x(), halfExtents.y(), halfExtents.z());
			glVertex3f(halfExtents.x(), halfExtents.y(), halfExtents.z());
			glVertex3f(halfExtents.x(), halfExtents.y(), -halfExtents.z());
			// Bottom
			glNormal3f(0, -1, 0);
			glVertex3f(-halfExtents.x(), -halfExtents.y(), -halfExtents.z());
			glVertex3f(halfExtents.x(), -halfExtents.y(), -halfExtents.z());
			glVertex3f(halfExtents.x(), -halfExtents.y(), halfExtents.z());
			glVertex3f(-halfExtents.x(), -halfExtents.y(), halfExtents.z());
			// Right
			glNormal3f(1, 0, 0);
			glVertex3f(halfExtents.x(), -halfExtents.y(), -halfExtents.z());
			glVertex3f(halfExtents.x(), halfExtents.y(), -halfExtents.z());
			glVertex3f(halfExtents.x(), halfExtents.y(), halfExtents.z());
			glVertex3f(halfExtents.x(), -halfExtents.y(), halfExtents.z());
			// Left
			glNormal3f(-1, 0, 0);
			glVertex3f(-halfExtents.x(), -halfExtents.y(), -halfExtents.z());
			glVertex3f(-halfExtents.x(), -halfExtents.y(), halfExtents.z());
			glVertex3f(-halfExtents.x(), halfExtents.y(), halfExtents.z());
			glVertex3f(-halfExtents.x(), halfExtents.y(), -halfExtents.z());
			glEnd();

			glPopMatrix();
		}

		glEnable(GL_LIGHTING);
		glEnable(GL_POLYGON_OFFSET_FILL);

		// Draw Physics Objects
		for (size_t i = 0; i < g_physicsObjects->size(); i++) {
			btTransform trans;
			(*g_physicsObjects)[i]->body->getMotionState()->getWorldTransform(trans);

			glPushMatrix();

			btScalar matrix[16];
			trans.getOpenGLMatrix(matrix);
			glMultMatrixf(matrix);

			btVector3 col = g_colors[(*g_physicsObjects)[i]->colorIndex];
			glColor3f(col.x(), col.y(), col.z());

			if ((*g_physicsObjects)[i]->type == ShapeType::Sphere) {
				btSphereShape* sphere = static_cast<btSphereShape*>((*g_physicsObjects)[i]->shape);
				DrawSphere(sphere->getRadius());
			} else {
				DrawBox((*g_physicsObjects)[i]->halfExtents);
			}

			glPopMatrix();
		}

		glDisable(GL_POLYGON_OFFSET_FILL);

		// 绘制光源位置标记（在所有 3D 物体之后，禁用深度测试以始终可见）
		DrawLightMarkers();

		// Draw Selection
		if (g_selectedObject) {
			btTransform trans;
			g_selectedObject->body->getMotionState()->getWorldTransform(trans);

			glDisable(GL_LIGHTING);
			glDisable(GL_DEPTH_TEST);
			glColor3f(1.0f, 1.0f, 0.0f);

			btVector3 pos = trans.getOrigin();

			glPushMatrix();
			glTranslatef(pos.x(), pos.y(), pos.z());
			DrawWireSphere(1.5f, 12);
			glPopMatrix();

			btVector3 pickerPos = g_mousePickerBody->getWorldTransform().getOrigin();
			glBegin(GL_LINES);
			glVertex3f(pos.x(), pos.y(), pos.z());
			glVertex3f(pickerPos.x(), pickerPos.y(), pickerPos.z());
			glEnd();

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_LIGHTING);
		}

		glDisable(GL_LIGHTING);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (g_mouseConstraint) {
		g_dynamicsWorld->removeConstraint(g_mouseConstraint);
		delete g_mouseConstraint;
	}

	if (g_mousePickerBody) {
		g_dynamicsWorld->removeRigidBody(g_mousePickerBody);
		delete g_mousePickerBody->getMotionState();
		delete g_mousePickerBody->getCollisionShape();
		delete g_mousePickerBody;
	}

	for (auto& obj : *g_physicsObjects) {
		g_dynamicsWorld->removeRigidBody(obj->body);
		delete obj->body;
		delete obj->motionState;
		delete obj->shape;
	}
	g_physicsObjects->clear();
	delete g_physicsObjects;

	for (btRigidBody* obj : g_groundObjects) {
		g_dynamicsWorld->removeRigidBody(obj);
		delete obj->getMotionState();
		delete obj->getCollisionShape();
		delete obj;
	}
	g_groundObjects.clear();

	delete g_dynamicsWorld;
	delete solver;
	delete dispatcher;
	delete collisionConfiguration;
	delete broadphase;

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
