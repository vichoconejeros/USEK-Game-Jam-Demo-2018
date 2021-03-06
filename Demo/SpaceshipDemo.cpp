// SpaceshipDemo.cpp
// Author: @vichoconejeros
// October 2018

#include "stdafx.h"

using namespace std;

#include <stdlib.h>
#include <string>
#include <sstream>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <iostream>
#include <map>

//SDL (joystick and windows handling)
#include <SDL.h>

//SDL_Image (to load textures)
#include <SDL_Image.h>

//OpenGL (graphics stuff)
#include <gl/glew.h>
#include "VertexBufferHelper.h"

extern vertexbufferCreatorMap vertexBufferCreator;
extern vertexbufferDrawerMap vertexBufferDrawer;
void faceBufferCreator(aiMesh* mesh, FaceBuffer& fb);
void initVertexBufferHelpers();

//assimp scene pointer to store the imported asset
const struct aiScene* assimpScene = NULL;

//define map properties
#define NUM_TILES    64 
#define NUM_TILESF	(float)NUM_TILES
#define TILE_SIZE    16 
#define TILE_SIZEF	(float)TILE_SIZE
#define VERTICAL_SCALEF	56.0f
#define TIME_SCALEF	3.0f

//joystick stuff
#define JOYSTICK_MIN_SENSITIVITY	3000
#define JOYSTICK_MAX_RANGE 32767
#define TRIGGER_FACTOR	0.5f

//initial conditions stuff
//#define INITIAL_ROTATION_X	0.572f
//#define INITIAL_ROTATION_Y	0.6f
#define INITIAL_ROTATION_X	0.57f
#define INITIAL_ROTATION_Y	3.1416f*0.5f
#define DEFAULT_RADIUS		207.3f
#define INITIAL_POSITION(pos) pos[0] = 271.0f; pos[1] = 0.0f; pos[2] = 1037.0f
#define HORIZONTAL_SPEED	1.5f

float JoystickNormalize(int x) {
	return (x > JOYSTICK_MIN_SENSITIVITY || x < -JOYSTICK_MIN_SENSITIVITY) ? (min(((float)x) / (JOYSTICK_MAX_RANGE), 1.0f)) : 0.0f;
}

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

SDL_Window* window = NULL;
SDL_GLContext context = NULL;
SDL_Joystick *joystick = NULL;

class ThirdPersonCamera {
private:
	float rotationX;
	float rotationY;
	float radius;

public:

	ThirdPersonCamera() {
		rotationX = 0.0f;
		rotationY = 0.0f;
		radius = 0.0f;
	}

	void AddRotation(float XRot, float YRot) {
		rotationX += XRot;
		rotationY += YRot;
	}

	float getRotationX() { return rotationX; }
	float getRotationY() { return rotationY; }

	void GetForward(float *forward) {
		forward[0] = (float)cosf(rotationX)*cosf(rotationY);
		forward[1] = (float)sinf(rotationX);
		forward[2] = (float)cosf(rotationX)*sinf(rotationY);
	}

	void SetLookTo(float avatarPos[3]) {
		float realPosition[3];

		GetForward(realPosition);

		gluLookAt(	avatarPos[0] + realPosition[0]*radius,
					avatarPos[1] + realPosition[1]*radius,
					avatarPos[2] + realPosition[2]*radius,

					avatarPos[0],
					avatarPos[1],
					avatarPos[2],

					0, 1, 0);
	}

	void SetRadius(float r) {
		radius = r;
	}
};

class SpaceShip {
private:
	VertexBuffer*	vb;
	FaceBuffer*		fb;
	unsigned int	numVb;
	unsigned int	numFb;
	std::map<uint, uint>	texture; //meshId to texId 

	void LoadVertexes(const struct aiScene *scene) {
		for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
			unsigned int vType = 0;
			if (scene->mMeshes[meshIdx]->HasPositions()) { vType |= Attribute_Position; }
			if (scene->mMeshes[meshIdx]->HasNormals()) { vType |= Attribute_Normal; }
			if (scene->mMeshes[meshIdx]->HasTangentsAndBitangents()) { vType |= (Attribute_Tangent | Attribute_Bitangent); }
			if (scene->mMeshes[meshIdx]->HasTextureCoords(0)) { vType |= Attribute_TexCoord0; }
			if (scene->mMeshes[meshIdx]->HasTextureCoords(1)) { vType |= Attribute_TexCoord1; }
			if (scene->mMeshes[meshIdx]->HasTextureCoords(2)) { vType |= Attribute_TexCoord2; }
			if (scene->mMeshes[meshIdx]->HasTextureCoords(3)) { vType |= Attribute_TexCoord3; }
			
			vertexBufferCreator[vType](scene->mMeshes[meshIdx], vb[meshIdx]);
			faceBufferCreator(scene->mMeshes[meshIdx], fb[meshIdx]);
		}
	}
	
	void LoadMaterials(const struct aiScene *scene,std::string folder) {
		for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
			aiMaterial* mat = scene->mMaterials[scene->mMeshes[meshIdx]->mMaterialIndex];
			aiString name;

			mat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), name);
			if (name.length == 0) continue;

			std::string imagePath = folder + name.C_Str();
			cout << "loading image:" << imagePath << endl;

			SDL_Surface* textureSurface = IMG_Load(imagePath.c_str());

			unsigned int textureId;
			glGenTextures(1, &textureId);
			glBindTexture(GL_TEXTURE_2D, textureId);
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSurface->w, textureSurface->h, 0, GL_RGB, GL_UNSIGNED_BYTE, textureSurface->pixels);
			glGenerateMipmap(GL_TEXTURE_2D);

			texture[meshIdx] = textureId;

			//stay safe, unbind the texture
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

public:
	void Load(const struct aiScene *scene) {
		cout << "ship has " << scene->mNumMeshes << "meshes" << endl;
		cout << "ship has " << scene->mNumMaterials << " materials" << endl;
		cout << "ship has " << scene->mNumTextures << " textures" << endl;

		numFb = numVb = scene->mNumMeshes;
		vb = new VertexBuffer[numVb];
		fb = new FaceBuffer[numFb];

		LoadVertexes(scene);
		LoadMaterials(scene,"Models");
	}

	void Render(float *position, float* scale, float rotationY) {
		glPushMatrix();
				
		glTranslatef(position[0], position[1], position[2]);
		glScalef(scale[0], scale[1], scale[2]);
		glRotatef(270.0f - rotationY * 180.0f / 3.1416f, 0.0f, 1.0f, 0.0f);
		
		glColor3ub(255, 255, 255);

		for (unsigned int meshIdx = 0; meshIdx < numFb; meshIdx++) {
			vertexBufferDrawer[vb[meshIdx].vertexType](vb[meshIdx], fb[meshIdx], texture[meshIdx]);
		}
		
		glPopMatrix();
	}
};

class WaviGround {
private:
	float height(float time, float xf, float zf) {
		float r = sqrtf(((float)xf)*((float)xf) + ((float)zf)*((float)zf)) / (TILE_SIZEF);
		float value = ((xf != 0.0f) || (zf != 0.0f)) ? (VERTICAL_SCALEF*cosf(r+time*TIME_SCALEF)*10.0f / (r)) : VERTICAL_SCALEF;
		return value;
	}
public:

	void Render(float time) {

		glColor3ub(0, 255, 0);

		glBegin(GL_LINES);

		int iz = -NUM_TILES;
		int ix = -NUM_TILES;

		float zf = (float)iz*TILE_SIZEF; float zfpdz = zf + TILE_SIZEF;
		for (int z = iz; z < NUM_TILES; z++, zf += TILE_SIZEF, zfpdz += TILE_SIZEF) {
			float xf = (float)ix*TILE_SIZEF; float xfpdx = xf + TILE_SIZEF;
			for (int x = ix; x < NUM_TILES; x++, xf += TILE_SIZEF, xfpdx += TILE_SIZEF) {

				glVertex3f(xf	, height(time, xf	, zf	), zf);
				glVertex3f(xfpdx, height(time, xfpdx, zf	), zf);

				glVertex3f(xfpdx, height(time, xfpdx, zf	), zf);
				glVertex3f(xfpdx, height(time, xfpdx, zfpdz	), zfpdz);

				glVertex3f(xfpdx, height(time, xfpdx, zfpdz	), zfpdz);
				glVertex3f(xf	, height(time, xf	, zfpdz	), zfpdz);

				glVertex3f(xf	, height(time, xf	, zfpdz	), zfpdz);
				glVertex3f(xf	, height(time, xf	, zf	), zf);
			}
		}
		glEnd();
	}
};

class SpaceshipDemo {
private:
	//frame measurement
	int last, now, frame, fps, milis;
	float dt, dtI;

	//ship's position
	float position[3];

	//input normalized values
	float leftJoyVector[2];
	float rightJoyVector[2];
	float triggerVector[2];

	//demo elements
	ThirdPersonCamera cam;
	SpaceShip ship;
	WaviGround ground;
public:

	SpaceshipDemo() {
		last = 0; now = 0; frame = 0; dt = 0.0f; dtI = 0.0f; milis = 0; fps = 0;
		INITIAL_POSITION(position);
		cam.AddRotation(INITIAL_ROTATION_X, INITIAL_ROTATION_Y);

	}

	void LoadSpaceship(std::string path) {
		assimpScene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
		ship.Load(assimpScene);
	}

	void Delta() {
		last = now;
		now = SDL_GetTicks();
		milis += now - last;
		fps = (milis > 1000) ? frame : fps;
		frame = (milis > 1000) ? 0 : frame;		
		milis = milis % 1000;
		frame++;
		dt = (float)(now - last) / 1000.0f;
		dtI += dt;
		
		//4 lines for this...
		std::ostringstream titleStr;
		titleStr << "Spaceship Demo: " << fps << " FPS"; 
		std::string title(titleStr.str());
		SDL_SetWindowTitle(window, title.c_str());
	}

	void HandleJoystick() {
		leftJoyVector[0] = JoystickNormalize(SDL_JoystickGetAxis(joystick, 0));
		leftJoyVector[1] = JoystickNormalize(SDL_JoystickGetAxis(joystick, 1));
		rightJoyVector[0] = JoystickNormalize(SDL_JoystickGetAxis(joystick, 4));
		rightJoyVector[1] = JoystickNormalize(SDL_JoystickGetAxis(joystick, 3));
		triggerVector[0] = JoystickNormalize(SDL_JoystickGetAxis(joystick, 2));
		triggerVector[1] = JoystickNormalize(SDL_JoystickGetAxis(joystick, 5));
	}

	void MoveShip() {

		float forward[3];

		//get camera forward
		cam.GetForward(forward);
		
		//rotate around Y axis 180 degrees and set y component to 0
		forward[0] *= -1.0f;	forward[1] = 0.0f; forward[2] *= -1.0f;
		float rotY = 0.01f*leftJoyVector[0];
		float newforwardX =  cos(rotY)*forward[0] + sin(rotY)*forward[2];
		float newforwardZ = -sin(rotY)*forward[0] + cos(rotY)*forward[2];

		//set new position
		position[1] += (triggerVector[1] - triggerVector[0]) * TRIGGER_FACTOR;
		position[0] += newforwardX * HORIZONTAL_SPEED; position[2] += newforwardZ * HORIZONTAL_SPEED;

		//rotate the camera around Y axis
		cam.AddRotation(0.0f, rotY);
	}

	void MainLoop() {

		cam.SetRadius(DEFAULT_RADIUS);

		SDL_Event evento;

		while (1) {

			SDL_PollEvent(&evento);

			if (evento.type == SDL_QUIT) {
				break;
			}

			Delta();
			HandleJoystick();
			MoveShip();
			Render();
		}
	}

	void Render() {

		//OpenGL shading model
		glShadeModel(GL_SMOOTH);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

		//viewport
		glViewport(0, 0, (GLsizei)SCREEN_WIDTH, (GLsizei)SCREEN_HEIGHT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(75, SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 10000.0f);

		//clear color & depth buffers
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClearDepth(1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_TEXTURE_2D);

		//model view
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		cam.SetLookTo(position);

		float rotY = cam.getRotationY();
		float scale[] = { 0.075f, 0.075f, 0.075f }; //for the space ship
		//float scale[] = { 10.0f, 10.0f, 10.0f }; //for the crate
		
		//render objects in scene
		ship.Render(position, scale, rotY);
		ground.Render(dtI);

		//do the swap
		SDL_GL_SwapWindow(window);
	}


};

SpaceshipDemo demo;

int main(int argc, char* argv[]) {

	//init SDL (window & joystick)
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	window = SDL_CreateWindow("Spaceship Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	context = SDL_GL_CreateContext(window);
	joystick = SDL_JoystickOpen(0);

	if (joystick) {
		cout << "Opened Joystick 0" << endl;
		cout << "Name: " << SDL_JoystickNameForIndex(0) << endl;
		cout << "Number of Axes: "<< SDL_JoystickNumAxes(joystick) << endl;
		cout << "Number of Buttons: " << SDL_JoystickNumButtons(joystick) << endl;
		cout << "Number of Balls: " << SDL_JoystickNumBalls(joystick) << endl;
	}
	else {
		cout << "Couldn't open Joystick 0" << endl;
	}

	//prepare GL context for SDL
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetSwapInterval(1);

	//init OpenGL extensions
	glewInit();

	//init vertex buffers helpers
	initVertexBufferHelpers();

	//do our magic
	demo.LoadSpaceship(".\\Models\\Wraith_Raider_Starship.obj");
	//demo.LoadSpaceship(".\\Models\\Crate1.obj");
	demo.MainLoop();

	//destroy leftovers
	aiReleaseImport(assimpScene);

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	window = NULL;
	SDL_Quit();

	return 0;
}	