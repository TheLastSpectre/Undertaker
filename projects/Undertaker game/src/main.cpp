#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Gameplay/Camera.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Behaviours/CameraControlBehaviour.h"
#include "Behaviours/FollowPathBehaviour.h"
#include "Behaviours/SimpleMoveBehaviour.h"
#include "Gameplay/Application.h"
#include "Gameplay/GameObjectTag.h"
#include "Gameplay/IBehaviour.h"
#include "Gameplay/Transform.h"
#include "Gameplay/Enemy.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Texture2DData.h"
#include "Utilities/InputHelpers.h"
#include "Utilities/MeshBuilder.h"
#include "Utilities/MeshFactory.h"
#include "Utilities/ObjLoader.h"
#include "Utilities/VertexTypes.h"
#include "Gameplay/Scene.h"
#include "Gameplay/ShaderMaterial.h"
#include "Gameplay/RendererComponent.h"
#include "Gameplay/Timing.h"
#include "Graphics/TextureCubeMap.h"
#include "Graphics/TextureCubeMapData.h"
#include <cstdlib>


#define LOG_GL_NOTIFICATIONS

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
	case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
	case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
	case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
		#ifdef LOG_GL_NOTIFICATIONS
	case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
		#endif
	default: break;
	}
}

GLFWwindow* window;

void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	Application::Instance().ActiveScene->Registry().view<Camera>().each([=](Camera & cam) {
		cam.ResizeWindow(width, height);
	});
}

bool initGLFW() {
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
	
	//Create a new GLFW window
	window = glfwCreateWindow(800, 800, "INFR1350U", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	// Store the window in the application singleton
	Application::Instance().Window = window;

	return true;
}

bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

void InitImGui() {
	// Creates a new ImGUI context
	ImGui::CreateContext();
	// Gets our ImGUI input/output 
	ImGuiIO& io = ImGui::GetIO();
	// Enable keyboard navigation
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Allow docking to our window
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Allow multiple viewports (so we can drag ImGui off our window)
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	// Allow our viewports to use transparent backbuffers
	io.ConfigFlags |= ImGuiConfigFlags_TransparentBackbuffers;

	// Set up the ImGui implementation for OpenGL
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	// Dark mode FTW
	ImGui::StyleColorsDark();

	// Get our imgui style
	ImGuiStyle& style = ImGui::GetStyle();
	//style.Alpha = 1.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.8f;
	}
}

void ShutdownImGui()
{
	// Cleanup the ImGui implementation
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	// Destroy our ImGui context
	ImGui::DestroyContext();
}

std::vector<std::function<void()>> imGuiCallbacks;
void RenderImGui() {
	// Implementation new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	// ImGui context new frame
	ImGui::NewFrame();

	if (ImGui::Begin("Debug")) {
		// Render our GUI stuff
		for (auto& func : imGuiCallbacks) {
			func();
		}
		ImGui::End();
	}
	
	// Make sure ImGui knows how big our window is
	ImGuiIO& io = ImGui::GetIO();
	int width{ 0 }, height{ 0 };
	glfwGetWindowSize(window, &width, &height);
	io.DisplaySize = ImVec2((float)width, (float)height);

	// Render all of our ImGui elements
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// If we have multiple viewports enabled (can drag into a new window)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		// Update the windows that ImGui is using
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		// Restore our gl context
		glfwMakeContextCurrent(window);
	}
}

void RenderVAO(
	const Shader::sptr& shader,
	const VertexArrayObject::sptr& vao,
	const glm::mat4& viewProjection,
	const Transform& transform)
{
	shader->SetUniformMatrix("u_ModelViewProjection", viewProjection * transform.LocalTransform());
	shader->SetUniformMatrix("u_Model", transform.LocalTransform()); 
	shader->SetUniformMatrix("u_NormalMatrix", transform.NormalMatrix());
	vao->Render();
}

void SetupShaderForFrame(const Shader::sptr& shader, const glm::mat4& view, const glm::mat4& projection) {
	shader->Bind();
	// These are the uniforms that update only once per frame
	shader->SetUniformMatrix("u_View", view);
	shader->SetUniformMatrix("u_ViewProjection", projection * view);
	shader->SetUniformMatrix("u_SkyboxMatrix", projection * glm::mat4(glm::mat3(view)));
	glm::vec3 camPos = glm::inverse(view) * glm::vec4(0,0,0,1);
	shader->SetUniform("u_CamPos", camPos);
}

//Variables
GLfloat tranX = 0.0f;
GLfloat tranZ = 0.0f;
GLfloat rotY = 270.0f;
GLfloat EnemyX = 24.0f;
GLfloat EnemyZ = 24.0f;
GLfloat Enemy2X = -24.0f;
GLfloat Enemy2Z = -24.0f;
GLfloat EnemyPosX[200];
GLfloat EnemyPosZ[200];
GLfloat Enemy2PosX[200];
GLfloat Enemy2PosZ[200];
GLfloat BarrierX = -24;
GLfloat BarrierZ = -27.5;
GLfloat PosTimer;
GLfloat PosMaxTime = 1.5f;
GLfloat t = 0.0f;
GLfloat CatT = 0.0f;
GLfloat CatTimer;
GLfloat CatMaxTime = 4.0f;
int EnemyNum = 0;
int Enemy2Num = 0;
int RandNum = 0;
int TimeCount = 0;
int LastTimeCount = 0;
int EnemySpawnCount = 0;
int MaxEnemyCount = 0;
int Enemy2SpawnCount = 0;
int MaxEnemy2Count = 0;
int CatmullLoop = 0;
int CatmullX;
bool PowerUp = false;
bool PowerUpTaken = false;
bool PULerp = true;
glm::vec3 p0(20,10, 0), p1(0,10,20), p2(-20,10,0), p3(0,10,-20);

GLfloat PUOriginPos = 1.0f;
GLfloat PUNewPos = 3.5f;

//Keyboard Input
void keyboard() {
	//Left
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && PowerUp == false)
	{
		tranX += 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && PowerUp == true)
	{
		tranX += 0.2f;
	}

	//Right
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && PowerUp == false)
	{
		tranX -= 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && PowerUp == true)
	{
		tranX -= 0.2f;
	}

	//Up
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && PowerUp == false)
	{
		tranZ += 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && PowerUp == true)
	{
		tranZ += 0.2f;
	}

	//Down
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && PowerUp == false)
	{
		tranZ -= 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && PowerUp == true)
	{
		tranZ -= 0.2f;
	}

	if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
	{
		PowerUp = false;
	}

	if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
	{
		PowerUp = true;
	}
}

void mouse() {
	
	POINT p;
	glm::vec2 player(tranX, tranZ); //convert position coords to a vector

	//Grab from entire screen position
	if (GetCursorPos(&p)) 
	{
		//If mouse is on top-most application, convert the coords to be relative to that
		if (ScreenToClient(GetForegroundWindow(), &p))
		{
			glm::vec2 mouse(p.x,p.y);
			glm::vec2 playerToMouseDirection = (mouse - player);
			glm::normalize(playerToMouseDirection);
		}
	}

	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) 
	{
		//Spawn bullet
	}
}

template<typename T>
T LERP(const T& p0, const T& p1, float t)
{
	return(1.0f - t) * p0 + t * p1;
}

template<typename T>
T Catmull(const T& p0, const T& p1, const T& p2, const T& p3, float t)
{
	return 0.5f * (2.f * p1 + t * (-p0 + p2)
		+ t * t * (2.f * p0 - 5.f * p1 + 4.f * p2 - p3)
		+ t * t * t * (-p0 + 3.f * p1 - 3.f * p2 + p3));
}

int main() {
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		glm::vec3 lightPos = glm::vec3(-24.0f, 0.0f, 20.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow = 1.0f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1f;
		float     lightLinearFalloff = 0.1f;
		float     lightQuadraticFalloff = 0.0f;

		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		
		
		//SECOND SHADER
		// Load our shaders2
		Shader::sptr shader2 = Shader::Create();
		shader2->LoadShaderPartFromFile("shaders/vertex_shader2.glsl", GL_VERTEX_SHADER);
		shader2->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured2.glsl", GL_FRAGMENT_SHADER);
		shader2->Link();
		
		glm::vec3 lightPos2 = glm::vec3(24.0f, 0.0f, 20.0f);
		glm::vec3 lightCol2 = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow2 = 1.0f;
		float     lightSpecularPow2 = 1.0f;
		glm::vec3 ambientCol2 = glm::vec3(1.0f);
		float     ambientPow2 = 0.1f;
		float     lightLinearFalloff2 = 0.1f;
		float     lightQuadraticFalloff2 = 0.0f;

		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader2->SetUniform("u_LightPos2", lightPos2);
		shader2->SetUniform("u_LightCol", lightCol2);
		shader2->SetUniform("u_AmbientLightStrength", lightAmbientPow2);
		shader2->SetUniform("u_SpecularLightStrength", lightSpecularPow2);
		shader2->SetUniform("u_AmbientCol", ambientCol2);
		shader2->SetUniform("u_AmbientStrength", ambientPow2);
		shader2->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader2->SetUniform("u_LightAttenuationLinear", lightLinearFalloff2);
		shader2->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff2);
		
		// We'll add some ImGui controls to control our shader
		imGuiCallbacks.push_back([&]() {
			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -30.0f, 30.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

			if (ImGui::CollapsingHeader("Scene Level Lighting Settings 2"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader2->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader2->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings 2"))
			{
				if (ImGui::DragFloat3("Light Pos 2", glm::value_ptr(lightPos2), 0.01f, -30.0f, 30.0f)) {
					shader2->SetUniform("u_LightPos2", lightPos2);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader2->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader2->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader2->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader2->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader2->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

		
			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New  

		#pragma region TEXTURE LOADING

		// Load some textures from files
		Texture2D::sptr grass = Texture2D::LoadFromFile("images/Grass.jpg");
		Texture2D::sptr checker = Texture2D::LoadFromFile("images/checker.jpg");
		Texture2D::sptr red = Texture2D::LoadFromFile("images/red.jpg");
		Texture2D::sptr stone = Texture2D::LoadFromFile("images/stone.jpg");
		Texture2D::sptr wood = Texture2D::LoadFromFile("images/wood.jpg");
		Texture2D::sptr white = Texture2D::LoadFromFile("images/white.jpg");
		Texture2D::sptr skeleton = Texture2D::LoadFromFile("images/skeleton.png");
		Texture2D::sptr character = Texture2D::LoadFromFile("images/player.png");
		Texture2D::sptr zombie = Texture2D::LoadFromFile("images/zombie.png");
		Texture2D::sptr bullettex = Texture2D::LoadFromFile("images/bullet.png");
		//Texture2D::sptr diffuse2 = Texture2D::LoadFromFile("images/box.bmp");
		//Texture2D::sptr specular = Texture2D::LoadFromFile("images/Stone_001_Specular.png"); 


		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		auto renderGroup = scene->Registry().group<RendererComponent, Transform>();

		// Create a material and set some properties for it
		ShaderMaterial::sptr grassmaterial = ShaderMaterial::Create();  
		grassmaterial->Shader = shader;
		grassmaterial->Set("s_Diffuse", grass);
		grassmaterial->Set("u_Shininess", 8.0f);

		ShaderMaterial::sptr checkertexture = ShaderMaterial::Create();
		checkertexture->Shader = shader;
		checkertexture->Set("s_Diffuse", checker);
		checkertexture->Set("u_Shininess", 8.0f);

		ShaderMaterial::sptr redtexture = ShaderMaterial::Create();
		redtexture->Shader = shader;
		redtexture->Set("s_Diffuse", red);
		redtexture->Set("u_Shininess", 8.0f);

		ShaderMaterial::sptr stonetexture = ShaderMaterial::Create();
		stonetexture->Shader = shader;
		stonetexture->Set("s_Diffuse", stone);
		stonetexture->Set("u_Shininess", 8.0f);

		ShaderMaterial::sptr woodtexture = ShaderMaterial::Create();
		woodtexture->Shader = shader;
		woodtexture->Set("s_Diffuse", wood);
		woodtexture->Set("u_Shininess", 2.0f);

		ShaderMaterial::sptr whitetexture = ShaderMaterial::Create();
		whitetexture->Shader = shader;
		whitetexture->Set("s_Diffuse", white);
		whitetexture->Set("u_Shininess", 2.0f);

		ShaderMaterial::sptr skeletontexture = ShaderMaterial::Create();
		skeletontexture->Shader = shader;
		skeletontexture->Set("s_Diffuse", skeleton);
		skeletontexture->Set("u_Shininess", 8.0f);

		ShaderMaterial::sptr playertexture = ShaderMaterial::Create();
		playertexture->Shader = shader;
		playertexture->Set("s_Diffuse", character);
		playertexture->Set("u_Shininess", 8.0f);

		ShaderMaterial::sptr zombietexture = ShaderMaterial::Create();
		zombietexture->Shader = shader;
		zombietexture->Set("s_Diffuse", zombie);
		zombietexture->Set("u_Shininess", 8.0f);

		ShaderMaterial::sptr bullettexture = ShaderMaterial::Create();
		bullettexture->Shader = shader;
		bullettexture->Set("s_Diffuse", bullettex);
		bullettexture->Set("u_Shininess", 8.0f);

		// Load a second material for our reflective material!
		Shader::sptr reflectiveShader = Shader::Create();
		reflectiveShader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflectiveShader->LoadShaderPartFromFile("shaders/frag_reflection.frag.glsl", GL_FRAGMENT_SHADER);
		reflectiveShader->Link();

		//GameObjects
		GameObject terrain = scene->CreateEntity("Terrain");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Terrain.obj");
			terrain.emplace<RendererComponent>().SetMesh(vao).SetMaterial(grassmaterial);
			terrain.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f).SetLocalScale(2.0f, 0.0f, 2.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(terrain);
		}
		
		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 33.5, -18).LookAt(glm::vec3(0, 0, -10));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::BindDisabled<CameraControlBehaviour>(cameraObject);
		}

		VertexArrayObject::sptr vao1 = ObjLoader::LoadFromFile("models/skeleton.obj");
		VertexArrayObject::sptr vao2 = ObjLoader::LoadFromFile("models/powerup.obj");
		VertexArrayObject::sptr vao3 = ObjLoader::LoadFromFile("models/player.obj");
		VertexArrayObject::sptr vao5 = ObjLoader::LoadFromFile("models/cube2.obj");
		VertexArrayObject::sptr vao6 = ObjLoader::LoadFromFile("models/fence.obj");
		VertexArrayObject::sptr vao7 = ObjLoader::LoadFromFile("models/fencegate.obj");
		VertexArrayObject::sptr vao18 = ObjLoader::LoadFromFile("models/spiderweb.obj");
		//tree vao
		VertexArrayObject::sptr vao8 = ObjLoader::LoadFromFile("models/deadTree.obj");
		VertexArrayObject::sptr vao9 = ObjLoader::LoadFromFile("models/deadTree2.obj");
		VertexArrayObject::sptr vao10 = ObjLoader::LoadFromFile("models/tree stump 1.obj");
		VertexArrayObject::sptr vao11 = ObjLoader::LoadFromFile("models/tree stump 2.obj");
		VertexArrayObject::sptr vao12 = ObjLoader::LoadFromFile("models/tree stump 3.obj");
		VertexArrayObject::sptr vao13 = ObjLoader::LoadFromFile("models/tree stump 4.obj");
		VertexArrayObject::sptr vao14 = ObjLoader::LoadFromFile("models/tree stump 5.obj");
		//gravestone vao
		VertexArrayObject::sptr vao15 = ObjLoader::LoadFromFile("models/graveStone1.obj");
		VertexArrayObject::sptr vao16 = ObjLoader::LoadFromFile("models/graveStone2.obj");
		VertexArrayObject::sptr vao17 = ObjLoader::LoadFromFile("models/roundedGrave.obj");
		VertexArrayObject::sptr vao4 = ObjLoader::LoadFromFile("models/cross.obj");
		VertexArrayObject::sptr vao19 = ObjLoader::LoadFromFile("models/wall broken wall.obj");
		VertexArrayObject::sptr vao20 = ObjLoader::LoadFromFile("models/zombie.obj");
		VertexArrayObject::sptr vao21 = ObjLoader::LoadFromFile("models/Bullet.obj");

		//Player vao
		GameObject player = scene->CreateEntity("player");
		{
			player.emplace<RendererComponent>().SetMesh(vao3).SetMaterial(playertexture);
			player.get<Transform>().SetLocalPosition(tranX, 3.0f, tranZ).SetLocalRotation(0.0, 0.0f, 0.0).SetLocalScale(2,2, 2);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(player);
		}

		//Barrier vao
		GameObject barrier = scene->CreateEntity("barrier");
		{
			barrier.emplace<RendererComponent>().SetMesh(vao6).SetMaterial(woodtexture);
			barrier.get<Transform>().SetLocalPosition(-24, 3.0f, -27.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(barrier);
		}

		GameObject fencegate = scene->CreateEntity("fencegate");
		{
			fencegate.emplace<RendererComponent>().SetMesh(vao7).SetMaterial(woodtexture);
			fencegate.get<Transform>().SetLocalPosition(-1, 3.0f, 26);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(fencegate);
		}

		//Powerup vao
		GameObject powerup = scene->CreateEntity("powerup");
		{
			powerup.emplace<RendererComponent>().SetMesh(vao2).SetMaterial(redtexture);
			powerup.get<Transform>().SetLocalPosition(3.0f, 1.0f, 8.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(powerup);
		}

		//Enemy vao	
		GameObject enemy = scene->CreateEntity("enemy");
		{
			enemy.emplace<RendererComponent>().SetMesh(vao1).SetMaterial(skeletontexture);
			enemy.get<Transform>().SetLocalPosition(-24, 3.0f, 0).SetLocalScale(4, 4, 4).SetLocalRotation(0, 180, 0);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(enemy);
		}

		GameObject enemy2 = scene->CreateEntity("enemy2");
		{
			enemy2.emplace<RendererComponent>().SetMesh(vao20).SetMaterial(zombietexture);
			enemy2.get<Transform>().SetLocalPosition(24, 3.0f, 0).SetLocalScale(1, 1, 1).SetLocalRotation(0, 270, 0);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(enemy2);
		}

		//Object vaos
		GameObject cross = scene->CreateEntity("cross");
		{
			cross.emplace<RendererComponent>().SetMesh(vao4).SetMaterial(checkertexture);
			cross.get<Transform>().SetLocalPosition(5, 1, -8).SetLocalRotation(0, 90, 0).SetLocalScale(0.4, 0.5, 0.5);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(cross);
		}

		GameObject slab = scene->CreateEntity("slab");
		{
			slab.emplace<RendererComponent>().SetMesh(vao5).SetMaterial(checkertexture);
			slab.get<Transform>().SetLocalPosition(-5, 1, 6);
			//slab.get<Transform>().SetLocalScale(0.2, 0.2, 0.2);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(slab);
		}

		GameObject spiderweb = scene->CreateEntity("spiderweb");
		{
			spiderweb.emplace<RendererComponent>().SetMesh(vao18).SetMaterial(whitetexture);
			spiderweb.get<Transform>().SetLocalPosition(-18, 1, -1);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(spiderweb);
		}

		GameObject deadtree = scene->CreateEntity("deadtree");
		{
			deadtree.emplace<RendererComponent>().SetMesh(vao8).SetMaterial(woodtexture);
			deadtree.get<Transform>().SetLocalPosition(18, 1, 8);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(deadtree);
		}

		GameObject deadtree2 = scene->CreateEntity("deadtree2");
		{
			deadtree2.emplace<RendererComponent>().SetMesh(vao9).SetMaterial(woodtexture);
			deadtree2.get<Transform>().SetLocalPosition(-18, 1, 14);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(deadtree2);
		}

		GameObject bullet = scene->CreateEntity("bullet");
		{
			bullet.emplace<RendererComponent>().SetMesh(vao21).SetMaterial(woodtexture);
			bullet.get<Transform>().SetLocalPosition(0, 5, 0).SetLocalScale(0.2, 0.2, 0.2);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(bullet);
		}
		/*
		GameObject treestump1 = scene->CreateEntity("treestump1");
		{
			treestump1.emplace<RendererComponent>().SetMesh(vao15).SetMaterial(woodtexture);
			treestump1.get<Transform>().SetLocalPosition(-36, 1, -10);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(treestump1);
		}
		*/

		#pragma endregion 
		{
			// Load our shaders
			Shader::sptr shaders = std::make_shared<Shader>();
			shaders->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
			shaders->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
			shaders->Link();

			// Load our shaders
			Shader::sptr shaders2 = std::make_shared<Shader>();
			shaders2->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
			shaders2->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
			shaders2->Link();


			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
		}
		////////////////////////////////////////////////////////////////////////////////////////

		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			
		}
		
		InitImGui();

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		double testNum;

		///// Game loop /////
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			TimeCount = TimeCount + 1;
			EnemySpawnCount = TimeCount / 200; //50
			
			//std::cout << Catmull(-28, -24, 24, 28, t) << "\n";
			
			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			PosTimer += time.DeltaTime;
			CatTimer += time.DeltaTime;

			lightPos = glm::vec3(tranX, 0.0f, tranZ);
			shader->SetUniform("u_LightPos", lightPos);

			if (PosTimer >= PosMaxTime)
			{
				PosTimer = 0.0f;
				PULerp = !PULerp;
			}

			if (CatTimer >= CatMaxTime)
			{
				CatTimer = 0.0f;
				CatmullLoop = CatmullLoop + 1;
				if (CatmullLoop >= 4)
					CatmullLoop = 0;
			}

			t = PosTimer / PosMaxTime;
			CatT = CatTimer / CatMaxTime;

			t = length(p1 - p0);

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			keyboard();
			mouse();

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;

			//Player Collision
			if (tranX > 27)
			{
				tranX = 27;
			}
			else if (tranX < -27)
			{
				tranX = -27;
			}

			if (tranZ > 27)
			{
				tranZ = 27;
			}
			else if (tranZ < -27.5)
			{
				tranZ = -27.5;
			}

			//Set Player Movements
			player.get<Transform>().SetLocalPosition(tranX, 1.0f, tranZ).SetLocalRotation(0.0f, rotY, 1.0f);
			//enemy.get<Transform>().SetLocalRotation(0, test, 0).SetLocalPosition(tranX, 1.0f, tranZ);

			//Power Up
			if (PULerp == true)
			{
				powerup.get<Transform>().SetLocalPosition(3.0f, LERP(PUOriginPos, PUNewPos, t), 8.0f);
			}
			else if (PULerp == false)
			{
				powerup.get<Transform>().SetLocalPosition(3.0f, LERP(PUNewPos, PUOriginPos, t), 8.0f);
			}

			//Power Up
			if (tranX > 2.0f && tranX < 5.0f && tranZ > 6.5f && tranZ < 8.0f && PowerUpTaken == false)
			{				
				if (PowerUpTaken == false)
				{
					PowerUp = true;
					LastTimeCount = TimeCount + 300;
				}

				PowerUpTaken = true;
			}

			if (TimeCount == LastTimeCount)
			{
				PowerUp = false;
			}

			//enemy.get<Transform>().SetLocalPosition(-24, 3.0f, 0).SetLocalScale(4, 4, 4).SetLocalRotation(0, 180, 0);
			//enemy2.get<Transform>().SetLocalPosition(-24, 3.0f, 0).SetLocalScale(1, 1, 1).SetLocalRotation(0, 180, 0);

			//glm::vec3 p0(20.0, 1.0, 0.0), p1(0.0, 1.0, 20.0), p2(-20.0, 1.0, 0.0), p3(0.0, 1.0, -20.0);
			//Power Up Catmull Circle
			if (CatmullLoop == 0)
			{
				powerup.get<Transform>().SetLocalPosition(Catmull(p0, p1, p2, p3, CatT));
			}
			else if (CatmullLoop == 1)
			{
				powerup.get<Transform>().SetLocalPosition(Catmull(p1, p2, p3, p0, CatT));
			}
			else if (CatmullLoop == 2)
			{
				powerup.get<Transform>().SetLocalPosition(Catmull(p2, p3, p0, p1, CatT));
			}
			else if (CatmullLoop == 3)
			{
				powerup.get<Transform>().SetLocalPosition(Catmull(p3, p0, p1, p2, CatT));
			}
			
			//Spawn Enemy
			if (TimeCount % 10 == 0 && MaxEnemyCount < 200)
			{
				RandNum = rand() % 3;

				if (RandNum == 0)
				{
					EnemyX = (rand() % 44) - 18;
					EnemyZ = (rand() % 44) - 22;
					if ((EnemyX > -18 && EnemyX < -14) || (EnemyX > 22 && EnemyX < 26))
					{
						EnemyPosX[EnemyNum] = EnemyX;
						EnemyPosZ[EnemyNum] = EnemyZ;
						EnemyNum = EnemyNum + 1;
						MaxEnemyCount = MaxEnemyCount + 1;
					}
			}else if (RandNum == 1)
				{
					EnemyX = (rand() % 44) - 18;
					EnemyZ = (rand() % 48) - 26;
					if ((EnemyZ > -26 && EnemyZ < -22) || (EnemyZ > 18 && EnemyZ < 22))
					{
						EnemyPosX[EnemyNum] = EnemyX;
						EnemyPosZ[EnemyNum] = EnemyZ;
						EnemyNum = EnemyNum + 1;
						MaxEnemyCount = MaxEnemyCount + 1;
					}
				}
			}

			//Spawn Enemy 2
			if (TimeCount % 10 == 0 && MaxEnemy2Count < 200)
			{
				RandNum = rand() % 3;

				if (RandNum == 0)
				{
					Enemy2X = (rand() % 44) - 18;
					Enemy2Z = (rand() % 44) - 22;
					if ((Enemy2X > -18 && Enemy2X < -14) || (Enemy2X > 22 && Enemy2X < 26))
					{
						Enemy2PosX[Enemy2Num] = Enemy2X;
						Enemy2PosZ[Enemy2Num] = Enemy2Z;
						Enemy2Num = Enemy2Num + 1;
						MaxEnemy2Count = MaxEnemy2Count + 1;
					}
				}
				else if (RandNum == 1)
				{
					Enemy2X = (rand() % 44) - 18;
					Enemy2Z = (rand() % 48) - 26;
					if ((Enemy2Z > -26 && Enemy2Z < -22) || (Enemy2Z > 18 && Enemy2Z < 22))
					{
						Enemy2PosX[Enemy2Num] = Enemy2X;
						Enemy2PosZ[Enemy2Num] = Enemy2Z;
						Enemy2Num = Enemy2Num + 1;
						MaxEnemy2Count = MaxEnemy2Count + 1;
					}
				}
			}
						
			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity, RendererComponent& renderer, Transform& transform) {
				// If the shader has changed, bind it and set up it's uniforms
				if (current != renderer.Material->Shader) {
					current = renderer.Material->Shader;
					current->Bind();
					SetupShaderForFrame(current, view, projection);
				}
				// If the material has changed, apply it
				if (currentMat != renderer.Material) {
					currentMat = renderer.Material;
					currentMat->Apply();
				}
				// Render the mesh
				if (renderer.Mesh == vao2 && PowerUpTaken == true)
				{				
				}
				else if (renderer.Mesh == vao1)
				{
					for (int Count = 0; Count < 200; Count++)
					{
						//Enemy X Movement
						if (EnemyPosX[Count] > tranX)
						{
							EnemyPosX[Count] = EnemyPosX[Count] - 0.02;
						}
						else if (EnemyPosX[Count] < tranX)
						{
							EnemyPosX[Count] = EnemyPosX[Count] + 0.02;
						}

						//Enemy Z movement
						if (EnemyPosZ[Count] > tranZ)
						{
							EnemyPosZ[Count] = EnemyPosZ[Count] - 0.02;
						}
						else if (EnemyPosZ[Count] < tranZ)
						{
							EnemyPosZ[Count] = EnemyPosZ[Count] + 0.02;
						}

						//Enemy Collision
						/*
						for (int Count2 = 0; Count2 < 200; Count2++)
						{
							if (Count == Count2)
							{
							}
							else {
								if ((EnemyPosX[Count] + 1) > (EnemyPosX[Count2] - 1) && (EnemyPosX[Count] + 1) < EnemyPosX[Count2])
								{
									EnemyPosX[Count] = EnemyPosX[Count2] - 2;
								}

								if ((EnemyPosX[Count] - 1) > (EnemyPosX[Count2] + 1) && (EnemyPosX[Count] - 1) < EnemyPosX[Count2])
								{
									EnemyPosX[Count] = EnemyPosX[Count2] + 2;
								}
							}
						}
						*/
						enemy.get<Transform>().SetLocalPosition(EnemyPosX[Count], 1.0f, EnemyPosZ[Count]);
						RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					}
				}
				else if (renderer.Mesh == vao6)
				{
					for (int Count = 0; Count < 18; Count++)
					{
						barrier.get<Transform>().SetLocalRotation(0, 0, 0).SetLocalPosition(BarrierX, 3.0f, -27.5f);;
						RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
						if (BarrierX == 0)
						{
						}
						else{
							barrier.get<Transform>().SetLocalPosition(BarrierX, 3.0f, 26);
							RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
						}
						BarrierX = BarrierX + 3;
					}

					for (int Count = 0; Count < 18; Count++)
					{
						barrier.get<Transform>().SetLocalRotation(0, 90, 0).SetLocalPosition(27, 3.0f, BarrierZ);
						RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
						barrier.get<Transform>().SetLocalPosition(-27, 3.0f, BarrierZ);
						RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
						BarrierZ = BarrierZ + 3;
					}

					BarrierX = -24;
					BarrierZ = -27.5f;
				}
				else if (renderer.Mesh == vao20)
				{
					for (int Count = 0; Count < 200; Count++)
					{
						//Enemy X Movement
						if (Enemy2PosX[Count] > tranX)
						{
							Enemy2PosX[Count] = Enemy2PosX[Count] - 0.02;
						}
						else if (Enemy2PosX[Count] < tranX)
						{
							Enemy2PosX[Count] = Enemy2PosX[Count] + 0.02;
						}

						//Enemy Z movement
						if (Enemy2PosZ[Count] > tranZ)
						{
							Enemy2PosZ[Count] = Enemy2PosZ[Count] - 0.02;
						}
						else if (Enemy2PosZ[Count] < tranZ)
						{
							Enemy2PosZ[Count] = Enemy2PosZ[Count] + 0.02;
						}
						enemy2.get<Transform>().SetLocalPosition(Enemy2PosX[Count], 1.0f, Enemy2PosZ[Count]).SetLocalRotation(0, 270, 0);
						RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					}
				}
				else
				{
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
				}
			});


			// Draw our ImGui content
			RenderImGui();

			scene->Poll();
			glfwSwapBuffers(window);
			time.LastFrame = time.CurrentFrame;
		}
		
		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}