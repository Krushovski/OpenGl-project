#include <GL\glew.h>
#include <glm/gtc/random.hpp>
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <gl\GLU.h>

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>

#include <iostream>

#include "Shader.h"
#include "Camera.h"
#include "Model.h"

enum class Weather {
	Sunny,
	Rainy,
	Night
};

Weather currentWeather = Weather::Sunny;

struct RainDrop {
	glm::vec3 position;
	float speed;
};
std::vector<RainDrop> rain;
const int RAIN_COUNT = 3000;
bool rainEnabled = false;

GLuint rainVAO, rainVBO;

float lampIntensity = 1.0f;
bool sconceLightOn = true;
glm::vec3 sconceLightPos = glm::vec3(-6.0f, 0.8f, -0.6f);
glm::vec3 sconceLightColor = glm::vec3(1.0f, 0.85f, 0.6f);
bool init();
bool initGL();
void render();
GLuint CreateCube(float, GLuint&);
void DrawCube(GLuint id);
void close();
void InitRain();
void InitRainBuffers();
void InitShadowBuffers();
void renderSceneDepth(const Shader& shader, const glm::mat4& lightSpaceMatrix);
void renderScene(const Shader& shader, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir);
//The window we'll be rendering to
SDL_Window* gWindow = NULL;

//OpenGL context
SDL_GLContext gContext;

Shader gShader, gLightSource_1, rainShader, depthShader;
Model gModelHouse, gSconceLight, gPc, zahaLight;

GLuint gVAO, gVBO, gEBO;

// camera
Camera camera(glm::vec3(-4.5f, 0.9f, 3.0f));
float lastX = -1;
float lastY = -1;

bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// lighting
glm::vec3 lightPos(-4.5f, 0.9f, 3.0f);

GLuint depthMapFBO = 0;
GLuint depthMap = 0;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;

//event handlers
void HandleKeyDown(const SDL_KeyboardEvent& key);
void HandleMouseMotion(const SDL_MouseMotionEvent& motion);
void HandleMouseWheel(const SDL_MouseWheelEvent& wheel);
bool CheckObstacleCollision(glm::vec3 position);



struct AABB {
	glm::vec3 min;
	glm::vec3 max;
};

std::vector<AABB> worldObstacles;

AABB CalculateWorldAABB(const Model& model, glm::mat4 modelMatrix) {
	glm::vec3 localMin = model.GetAABBMin();
	glm::vec3 localMax = model.GetAABBMax();

	glm::vec3 corners[8] = {
		glm::vec3(localMin.x, localMin.y, localMin.z),
		glm::vec3(localMax.x, localMin.y, localMin.z),
		glm::vec3(localMin.x, localMax.y, localMin.z),
		glm::vec3(localMin.x, localMin.y, localMax.z),
		glm::vec3(localMax.x, localMax.y, localMin.z),
		glm::vec3(localMax.x, localMin.y, localMax.z),
		glm::vec3(localMin.x, localMax.y, localMax.z),
		glm::vec3(localMax.x, localMax.y, localMax.z)
	};

	glm::vec3 worldMin(FLT_MAX);
	glm::vec3 worldMax(-FLT_MAX);

	for (int i = 0; i < 8; ++i) {
		glm::vec4 worldPos = modelMatrix * glm::vec4(corners[i], 1.0f);

		worldMin.x = std::min(worldMin.x, worldPos.x);
		worldMin.y = std::min(worldMin.y, worldPos.y);
		worldMin.z = std::min(worldMin.z, worldPos.z);

		worldMax.x = std::max(worldMax.x, worldPos.x);
		worldMax.y = std::max(worldMax.y, worldPos.y);
		worldMax.z = std::max(worldMax.z, worldPos.z);
	}

	return { worldMin, worldMax };
}

bool CheckObstacleCollision(glm::vec3 position) {
	float playerSize = 0.45f;

	for (const auto& box : worldObstacles) {
		bool collisionX = position.x + playerSize > box.min.x && position.x - playerSize < box.max.x;
		bool collisionY = position.y + playerSize > box.min.y && position.y - playerSize < box.max.y;
		bool collisionZ = position.z + playerSize > box.min.z && position.z - playerSize < box.max.z;

		if (collisionX && collisionZ && collisionY) {
			return true;
		}
	}
	return false;
}




int main(int argc, char* args[])
{
	init();

	SDL_Event e;
	//While application is running
	bool quit = false;
	while (!quit)
	{
		// per-frame time logic
		// --------------------
		float currentFrame = SDL_GetTicks() / 1000.0f;
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//User requests quit
			if (e.type == SDL_QUIT)
			{
				quit = true;
			}
			switch (e.type)
			{
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				if (e.key.keysym.sym == SDLK_ESCAPE)
				{
					quit = true;
				}
				else
				{
					HandleKeyDown(e.key);
				}
				break;
			case SDL_MOUSEMOTION:
				HandleMouseMotion(e.motion);
				break;
			case SDL_MOUSEWHEEL:
				HandleMouseWheel(e.wheel);
				break;
			}
		}

		//Render
		render();

		//Update screen
		SDL_GL_SwapWindow(gWindow);
	}

	close();

	return 0;
}

void HandleKeyDown(const SDL_KeyboardEvent& key)
{
	float velocity = camera.MovementSpeed * deltaTime;
	glm::vec3 nextPos = camera.Position; // Predict where we are going

	switch (key.keysym.sym)
	{
	case SDLK_w:
		nextPos += camera.Front * velocity;
		if (!CheckObstacleCollision(nextPos)) camera.ProcessKeyboard(FORWARD, deltaTime);
		break;
	case SDLK_s:
		nextPos -= camera.Front * velocity;
		if (!CheckObstacleCollision(nextPos)) camera.ProcessKeyboard(BACKWARD, deltaTime);
		break;
	case SDLK_a:
		nextPos -= camera.Right * velocity;
		if (!CheckObstacleCollision(nextPos)) camera.ProcessKeyboard(LEFT, deltaTime);
		break;
	case SDLK_d:
		nextPos += camera.Right * velocity;
		if (!CheckObstacleCollision(nextPos)) camera.ProcessKeyboard(RIGHT, deltaTime);
		break;
	case SDLK_l:
		sconceLightOn = !sconceLightOn;
		break;
	case SDLK_p:
		if (currentWeather == Weather::Sunny)
			currentWeather = Weather::Rainy;
		else if (currentWeather == Weather::Rainy)
			currentWeather = Weather::Night;
		else
			currentWeather = Weather::Sunny;
		break;
	case SDLK_PLUS:          // main keyboard +
	case SDLK_KP_PLUS:      // numpad +
		lampIntensity += 0.1f;
		lampIntensity = glm::min(lampIntensity, 3.0f); 
		break;

	case SDLK_MINUS:         // main keyboard -
	case SDLK_KP_MINUS:     // numpad -
		lampIntensity -= 0.1f;
		lampIntensity = glm::max(lampIntensity, 0.0f); 
		break;

	}
}

void HandleMouseMotion(const SDL_MouseMotionEvent& motion)
{
	if (firstMouse)
	{
		lastX = motion.x;
		lastY = motion.y;
		firstMouse = false;
	}
	else
	{
		
		camera.ProcessMouseMovement(motion.x - lastX, lastY - motion.y);
		lastX = motion.x;
		lastY = motion.y;
	}
}

void HandleMouseWheel(const SDL_MouseWheelEvent& wheel)
{
	camera.ProcessMouseScroll(wheel.y);
}

bool init()
{
	//Initialization flag
	bool success = true;

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		success = false;
	}
	else
	{
		//Use OpenGL 3.3
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

		//Create window
		gWindow = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 840, 680,
			SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
		if (gWindow == NULL)
		{
			printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
			success = false;
		}
		else
		{
			//Create context
			gContext = SDL_GL_CreateContext(gWindow);
			if (gContext == NULL)
			{
				printf("OpenGL context could not be created! SDL Error: %s\n", SDL_GetError());
				success = false;
			}
			else
			{
				//Use Vsync
				if (SDL_GL_SetSwapInterval(1) < 0)
				{
					printf("Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
				}

				//Initialize OpenGL
				if (!initGL())
				{
					printf("Unable to initialize OpenGL!\n");
					success = false;
				}
			}
		}
	}

	return success;
}

void InitRain()
{
	rain.reserve(RAIN_COUNT);

	for (int i = 0; i < RAIN_COUNT; i++)
	{
		RainDrop d;
		d.position.x = glm::linearRand(-10.0f, 10.0f);
		d.position.y = glm::linearRand(5.0f, 15.0f);
		d.position.z = glm::linearRand(-10.0f, 10.0f);
		d.speed = glm::linearRand(8.0f, 15.0f);
		rain.push_back(d);
	}
}

bool initGL()
{
	bool success = true;
	GLenum error = GL_NO_ERROR;

	glewInit();

	error = glGetError();
	if (error != GL_NO_ERROR)
	{
		success = false;
		printf("Error initializing OpenGL! %s\n", gluErrorString(error));
	}

	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gShader.Load("./shaders/vertex.vert", "./shaders/fragment.frag");
	gLightSource_1.Load("./shaders/light_source_vertex.vert", "./shaders/light_source_fragment.frag");
	depthShader.Load("./shaders/shadow_depth.vert", "./shaders/shadow_depth.frag");
	rainShader.Load("./shaders/rain.vert", "./shaders/rain.frag");

	// Load Models
	gModelHouse.LoadModel("./models/house/house_interior.obj");
	gSconceLight.LoadModel("./models/light/eb_sconce_light_01.obj");
	gPc.LoadModel("./models/ko/ko.obj");
	zahaLight.LoadModel("./models/zahaLight/ZAHA LIGHT white chandelier.obj");
	// -----------------------------------------------------------------------
	// COLLISION SETUP
	// -----------------------------------------------------------------------

	// 1. SETUP HOUSE BOUNDARIES (The "Container")
	// Must match render() transform: Translate(-6,0,0) -> Scale(0.009)
	glm::mat4 houseModel = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, 0.0f));
	houseModel = glm::scale(houseModel, glm::vec3(0.009f, 0.009f, 0.009f));

	// Calculate world AABB for the house
	AABB houseBox = CalculateWorldAABB(gModelHouse, houseModel);

	// Pass this to the Camera so it knows the room limits
	// Margin 0.15f keeps us slightly away from the walls
	camera.SetAABBBounds(houseBox.min, houseBox.max, 0.15f);


	// 2. SETUP PC OBSTACLE
	// Must match render() transform: Translate(-5.5, 0, -0.2) -> Scale(0.009)
	glm::mat4 pcModel = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.8f, 5.2f));
	pcModel = glm::scale(pcModel, glm::vec3(0.006f, 0.006f, 0.006f));

	worldObstacles.push_back(CalculateWorldAABB(gPc, pcModel));


	// 3. SETUP SCONCE OBSTACLE
	// Must match render() transform: Translate(-6, 0.8, -0.6) -> Scale(1e-8)
	glm::mat4 lightModel = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.8f, -0.6f));
	lightModel = glm::scale(lightModel, glm::vec3(0.009f));

	worldObstacles.push_back(CalculateWorldAABB(gSconceLight, lightModel));

	// -----------------------------------------------------------------------

	InitRain();
	InitRainBuffers();

	gVAO = CreateCube(1.0f, gVBO);
	InitShadowBuffers();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	return success;
}


void InitShadowBuffers()
{
	GLuint depthFBO;
	glGenFramebuffers(1, &depthFBO);

	GLuint depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		2048, 2048, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, nullptr
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
	glFramebufferTexture2D(
		GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, depthMap, 0
	);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void InitRainBuffers()
{
	glGenVertexArrays(1, &rainVAO);
	glGenBuffers(1, &rainVBO);

	glBindVertexArray(rainVAO);
	glBindBuffer(GL_ARRAY_BUFFER, rainVBO);

	glBufferData(
		GL_ARRAY_BUFFER,
		RAIN_COUNT * 2 * sizeof(glm::vec3),
		nullptr,
		GL_DYNAMIC_DRAW
	);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}


void close()
{
	//delete GL programs, buffers and objects
	glDeleteProgram(gShader.ID);
	glDeleteProgram(depthShader.ID);
	glDeleteProgram(rainShader.ID);
	glDeleteVertexArrays(1, &gVAO);
	glDeleteProgram(gLightSource_1.ID);
	glDeleteBuffers(1, &gVBO);

	//Delete OGL context
	if (rainVAO) glDeleteVertexArrays(1, &rainVAO);
	if (rainVBO) glDeleteBuffers(1, &rainVBO);
	SDL_GL_DeleteContext(gContext);
	//Destroy window	
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;

	//Quit SDL subsystems
	SDL_Quit();
}

void render()
{
	glm::vec3 lightDir = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f));
	glm::vec3 lightPosWorld = -lightDir * 20.0f;


	float near_plane = 1.0f, far_plane = 50.0f;
	float ortho_size = 20.0f;
	glm::mat4 lightProjection = glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);
	glm::mat4 lightView = glm::lookAt(lightPosWorld, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	// ---------------- PASS 1: depth pass ----------------
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	depthShader.use();
	depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
	renderSceneDepth(depthShader, lightSpaceMatrix);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// ---------------- PASS 2: render scene ----------------
	glViewport(0, 0, 840, 680);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));

	glm::mat4 view = camera.GetViewMatrix();
	glm::mat4 proj = glm::perspective(glm::radians(camera.Zoom), 4.0f / 3.0f, 0.1f, 100.0f);
	glm::mat3 normalMat = glm::transpose(glm::inverse(model));

	
	gShader.use();
	// sconce uniforms
	gShader.setBool("sconceLightOn", sconceLightOn);
	gShader.setVec3("sconceLightPos", sconceLightPos);
	gShader.setVec3("sconceLightColor", sconceLightColor * lampIntensity);
	gShader.setFloat("sconceConstant", 1.0f);
	gShader.setFloat("sconceLinear", 0.09f);
	gShader.setFloat("sconceQuadratic", 0.032f);
	gShader.setVec3("lampEmissionColor", sconceLightColor);
	float lampStrength = (currentWeather == Weather::Night) ? 2.5f : 1.2f;
	gShader.setFloat("lampEmissionStrength", lampStrength * lampIntensity);

	gShader.setMat4("model", model);
	gShader.setMat4("view", view);
	gShader.setMat4("proj", proj);
	gShader.setMat3("normalMat", normalMat);

	// set dirLight + lightSpaceMatrix + bind shadow map
	gShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
	gShader.setVec3("dirLight.direction", lightDir);
	gShader.setVec3("dirLight.color", glm::vec3(1.0f));

	// bind depth map to texture unit 1 (matches shader sampler)
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	gShader.setInt("shadowMap", 1);

	// weather-dependent uniforms (same as before)
	switch (currentWeather)
	{
	case Weather::Sunny:
		gShader.setVec3("globalLightColor", glm::vec3(1.0f));
		gShader.setFloat("ambientStrength", 0.6f);
		gShader.setFloat("exposure", 1.0f);
		gShader.setFloat("fogDensity", 0.0f);
		break;

	case Weather::Rainy:
		gShader.setVec3("globalLightColor", glm::vec3(0.6f, 0.65f, 0.7f));
		gShader.setFloat("ambientStrength", 0.35f);
		gShader.setFloat("exposure", 0.85f);
		gShader.setVec3("fogColor", glm::vec3(0.5f, 0.55f, 0.6f));
		gShader.setFloat("fogDensity", 0.03f);
		break;

	case Weather::Night:
		gShader.setVec3("globalLightColor", glm::vec3(0.2f, 0.2f, 0.4f));
		gShader.setFloat("ambientStrength", 0.1f);
		gShader.setFloat("exposure", 0.5f);
		gShader.setVec3("fogColor", glm::vec3(0.05f, 0.05f, 0.1f));
		gShader.setFloat("fogDensity", 0.01f);
		break;
	}

	// after the switch
	rainEnabled = (currentWeather == Weather::Rainy);

	// ---- update rain particle positions (camera-centered respawn) ----
	if (rainEnabled)
	{
		// optional wind vector
		glm::vec3 wind = glm::vec3(0.5f, 0.0f, -0.3f); // tweak if you want slanting rain

		for (auto& d : rain)
		{
			// apply wind + fall
			d.position.x += wind.x * deltaTime;
			d.position.z += wind.z * deltaTime;
			d.position.y -= d.speed * deltaTime;

			// respawn relative to camera so rain is visible when inside
			if (d.position.y < camera.Position.y - 5.0f) // below camera
			{
				d.position.y = camera.Position.y + glm::linearRand(5.0f, 15.0f);
				d.position.x = camera.Position.x + glm::linearRand(-10.0f, 10.0f);
				d.position.z = camera.Position.z + glm::linearRand(-10.0f, 10.0f);
			}
		}
	}

	// ---- prepare vertex data and update GPU buffer and draw rain ----
	if (rainEnabled)
	{
		std::vector<glm::vec3> rainVerts;
		rainVerts.reserve(RAIN_COUNT * 2);

		for (auto& d : rain)
		{
			// short line per drop (motion-blur look)
			rainVerts.push_back(d.position);
			rainVerts.push_back(d.position + glm::vec3(0.0f, -0.15f, 0.0f));
		}

		if (!rainVerts.empty())
		{
			glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, rainVerts.size() * sizeof(glm::vec3), rainVerts.data());
		}

		// ---- draw rain ----
		rainShader.use();
		rainShader.setMat4("view", view);
		rainShader.setMat4("proj", proj);

		glLineWidth(1.5f);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);

		glBindVertexArray(rainVAO);
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(rainVerts.size()));

		// restore depth write
		glDepthMask(GL_TRUE);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	// Render the scene using the scene shader (samples depthMap internally)
	// We already set many uniforms above, call helper to draw the objects
	renderScene(gShader, view, proj, lightSpaceMatrix, lightDir);

	// draw light visualizer
	static float angle = 0.0f;
	angle += 45.0f * deltaTime;
	lightPos.x = cos(glm::radians(angle)) * 1.5f;
	lightPos.z = sin(glm::radians(angle)) * 1.5f;

	glUseProgram(gLightSource_1.ID);
	glm::mat4 model1 = glm::mat4(1.0f);	//transformations
	//gLightSource_1.setMat4("model", model);

	//gLightSource_1.setMat3("normalMat", normalMat);

	//lighting
	gLightSource_1.setVec3("light.diffuse", 1.0f, 1.0f, 1.0f);
	gLightSource_1.setVec3("light.position", lightPos);
	gLightSource_1.setVec3("viewPos", camera.Position);

	model1 = glm::translate(model1, lightPos);
	model1 = glm::scale(model1, glm::vec3(5.2f));
	gLightSource_1.setMat4("model", model1);
	gLightSource_1.setMat4("view", view);
	gLightSource_1.setMat4("projection", proj);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	DrawCube(gVAO);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

// ------------- Scene rendering helpers -------------
// Render objects into the depth map: use depthShader and only write depth
void renderSceneDepth(const Shader& shader, const glm::mat4& lightSpaceMatrix)
{
	// house
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.009f, 0.009f, 0.009f));
	shader.setMat4("model", model);
	shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
	gModelHouse.Draw(const_cast<Shader&>(shader));

	// pc
	glm::mat4 pc = glm::mat4(1.0f);
	pc = glm::translate(pc, glm::vec3(-6.0f, 0.0f, 5.2f));
	pc = glm::scale(pc, glm::vec3(0.004f, 0.004f, 0.004f));
	shader.setMat4("model", pc);
	gPc.Draw(const_cast<Shader&>(shader));

	// sconce (if you want it to cast shadows)
	glm::mat4 sModel = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.8f, -0.6f));
	sModel = glm::scale(sModel, glm::vec3(0.009f, 0.009f, 0.009f));
	shader.setMat4("model", sModel);
	gSconceLight.Draw(const_cast<Shader&>(shader));


	//zahalight
	glm::mat4 zaha = glm::translate(glm::mat4(1.0f), glm::vec3(-5.8f, 0.7f, 0.5f));
	zaha = glm::scale(zaha, glm::vec3(0.9f, 0.9f, 0.9f));
	shader.setMat4("model", zaha);
	zahaLight.Draw(const_cast<Shader&>(shader));
}

// Render scene normally using gShader which samples shadowMap
void renderScene(const Shader& shader, const glm::mat4& view, const glm::mat4& proj, const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir)
{
	// set uniforms commonly used by models
	shader.setMat4("view", view);
	shader.setMat4("proj", proj);
	shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
	shader.setVec3("viewPos", camera.Position);
	shader.setVec3("dirLight.direction", lightDir);
	shader.setVec3("dirLight.color", glm::vec3(1.0f));

	// house
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.009f, 0.009f, 0.009f));
	shader.setMat4("model", model);
	shader.setBool("isLamp", false);
	gModelHouse.Draw(const_cast<Shader&>(shader));

	// pc
	glm::mat4 pc = glm::mat4(1.0f);
	pc = glm::translate(pc, glm::vec3(-6.0f, 0.0f, 2.3f));
	pc = glm::scale(pc, glm::vec3(0.004f, 0.004f, 0.004f));
	shader.setMat4("model", pc);
	gPc.Draw(const_cast<Shader&>(shader));

	// sconce (lamp) - render tiny model but mark as lamp (emission)
	shader.setBool("isLamp", true);
	glm::mat4 sModel = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.8f, -0.6f));
	sModel = glm::scale(sModel, glm::vec3(0.009f, 0.009f, 0.009f));
	shader.setMat4("model", sModel);
	gSconceLight.Draw(const_cast<Shader&>(shader));
	shader.setBool("isLamp", false);


	shader.setBool("isLamp", true);
	glm::mat4 zaha = glm::translate(glm::mat4(1.0f), glm::vec3(-5.8f, 0.7f, 0.5f));
	zaha = glm::scale(zaha, glm::vec3(0.9f, 0.9f, 0.9f));
	shader.setMat4("model", zaha);
	zahaLight.Draw(const_cast<Shader&>(shader));
	shader.setBool("isLamp", false);

}

GLuint CreateCube(float width, GLuint& VBO)
{
	//each side of the cube with its own vertices to use different normals
	float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
		0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
		0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
		0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
		0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,

		0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
		0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
		0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
		0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,

		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
		0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f
	};

	GLuint VAO;
	glGenBuffers(1, &VBO);
	glGenVertexArrays(1, &VAO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); //the data comes from the currently bound GL_ARRAY_BUFFER
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_TRUE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	// note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
	// VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
	glBindVertexArray(0);

	return VAO;
}

void DrawCube(GLuint vaoID)
{
	glBindVertexArray(vaoID);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
