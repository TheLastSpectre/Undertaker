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

//Keyboard Variables
GLfloat tranX = 0.0f;
GLfloat tranZ = 0.0f;
GLfloat rotX = 0.0f;
GLfloat rotY = 0.0f;
GLfloat EnemyX = 0.0f;
GLfloat EnemyZ = 0.0f;
int RandNum = 0;
int TimeCount = 0;
int LastTimeCount = 0;
bool PowerUp = false;
bool PowerUpTaken = false;

//Keyboard Input
void keyboard() {
	//Left
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS && PowerUp == false)
	{
		tranX += 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS && PowerUp == true)
	{
		tranX += 0.2f;
	}

	//Right
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS && PowerUp == false)
	{
		tranX -= 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS && PowerUp == true)
	{
		tranX -= 0.2f;
	}

	//Up
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && PowerUp == false)
	{
		tranZ += 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && PowerUp == true)
	{
		tranZ += 0.2f;
	}

	//Down
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && PowerUp == false)
	{
		tranZ -= 0.1f;
	}
	else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && PowerUp == true)
	{
		tranZ -= 0.2f;
	}

	if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
	{
		rotX += 1.5f;
		PowerUp = false;
	}

	if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
	{
		rotX -= 1.5f;
		PowerUp = true;
	}
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

		//Load OBJ
		//VertexArrayObject::sptr vao1 = ObjLoader::LoadFromFile("models/cube2.obj");

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 2.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow = 1.0f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1f;
		float     lightLinearFalloff = 0.0f;
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
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
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
			terrain.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
			terrain.get<Transform>().SetLocalRotation(0.0f, 0.0f, 1.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(terrain);
		}
		
		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 15, -8).LookAt(glm::vec3(0, 0, -4));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::BindDisabled<CameraControlBehaviour>(cameraObject);
		}

		VertexArrayObject::sptr vao1 = ObjLoader::LoadFromFile("models/fence.obj");
		VertexArrayObject::sptr vao2 = ObjLoader::LoadFromFile("models/cube2.obj");
		VertexArrayObject::sptr vao3 = ObjLoader::LoadFromFile("models/house.obj");

		GameObject barrier = scene->CreateEntity("barrier");
		{
			barrier.emplace<RendererComponent>().SetMesh(vao3).SetMaterial(checkertexture);
			barrier.get<Transform>().SetLocalPosition(tranX, 3.0f, tranZ);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(barrier);
		}


			GameObject cube = scene->CreateEntity("cube");
			{
				cube.emplace<RendererComponent>().SetMesh(vao2).SetMaterial(checkertexture);
				cube.get<Transform>().SetLocalPosition(3.0f, 1.0f, 8.0f);
				BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(cube);
			}

			/*
			GameObject enemy = scene->CreateEntity("enemy");
			{
				enemy.emplace<RendererComponent>().SetMesh(vao1).SetMaterial(checkertexture);
				enemy.get<Transform>().SetLocalPosition(0, 3.0f, 0);
				BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(enemy);
			}
			*/

		

		#pragma endregion 
		{
			// Load our shaders
			Shader::sptr shaders = std::make_shared<Shader>();
			shaders->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
			shaders->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
			shaders->Link();

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
				

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;


			
			

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			keyboard();

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
			

			//Set Player Movements
			barrier.get<Transform>().SetLocalPosition(tranX, 1.0f, tranZ);
			barrier.get<Transform>().SetLocalRotation(0.0f, rotX, 1.0f);

			//Spawn Enemy
			if(TimeCount % 5 == 0)
			{
				RandNum = rand() % 3;
				if (RandNum == 0)
				{
					EnemyX = (rand() % 22) - 9;
					EnemyZ = (rand() % 22) - 11;
					if ((EnemyX > -9 && EnemyX < -7) || (EnemyX > 11 && EnemyX < 13))
					{
						GameObject enemy = scene->CreateEntity("enemy");
						{
							enemy.emplace<RendererComponent>().SetMesh(vao1).SetMaterial(checkertexture);
							enemy.get<Transform>().SetLocalPosition(EnemyX, 3.0f, EnemyZ);
							BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(enemy);
						}
						
					}
				}
				else if (RandNum == 1)
				{
					EnemyX = (rand() % 22) - 9;
					EnemyZ = (rand() % 24) - 13;
					if ((EnemyZ > -13 && EnemyZ < -11) || (EnemyZ > 9 && EnemyZ < 11))
					{
						GameObject enemy = scene->CreateEntity("enemy");
						{
							enemy.emplace<RendererComponent>().SetMesh(vao1).SetMaterial(checkertexture);
							enemy.get<Transform>().SetLocalPosition(EnemyX, 3.0f, EnemyZ);
							BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(enemy);
						}
						
					}

				}
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
				else if (renderer.Mesh == vao3)
				{
					barrier.get<Transform>().SetLocalPosition(tranX	, 1.0f, tranZ);
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					barrier.get<Transform>().SetLocalPosition(0, 1.0f, 0);
					RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
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