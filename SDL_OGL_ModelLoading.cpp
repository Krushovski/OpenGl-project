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

//rainEnabled = (currentWeather == Weather::Rainy);  inside render

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

//The window we'll be rendering to
SDL_Window* gWindow = NULL;

//OpenGL context
SDL_GLContext gContext;

Shader gShader, gLightSource_1, rainShader;
Model gModel, gModelHouse, gSconceLight;

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

//event handlers
void HandleKeyDown(const SDL_KeyboardEvent& key);
void HandleMouseMotion(const SDL_MouseMotionEvent& motion);
void HandleMouseWheel(const SDL_MouseWheelEvent& wheel);

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
	switch (key.keysym.sym)
	{
	case SDLK_w:
		camera.ProcessKeyboard(FORWARD, deltaTime);
		break;
	case SDLK_s:
		camera.ProcessKeyboard(BACKWARD, deltaTime);
		break;
	case SDLK_a:
		camera.ProcessKeyboard(LEFT, deltaTime);
		break;
	case SDLK_d:
		camera.ProcessKeyboard(RIGHT, deltaTime);
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
	rainShader.Load("./shaders/rain.vert",
		"./shaders/rain.frag");
	gVAO = CreateCube(1.0f, gVBO);

	//gModel.LoadModel("./models/casa/casa moderna.obj");
	/*gModel.LoadModel("./models/nanosuit/nanosuit.obj");
	gModelGasTank.LoadModel("./models/gas_tank/gas_tanck27_lod0.obj");*/
	gModelHouse.LoadModel("./models/house/house_interior.obj");
	gSconceLight.LoadModel("./models/light/eb_sconce_light_01.obj");
	gModelHouse.GetAABBMin();
	gModelHouse.GetAABBMax();

	// compute world-space AABB for the house and assign bounds to camera
	
	glm::vec3 localMin = gModelHouse.GetAABBMin();
	glm::vec3 localMax = gModelHouse.GetAABBMax();

	// house model transform used in render()
	glm::mat4 houseModel = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, 0.0f));
	houseModel = glm::scale(houseModel, glm::vec3(0.009f, 0.009f, 0.009f));

	// transform all 8 corners and compute world AABB
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

	glm::vec3 worldMin(FLT_MAX), worldMax(-FLT_MAX);
	for (int i = 0; i < 8; ++i) {
		glm::vec4 wc = houseModel * glm::vec4(corners[i], 1.0f);
		worldMin.x = std::min(worldMin.x, wc.x);
		worldMin.y = std::min(worldMin.y, wc.y);
		worldMin.z = std::min(worldMin.z, wc.z);
		worldMax.x = std::max(worldMax.x, wc.x);
		worldMax.y = std::max(worldMax.y, wc.y);
		worldMax.z = std::max(worldMax.z, wc.z);
	}

	InitRain();
	InitRainBuffers();

	// optional: set a slightly smaller interior bounds if the AABB touches outer walls.
	float margin = 0.15f; // tweak as needed
	camera.SetAABBBounds(worldMin, worldMax, margin);
	
	//gVAO = CreateCube(1.0f, gVBO, gEBO);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //other modes GL_FILL, GL_POINT

	return success;
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
	glDeleteVertexArrays(1, &gVAO);
	glDeleteBuffers(1, &gVBO);

	//Delete OGL context
	SDL_GL_DeleteContext(gContext);
	//Destroy window	
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;

	//Quit SDL subsystems
	SDL_Quit();
}

void render()
{
	//Clear color buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 model = glm::mat4(1.0f);
//	model = glm::rotate(model, glm::radians(30.0f), glm::vec3(0, 0, 1));
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	//depending on the model size, the model may have to be scaled up or down to be visible
//  model = glm::scale(model, glm::vec3(0.001f, 0.001f, 0.001f));
	model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));

	glm::mat4 view = camera.GetViewMatrix();
	glm::mat4 proj = glm::perspective(glm::radians(camera.Zoom), 4.0f / 3.0f, 0.1f, 100.0f);

	glm::mat3 normalMat = glm::transpose(glm::inverse(model));
	
	glUseProgram(gShader.ID);
	// --- send sconce uniforms ---
	gShader.setBool("sconceLightOn", sconceLightOn);             // if setBool exists
	// fallback if no setBool: gShader.setInt("sconceLightOn", sconceLightOn ? 1 : 0);
	gShader.setVec3("sconceLightPos", sconceLightPos);
	gShader.setVec3("sconceLightColor", sconceLightColor);
	gShader.setFloat("sconceConstant", 1.0f);
	gShader.setFloat("sconceLinear", 0.09f);
	gShader.setFloat("sconceQuadratic", 0.032f);

	// emission defaults
	gShader.setVec3("lampEmissionColor", sconceLightColor);
	float lampStrength = (currentWeather == Weather::Night) ? 2.5f : 1.2f;
	gShader.setFloat("lampEmissionStrength", lampStrength);


	// ensure isLamp is false for normal models:
	gShader.setBool("isLamp", false); // or setInt(..., 0)

	//transformations
	gShader.setMat4("model", model);
	gShader.setMat4("view", view);
	gShader.setMat4("proj", proj);
	gShader.setMat3("normalMat", normalMat);

	//lighting
	gShader.setVec3("light.diffuse", 1.0f, 1.0f, 1.0f);
	gShader.setVec3("light.position", lightPos);
	gShader.setVec3("viewPos", camera.Position);


	model = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.009f, 0.009f, 0.009f));
	gShader.setMat4("model", model);


	switch (currentWeather)
	{
	case Weather::Sunny:
		gShader.setVec3("globalLightColor", glm::vec3(1.0f));
		gShader.setFloat("ambientStrength", 0.6f);
		gShader.setFloat("exposure", 1.0f);

		// no fog
		gShader.setFloat("fogDensity", 0.0f);
		break;

	case Weather::Rainy:
		gShader.setVec3("globalLightColor", glm::vec3(0.6f, 0.65f, 0.7f));
		gShader.setFloat("ambientStrength", 0.35f);
		gShader.setFloat("exposure", 0.85f);

		// fog ON
		gShader.setVec3("fogColor", glm::vec3(0.5f, 0.55f, 0.6f));
		gShader.setFloat("fogDensity", 0.03f);
		

		break;

	case Weather::Night:
		gShader.setVec3("globalLightColor", glm::vec3(0.2f, 0.2f, 0.4f));
		gShader.setFloat("ambientStrength", 0.1f);
		gShader.setFloat("exposure", 0.5f);

		// light fog or none (your choice)
		gShader.setVec3("fogColor", glm::vec3(0.05f, 0.05f, 0.1f));
		gShader.setFloat("fogDensity", 0.01f);
		break;
	}
	// after the switch(currentWeather) { ... } block:
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

	// ---- prepare vertex data and update GPU buffer ----
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

		// safety: don't upload zero bytes
		if (!rainVerts.empty())
		{
			glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, rainVerts.size() * sizeof(glm::vec3), rainVerts.data());
		}

		// ---- draw rain ----
		rainShader.use();
		rainShader.setMat4("view", view);
		rainShader.setMat4("proj", proj);

		// nice visibility tweaks:
		glLineWidth(1.5f);            // make streaks thicker
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// keep depth test on, but don't write to depth buffer (so translucent streaks render nicely)
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE); // prevent rain from occluding subsequent draws

		glBindVertexArray(rainVAO);
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(rainVerts.size()));

		// restore depth write
		glDepthMask(GL_TRUE);
		// optional: glLineWidth(1.0f);

		// unbind
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	gModelHouse.Draw(gShader);

	//                  draw gSconceLight
	gShader.setBool("isLamp", true);
	model = glm::translate(glm::mat4(1.0f), glm::vec3(-6.0f, 0.8, -0.6f));
	model = glm::scale(model, glm::vec3(0.009f, 0.009f, 0.009f));
	gShader.setMat4("model", model);
	gSconceLight.Draw(gShader);
	gShader.setBool("isLamp", false);

	//------------- code for the lights ---------------------
	
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

	DrawCube(gVAO);

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