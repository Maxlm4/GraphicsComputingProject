//SDL Libraries
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

//OpenGL Libraries
#include <GL/glew.h>
#include <GL/gl.h>

//GML libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"

#include <SDL2/SDL_image.h>

#include "logger.h"

// objects 3D
#include "Sphere.h"
#include "Cube.h"
#include "Cylinder.h"

//libraries supplémentaires
#include "vector"
#include "math.h"
#include "stdlib.h"

//On définit une fenêtre carrée pour éviter tout problème de rotation ou scaling.
#define WIDTH     1000
#define HEIGHT    1000
#define FRAMERATE 60
#define TIME_PER_FRAME_MS  (1.0f/FRAMERATE * 1e3)
#define INDICE_TO_PTR(x) ((void*)(x))


//On définit ici les paramètres nécessaires pour créer un matériau. La couleur est inutilisée dans ce projet.
struct Material {
	glm::vec3 color;
	float ka;
	float kd;
	float ks;
	float alpha;
};

//On définit les paramètres nécessaires pour créer une lumière. On a donné une valeur par défaut à chaque paramètres car ceux de nos différentes lumières varient peu.
struct Light {
	glm::vec3 position = glm::vec3(0.f,0.4f,-46.f);
	glm::mat4 Coordinates = glm::translate(glm::mat4(1.0f), position);
	glm::vec3 color = glm::vec3(0.7f, 0.65f, 0.8f);
};

//La méthode generate() permet d'instancier les buffers associés à une figure, sa texture et sa lumière,  et les récupérer
std::vector<GLuint> generate(Geometry g, const char* source)
{
	const float* data = g.getVertices(); //get the vertices created by the cube.
	const float* normals = g.getNormals(); //Get the normal vectors
	const float* uvs = g.getUVs(); //Get the uv vectors
	int nbVertices = g.getNbVertices();

	//Convert to an RGBA8888 surface
	SDL_Surface* img = IMG_Load(source);
	SDL_Surface* rgbImg = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGBA32, 0);

	uint8_t* imgInverted = (uint8_t*)malloc(sizeof(uint8_t) * 4 * rgbImg->w*rgbImg->h);
	for (uint32_t j = 0; j < rgbImg->h; j++)
	{
		for (uint32_t i = 0; i < rgbImg->w; i++)
		{
			for (uint8_t k = 0; k < 4; k++)
			{
				uint32_t oldID = 4 * (j*rgbImg->w + i) + k;
				uint32_t newID = 4 * (j*rgbImg->w + rgbImg->w - 1 - i) + k;

				imgInverted[newID] = ((uint8_t*)(rgbImg->pixels))[oldID];
			}
		}
	}

	SDL_FreeSurface(img);

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbImg->w, rgbImg->h, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)imgInverted);
	glBindTexture(GL_TEXTURE_2D, 0);
	free(imgInverted);
	SDL_FreeSurface(rgbImg);

	GLuint buffer; //texture buffer
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, (3 + 2) * sizeof(float)*nbVertices, NULL, GL_DYNAMIC_DRAW); // 3 pour les coordonnees , 3 pour la couleur
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 3 * nbVertices, data);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 3 * nbVertices, 2 * sizeof(float)*nbVertices, uvs);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLuint buffer2; //light buffer
	glGenBuffers(1, &buffer2);
	glBindBuffer(GL_ARRAY_BUFFER, buffer2);
	glBufferData(GL_ARRAY_BUFFER, (3 + 3) * sizeof(float)*nbVertices, NULL, GL_DYNAMIC_DRAW); // 3 pour les coordonnees , 3 pour la couleur
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 3 * nbVertices, data);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 3 * nbVertices, 3 * sizeof(float)*nbVertices, normals);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	std::vector<GLuint> tab;
	tab.push_back(buffer);
	tab.push_back(textureID);
	tab.push_back(buffer2);
	return tab;
}

//getMatrix() permet d'effectuer une translation de tx en x, ty en y, tz en z et effectuer une rotation de angle radians autours de l'axe dont la valeur vaut 1
glm::mat4 getMatrix(float tx, float ty, float tz, float angle, int x, int y, int z)
{
	glm::mat4 matrix(1.0f);

	matrix = glm::translate(matrix, glm::vec3(tx, ty, tz));
	matrix = glm::rotate(matrix, angle, glm::vec3(x, y, z));

	return matrix;
}

//scaleMatrix est utile pour scaler l'objet indépendamment des translations et rotations. Permet d'éviter le rescale de tous les objets liés à l'objet que l'on souhaite rescale
glm::mat4 scaleMatrix(float sx, float sy, float sz)
{
	glm::mat4 matrix(1.0f);

	matrix = glm::scale(matrix, glm::vec3(sx, sy, sz));

	return matrix;
}

//draw permet de dessiner la figure
void draw(GLuint texture, GLuint buffer, GLuint buffer2, Geometry g, Shader* shader, glm::mat4 mvp, Material m, Light l, std::vector<GLint> glValues)
{
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glVertexAttribPointer(glValues[0], 3, GL_FLOAT, 0, 0, 0);
	glEnableVertexAttribArray(glValues[0]);
	glVertexAttribPointer(glValues[10], 2, GL_FLOAT, 0, 0, INDICE_TO_PTR(3 * sizeof(float) * g.getNbVertices()));
	glEnableVertexAttribArray(glValues[10]);
	glUniformMatrix4fv(glValues[3], 1, GL_FALSE, glm::value_ptr(mvp));
	glUniform1i(glValues[9], 0);

	glBindBuffer(GL_ARRAY_BUFFER, buffer2);
	glVertexAttribPointer(glValues[0], 3, GL_FLOAT, 0, 0, 0);
	glEnableVertexAttribArray(glValues[0]);
	glVertexAttribPointer(glValues[1], 3, GL_FLOAT, 0, 0, INDICE_TO_PTR(sizeof(float) * 3 * g.getNbVertices()));
	glEnableVertexAttribArray(glValues[1]);
	glUniformMatrix4fv(glValues[3], 1, GL_FALSE, glm::value_ptr(mvp));
	glUniformMatrix4fv(glValues[4], 1, GL_FALSE, glm::value_ptr(mvp));
	glUniform4fv(glValues[2], 1, glm::value_ptr(glm::vec4(m.ka, m.kd, m.ks, m.alpha)));
	glUniform3fv(glValues[5], 1, glm::value_ptr(m.color));
	glUniform3fv(glValues[6], 1, glm::value_ptr(l.color));
	glUniform3fv(glValues[7], 1, glm::value_ptr(l.position));
	glUniform3fv(glValues[8], 1, glm::value_ptr(glm::vec3(0.f, 0.f, 0.f)));
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDrawArrays(GL_TRIANGLES, 0, g.getNbVertices());
	glBindTexture(GL_TEXTURE_2D, 0);
}

int main(int argc, char *argv[])
{
    ////////////////////////////////////////
    //SDL2 / OpenGL Context initialization : 
    ////////////////////////////////////////
    
    //Initialize SDL2
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
    {
        ERROR("The initialization of the SDL failed : %s\n", SDL_GetError());
        return 0;
    }

	//Init the IMG component
	if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
	{
		ERROR("Could not load SDL2_image with PNG files\n");
		return EXIT_FAILURE;
	}

    //Create a Window
    SDL_Window* window = SDL_CreateWindow("VR Camera",                           //Titre
                                          SDL_WINDOWPOS_UNDEFINED,               //X Position
                                          SDL_WINDOWPOS_UNDEFINED,               //Y Position
                                          WIDTH, HEIGHT,                         //Resolution
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN); //Flags (OpenGL + Show)

    //Initialize OpenGL Version (version 3.0)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    //Initialize the OpenGL Context (where OpenGL resources (Graphics card resources) lives)
    SDL_GLContext context = SDL_GL_CreateContext(window);

    //Tells GLEW to initialize the OpenGL function with this version
    glewExperimental = GL_TRUE;
    glewInit();


    //Start using OpenGL to draw something on screen
    glViewport(0, 0, WIDTH, HEIGHT); //Draw on ALL the screen

    //The OpenGL background color (RGBA, each component between 0.0f and 1.0f)
    glClearColor(0.0, 0.0, 0.0, 1.0); //Full Black

    glEnable(GL_DEPTH_TEST); //Active the depth test

	//////////////////////////////////////////////////////////////////////////////////////////PARTIE_ELEVE////////////////////////////////////////////////////////////////////////////////////
	
	//Ici, on instancie des variables qui vont servir à l'animation
    int t = 0; //incrémenté à chaque tour de boucle

	double x = 0; //position de la balle de ping pong en 0.9 - x (voir plus bas)
	double y = 0; //position de la balle de ping pong en y (voir plus bas)
	double z = -0.3; //position de la balle de ping pong en z (voir plus bas)
	int sideBall = 0; //0 la balle se dirige vers la droite, 1 la balle se dirige vers la gauche
	//pour les variables suivantes: 0 = arrêt, 1 = avant, -1 = arière
	int shoulderMovement = 0; //rotation du bras du personnage de droite selon l'axe x
	int shoulderRotation = 0; //rotation du bras du personnage de droite selon l'axe y
	int shoulderTurning = 0; //rotation du bras du personnage de droite selon l'axe z
	int shoulderMovement2 = 0; //rotation du bras du personnage de gauche selon l'axe x
	int shoulderRotation2 = 0; //rotation du bras du personnage de gauche selon l'axe y
	int shoulderTurning2 = 0; //rotation du bras du personnage de gauche selon l'axe z

	double bodyMovement = 0; //surélévation du corps simulant un flottement ou une respiration
	float legMovement = 0.002; //mouvement rotatif ajouté aux jambes pour simuler un flottement

    //TODO
	std::vector <Geometry> listeFigures; //liste de toutes les figures créées
	std::vector <GLuint> listeBuffer; //liste des buffers texture associés aux figures
	std::vector <GLuint> listeBuffer2; //liste des buffers lumière associés aux figures
	std::vector < GLuint> listeTexture; //liste des textures assocciées aux figures
	std::vector <glm::mat4> listeMvp; //liste des matrices associées aux figures
	std::vector <glm::mat4> listeModel; // liste des matrices modèle associées aux figures
	std::vector <Material> listeMaterial; // liste des matériaux associés aux figures

	//Variables liées à la camera
	glm::vec3 cameraPos = glm::vec3(0.0, 0.0f, -43.0f);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

	//ATTENTION: on veillera à ce que tous les objets liés à une même figure soient au même index dans toutes les listes

	//on instancie la matrice de la caméra
	glm::mat4 cameraMatrix;
	cameraMatrix = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

	//pour voir en perspective
	glm::mat4 projectionMatrix = glm::perspective(20.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 100.f);


	//On instancie les différents matériaux et lumières.
	Light myLight;
	Light ballLight;
	ballLight.color = glm::vec3((rand() % 101) / 100.f, (rand() % 101) / 100.f, (rand() % 101) / 100.f);
	Material textile = { glm::vec3(1.f, 0.f, 0.f), 0.4f, 0.3f, 0.1f, 50.0f};
	Material lightingBall = { glm::vec3(1.f, 0.f, 0.f), 1.0f, 1.0f, 0.0f, 50.0f };
	Material lightingWorld = { glm::vec3(1.f, 0.f, 0.f), 0.8f, 0.0f, 0.0f, 50.0f };
	Material stone = { glm::vec3(1.f, 0.f, 0.f), 0.4f, 0.5f, 0.2f, 50.0f };
	Material trollSkin = { glm::vec3(1.f, 0.f, 0.f), 0.4f, 0.7f, 0.2f, 50.0f };
	Material plastic = { glm::vec3(1.f, 0.f, 0.f), 0.4f, 0.8f, 0.4f, 100.0f };
	Material wood = { glm::vec3(1.f, 0.f, 0.f), 0.4f, 0.4f, 0.2f, 50.0f };
	Material leather = { glm::vec3(1.f, 0.f, 0.f), 0.4f, 0.7f, 0.3f, 50.0f };

	glm::mat4 lightPosition = projectionMatrix * cameraMatrix * myLight.Coordinates;
	myLight.position = glm::vec3(lightPosition[3][0], lightPosition[3][1], lightPosition[3][2]);

	listeMaterial.push_back(textile);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(leather);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(leather);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(trollSkin);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(leather);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(leather);
	listeMaterial.push_back(plastic);
	listeMaterial.push_back(plastic);
	listeMaterial.push_back(wood);
	listeMaterial.push_back(plastic);
	listeMaterial.push_back(plastic);
	listeMaterial.push_back(wood);
	listeMaterial.push_back(stone);
	listeMaterial.push_back(textile);
	listeMaterial.push_back(stone);
	listeMaterial.push_back(stone);
	listeMaterial.push_back(lightingBall);
	listeMaterial.push_back(lightingWorld);

	/*Ici, on va créer une par une toutes les figures qui composent notre personnage

	Dans l'ordre:
	-on instancie la figure
	-on l'ajoute à la liste des figures
	-on génère ses buffers associés qu'on ajoute à la liste des buffers correspondante
	-on créé sa matrice sans prendre en compte le scaling par souci de simplification
	-on ajoute sa matrice à la liste des matrices. Attention à l'ordre des matrices. Chaque matrice doit dépendre de la matrice à sa gauche, la caméra étant le référentiel absolu

	On ne scale qu'une fois tous les objets créés afin de ne pas avoir besoin d'adapter le scale de tous les objets en fonction de celui des objets dont ils dépendent
	
	On créé un premier cylindre qui sera le corps de notre personnage, l'angle de -pi / 2 permet d'orienter le cylindre comme souhaité.
	Attention, par défaut un cylindre fait face à la caméra et ses faces plates sont invisibles.

	Les épaules, coudes, cuisses et genoux car ce sont des articulations dans notre modèle
	*/
	std::vector<GLuint> tab;

	Cylinder body(32);
	listeFigures.push_back(body);
	tab = generate(body, "Images/costar.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 bodyMatrix = getMatrix(-1.9, 0.3, -40, -M_PI / 2.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix);

	Sphere head(32, 32);
	listeFigures.push_back(head);
	tab = generate(head, "Images/TrollFace2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 headMatrix = getMatrix(0, 0, 0.55, 0.f, 0, 0, 1);
	headMatrix = glm::rotate(headMatrix, (float)(M_PI / 2), glm::vec3(1, 0, 0));
	headMatrix = glm::rotate(headMatrix, (float)(M_PI), glm::vec3(0, 0, 1));
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * headMatrix);

	Sphere shoulder1(32, 32);
	listeFigures.push_back(shoulder1);
	tab = generate(shoulder1, "Images/manche.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 shoulder1Matrix = getMatrix(-0.32, 0, 0.3, M_PI/14.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix);

	Cylinder arm1(32);
	listeFigures.push_back(arm1);
	tab = generate(arm1, "Images/manche.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 arm1Matrix = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix * arm1Matrix);

	Sphere elbow1(32, 32);
	listeFigures.push_back(elbow1);
	tab = generate(elbow1, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 elbow1Matrix = getMatrix(0, 0, -0.2, M_PI/12.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix * arm1Matrix * elbow1Matrix);

	Cylinder forearm1(32);
	listeFigures.push_back(forearm1);
	tab = generate(forearm1, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 forearm1Matrix = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix * arm1Matrix * elbow1Matrix * forearm1Matrix);
	
	Sphere shoulder2(32, 32);
	listeFigures.push_back(shoulder2);
	tab = generate(shoulder2, "Images/manche.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 shoulder2Matrix = getMatrix(0.32, 0, 0.3, M_PI / 1.9f, 1, 0, 0);
	shoulder2Matrix = glm::rotate(shoulder2Matrix, (float)-M_PI/2.f, glm::vec3(0, 1, 0));
	shoulder2Matrix = glm::rotate(shoulder2Matrix, (float)-M_PI / 2.f, glm::vec3(1, 0, 0));
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix);

	Cylinder arm2(32);
	listeFigures.push_back(arm2);
	tab = generate(arm2, "Images/manche.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 arm2Matrix = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix);

	Sphere elbow2(32, 32);
	listeFigures.push_back(elbow2);
	tab = generate(elbow2, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 elbow2Matrix = getMatrix(0, 0, -0.2, M_PI/12.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix);

	Cylinder forearm2(32);
	listeFigures.push_back(forearm2);
	tab = generate(forearm2, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 forearm2Matrix = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix);

	Cylinder thigh1(32);
	listeFigures.push_back(thigh1);
	tab = generate(thigh1, "Images/jean.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 thigh1Matrix = getMatrix(-0.15, 0.1, -0.55, M_PI/4.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix);

	Sphere knee1(32, 32);
	listeFigures.push_back(knee1);
	tab = generate(knee1, "Images/jean.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 knee1Matrix = getMatrix(0, 0, -0.2, -M_PI/4.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix * knee1Matrix);

	Cylinder leg1(32);
	listeFigures.push_back(leg1);
	tab = generate(leg1, "Images/jean.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 leg1Matrix = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix * knee1Matrix * leg1Matrix);

	Sphere foot1(32, 32);
	listeFigures.push_back(foot1);
	tab = generate(foot1, "Images/chaussure.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 foot1Matrix = getMatrix(0, 0.1, -0.2, 0.f, 0, 0, 1);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix * knee1Matrix * leg1Matrix * foot1Matrix);

	Cylinder thigh2(32);
	listeFigures.push_back(thigh2);
	tab = generate(thigh2, "Images/jean.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 thigh2Matrix = getMatrix(0.15, 0.12, -0.55, M_PI/3.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix);

	Sphere knee2(32, 32);
	listeFigures.push_back(knee2);
	tab = generate(knee2, "Images/jean.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 knee2Matrix = getMatrix(0, 0, -0.2, -M_PI / 3.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix * knee2Matrix);

	Cylinder leg2(32);
	listeFigures.push_back(leg2);
	tab = generate(leg2, "Images/jean.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 leg2Matrix = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix * knee2Matrix * leg2Matrix);

	Sphere foot2(32, 32);
	listeFigures.push_back(foot2);
	tab = generate(foot2, "Images/chaussure.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 foot2Matrix = getMatrix(0, 0.1, -0.2, 0.f, 0, 0, 1);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix * knee2Matrix * leg2Matrix * foot2Matrix);

	Cylinder body2(32);
	listeFigures.push_back(body2);
	tab = generate(body2, "Images/costar2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 bodyMatrix2 = getMatrix(1.9, 0.3, -40, -M_PI / 2.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2);

	Sphere head2(32, 32);
	listeFigures.push_back(head2);
	tab = generate(head2, "Images/TrollFace.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 headMatrix2 = getMatrix(0, 0, 0.55, 0.f, 0, 0, 1);
	headMatrix2 = glm::rotate(headMatrix2, (float)(M_PI / 2), glm::vec3(1, 0, 0));
	headMatrix2 = glm::rotate(headMatrix2, (float)(M_PI), glm::vec3(0, 0, 1));
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * headMatrix2);

	Sphere shoulder12(32, 32);
	listeFigures.push_back(shoulder12);
	tab = generate(shoulder12, "Images/manche2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 shoulder1Matrix2 = getMatrix(-0.32, 0, 0.3, M_PI / 14.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2);

	Cylinder arm12(32);
	listeFigures.push_back(arm12);
	tab = generate(arm12, "Images/manche2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 arm1Matrix2 = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2 * arm1Matrix2);

	Sphere elbow12(32, 32);
	listeFigures.push_back(elbow12);
	tab = generate(elbow12, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 elbow1Matrix2 = getMatrix(0, 0, -0.2, M_PI / 12.f , 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2 * arm1Matrix2 * elbow1Matrix2);

	Cylinder forearm12(32);
	listeFigures.push_back(forearm12);
	tab = generate(forearm12, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 forearm1Matrix2 = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2 * arm1Matrix2 * elbow1Matrix2 * forearm1Matrix2);

	Sphere shoulder22(32, 32);
	listeFigures.push_back(shoulder22);
	tab = generate(shoulder22, "Images/manche2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 shoulder2Matrix2 = getMatrix(0.32, 0, 0.3, M_PI / 1.9f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2);

	Cylinder arm22(32);
	listeFigures.push_back(arm22);
	tab = generate(arm22, "Images/manche2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 arm2Matrix2 = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2);

	Sphere elbow22(32, 32);
	listeFigures.push_back(elbow22);
	tab = generate(elbow22, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 elbow2Matrix2 = getMatrix(0, 0, -0.2, M_PI / 12.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2);

	Cylinder forearm22(32);
	listeFigures.push_back(forearm22);
	tab = generate(forearm22, "Images/skin.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 forearm2Matrix2 = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2);

	Cylinder thigh12(32);
	listeFigures.push_back(thigh12);
	tab = generate(thigh12, "Images/jean2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 thigh1Matrix2 = getMatrix(-0.15, 0.1, -0.55, M_PI / 4.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2);

	Sphere knee12(32, 32);
	listeFigures.push_back(knee12);
	tab = generate(knee12, "Images/jean2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 knee1Matrix2 = getMatrix(0, 0, -0.2, -M_PI / 4.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2 * knee1Matrix2);

	Cylinder leg12(32);
	listeFigures.push_back(leg12);
	tab = generate(leg12, "Images/jean2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 leg1Matrix2 = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2 * knee1Matrix2 * leg1Matrix2);

	Sphere foot12(32, 32);
	listeFigures.push_back(foot12);
	tab = generate(foot12, "Images/chaussure2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 foot1Matrix2 = getMatrix(0, 0.1, -0.2, 0.f, 0, 0, 1);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2 * knee1Matrix2 * leg1Matrix2 * foot1Matrix2);

	Cylinder thigh22(32);
	listeFigures.push_back(thigh22);
	tab = generate(thigh22, "Images/jean2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 thigh2Matrix2 = getMatrix(0.15, 0.12, -0.55, M_PI / 3.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2);

	Sphere knee22(32, 32);
	listeFigures.push_back(knee22);
	tab = generate(knee22, "Images/jean2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 knee2Matrix2 = getMatrix(0, 0, -0.2, -M_PI / 3.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2 * knee2Matrix2);

	Cylinder leg22(32);
	listeFigures.push_back(leg22);
	tab = generate(leg22, "Images/jean2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 leg2Matrix2 = getMatrix(0, 0, -0.2, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2 * knee2Matrix2 * leg2Matrix2);

	Sphere foot22(32, 32);
	listeFigures.push_back(foot22);
	tab = generate(foot22, "Images/chaussure2.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 foot2Matrix2 = getMatrix(0, 0.1, -0.2, 0.f, 0, 0, 1);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2 * knee2Matrix2 * leg2Matrix2 * foot2Matrix2);

	Cylinder raquette1(32);
	listeFigures.push_back(raquette1);
	tab = generate(raquette1, "Images/red.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 raquette1Matrix = getMatrix(0, 0, -0.28, -M_PI / 2.f, 1, 0, 0);
	raquette1Matrix = glm::rotate(raquette1Matrix, (float)M_PI / 2.f, glm::vec3(0, 1, 0));
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix * raquette1Matrix);

	Sphere face1(32, 32);
	listeFigures.push_back(face1);
	tab = generate(face1, "Images/red.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 face1Matrix = getMatrix(0, 0, 0, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix * raquette1Matrix * face1Matrix);

	Cylinder manche1(32);
	listeFigures.push_back(manche1);
	tab = generate(manche1, "Images/wood.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 manche1Matrix = getMatrix(0, -0.15, 0, M_PI / 2.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix * raquette1Matrix * manche1Matrix);

	Cylinder raquette2(32);
	listeFigures.push_back(raquette2);
	tab = generate(raquette2, "Images/red.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 raquette2Matrix = getMatrix(0, 0, -0.28, -M_PI / 2.f, 1, 0, 0);
	raquette2Matrix = glm::rotate(raquette2Matrix, (float)M_PI / 2.f, glm::vec3(0, 1, 0));
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2 * raquette2Matrix);

	Sphere face2(32, 32);
	listeFigures.push_back(face2);
	tab = generate(face2, "Images/red.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 face2Matrix = getMatrix(0, 0, 0, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2 * raquette2Matrix * face2Matrix);

	Cylinder manche2(32);
	listeFigures.push_back(manche2);
	tab = generate(manche2, "Images/wood.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 manche2Matrix = getMatrix(0, -0.15, 0, M_PI / 2.f, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2 * raquette2Matrix * manche2Matrix);

	Cube table = Cube();
	listeFigures.push_back(table);
	tab = generate(table, "Images/table.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 tableMatrix = getMatrix(0, 0, -40,  0 * (M_PI / 2.f), 0, 1, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * tableMatrix);

	Cube filet = Cube();
	listeFigures.push_back(filet);
	tab = generate(filet, "Images/filet.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 filetMatrix = getMatrix(0, 0.075, 0, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * tableMatrix * filetMatrix);

	Cube support = Cube();
	listeFigures.push_back(support);
	tab = generate(support, "Images/support.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 supportMatrix = getMatrix(0, -0.34, 0, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * tableMatrix * supportMatrix);

	Cube socle = Cube();
	listeFigures.push_back(socle);
	tab = generate(socle, "Images/support.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 socleMatrix = getMatrix(0, -0.36, 0, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * tableMatrix * supportMatrix * socleMatrix);

	Sphere ball(32,32);
	listeFigures.push_back(ball);
	tab = generate(ball, "Images/ball.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 ballMatrix = getMatrix(0.9, 0.4, -40, 0, 1, 0, 0);
	listeMvp.push_back(projectionMatrix * cameraMatrix * ballMatrix);

	Sphere World(32, 32);
	listeFigures.push_back(World);
	tab = generate(World, "Images/space.png");
	listeBuffer.push_back(tab[0]);
	listeTexture.push_back(tab[1]);
	listeBuffer2.push_back(tab[2]);
	glm::mat4 worldMatrix = getMatrix(0, 0, 0, M_PI, 0, 1, 0);
	listeMvp.push_back(projectionMatrix* cameraMatrix* worldMatrix);

	//on scale tous les objets

	listeMvp[0] = listeMvp[0] * scaleMatrix(0.5, 0.25, 0.8);
	listeMvp[1] = listeMvp[1] * scaleMatrix(0.3, 0.3, 0.3);
	listeMvp[2] = listeMvp[2] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[3] = listeMvp[3] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[4] = listeMvp[4] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[5] = listeMvp[5] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[6] = listeMvp[6] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[7] = listeMvp[7] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[8] = listeMvp[8] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[9] = listeMvp[9] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[10] = listeMvp[10] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[11] = listeMvp[11] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[12] = listeMvp[12] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[13] = listeMvp[13] * scaleMatrix(0.2, 0.4, 0.2);
	listeMvp[14] = listeMvp[14] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[15] = listeMvp[15] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[16] = listeMvp[16] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[17] = listeMvp[17] * scaleMatrix(0.2, 0.4, 0.2);

	listeMvp[18] = listeMvp[18] * scaleMatrix(0.5, 0.25, 0.8);
	listeMvp[19] = listeMvp[19] * scaleMatrix(0.3, 0.3, 0.3);
	listeMvp[20] = listeMvp[20] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[21] = listeMvp[21] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[22] = listeMvp[22] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[23] = listeMvp[23] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[24] = listeMvp[24] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[25] = listeMvp[25] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[26] = listeMvp[26] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[27] = listeMvp[27] * scaleMatrix(0.1, 0.1, 0.25);
	listeMvp[28] = listeMvp[28] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[29] = listeMvp[29] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[30] = listeMvp[30] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[31] = listeMvp[31] * scaleMatrix(0.2, 0.4, 0.2);
	listeMvp[32] = listeMvp[32] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[33] = listeMvp[33] * scaleMatrix(0.2, 0.2, 0.2);
	listeMvp[34] = listeMvp[34] * scaleMatrix(0.15, 0.15, 0.38);
	listeMvp[35] = listeMvp[35] * scaleMatrix(0.2, 0.4, 0.2);

	listeMvp[36] = listeMvp[36] * scaleMatrix(0.2, 0.2, 0.02);
	listeMvp[37] = listeMvp[37] * scaleMatrix(0.2, 0.2, 0.02);
	listeMvp[38] = listeMvp[38] * scaleMatrix(0.035, 0.02, 0.1);

	listeMvp[39] = listeMvp[39] * scaleMatrix(0.2, 0.2, 0.02);
	listeMvp[40] = listeMvp[40] * scaleMatrix(0.2, 0.2, 0.02);
	listeMvp[41] = listeMvp[41] * scaleMatrix(0.035, 0.02, 0.1);

	listeMvp[42] = listeMvp[42] * scaleMatrix(1.8, 0.05, 1.0);
	listeMvp[43] = listeMvp[43] * scaleMatrix(0.02, 0.15, 0.98);
	listeMvp[44] = listeMvp[44] * scaleMatrix(0.2, 0.65, 0.95);
	listeMvp[45] = listeMvp[45] * scaleMatrix(1.0, 0.1, 1.0);

	listeMvp[46] = listeMvp[46] * scaleMatrix(0.075, 0.075, 0.075);

	listeMvp[47] = listeMvp[47] * scaleMatrix(100, 100, 100);

    //From here you can load your OpenGL objects, like VBO, Shaders, etc.
    //TODO
	//On charge les fichiers relatifs aux shaders
    FILE* vertFile = fopen("Shaders/color.vert", "r");
    FILE* fragFile = fopen("Shaders/color.frag", "r");
    if (vertFile == NULL || fragFile == NULL) {
      return EXIT_FAILURE;
    }
    Shader* shader = Shader::loadFromFiles(vertFile, fragFile);


    fclose(vertFile);
    fclose(fragFile);
    if (shader == NULL)
    {
      return EXIT_FAILURE;
    }

	//////////////////////////////////////////////////////////////////////////////////////FIN_PARTIE_ELEVE////////////////////////////////////////////////////////////////////////////////////

    bool isOpened = true;

    //Main application loop
	while (isOpened)
	{
		//Time in ms telling us when this frame started. Useful for keeping a fix framerate
		uint32_t timeBegin = SDL_GetTicks();

		t++;

		//Fetch the SDL events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
				case SDL_WINDOWEVENT_CLOSE:
					isOpened = false;
					break;
				default:
					break;
				}
				break;
				//We can add more event, like listening for the keyboard or the mouse. See SDL_Event documentation for more details

			case SDL_KEYDOWN:
			{
				//Pour déplacer ou tourner la caméra
				if (event.key.keysym.sym == SDLK_UP) {
					if (cameraPos.y < 10) {
						cameraPos.y += 0.20f;
					}
				}
				if (event.key.keysym.sym == SDLK_DOWN) {
					if (cameraPos.y > -10) {
						cameraPos.y -= 0.20f;
					}
				}
				if (event.key.keysym.sym == SDLK_LEFT) {
					if (cameraPos.x < 10) {
						cameraPos.x += 0.20f;
					}
				}
				if (event.key.keysym.sym == SDLK_RIGHT) {
					if (cameraPos.x > -10) {
						cameraPos.x -= 0.20f;
					}
				}
				if (event.key.keysym.sym == SDLK_z) {
					if (cameraPos.z <= -30.11) {
						cameraPos.z += 0.11f;
					}
				}
				if (event.key.keysym.sym == SDLK_s) {
					if (cameraPos.z >= -44.89) {
						cameraPos.z -= 0.11f;
					}
				}
				if (event.key.keysym.sym == SDLK_q) {
					cameraFront.x += 0.11f;
				}
				if (event.key.keysym.sym == SDLK_d) {
					cameraFront.x -= 0.11f;
				}
				cameraMatrix = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
				glm::mat4 lightPosition = projectionMatrix * cameraMatrix * myLight.Coordinates;
				myLight.position = glm::vec3(lightPosition[3][0], lightPosition[3][1], lightPosition[3][2]); // on doit redéfinir la position de la lumière à chaque changement de caméra
			}
			}
		}


		//Clear the screen : the depth buffer and the color buffer
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		//////////////////////////////////////////////////////////////////////////////////////////PARTIE_ELEVE////////////////////////////////////////////////////////////////////////////////////

		//Simulation de renvoi de la balle de ping pong en fonction des différents paramètres décrits plus haut.
		if (sideBall == 0) {
			if (x < 0.6)
			{
				x += 0.03;
				y = abs(cos(x / 2.4 * M_PI)) * 0.5 + 0.05;
				z += 0.01;
				shoulderRotation2 = -1;
			}
			else if (x < 1.2)
			{
				x += 0.03;
				y = abs(cos(x / 2.4 * M_PI)) * 0.5 + 0.05;
				z += 0.01;
				shoulderRotation2 = 0;
				shoulderTurning2 = -1;
				shoulderRotation = 1;
			}
			else if (x < 1.8)
			{
				x += 0.03;
				y = abs(cos(2 * x / 2.4 * M_PI - M_PI / 2)) * 0.5 + 0.05;
				z += 0.01;
				shoulderMovement = 1;
				shoulderRotation = 0;
				shoulderTurning2 = 0;
			}
			else {
				sideBall = 1;
				ballLight.color = glm::vec3((rand() % 101) / 100.f, (rand() % 101) / 100.f, (rand() % 101) / 100.f);
				shoulderMovement = 0;
				shoulderRotation = 1;
			}
		}
		else {
			if (x > 1.2)
			{
				x -= 0.03;
				y = abs(cos((1.8-x) / 2.4 * M_PI)) * 0.5 + 0.05;
				z -= 0.01;
				shoulderRotation = -1;
			}
			else if (x > 0.6)
			{
				x -= 0.03;
				y = abs(cos((1.8 - x) / 2.4 * M_PI)) * 0.5 + 0.05;
				z -= 0.01;
				shoulderRotation = 0;
				shoulderTurning = -1;
				shoulderRotation2 = 1;
			}
			else if (x > 0)
			{
				x -= 0.03;
				y = abs(cos((2 * (1.8-x) / 2.4 * M_PI - M_PI / 2))) * 0.5 + 0.05;
				z -= 0.01;
				shoulderMovement2 = 1;
				shoulderRotation2 = 0;
				shoulderTurning = 0;
			}
			else {
				sideBall = 0;
				ballLight.color = glm::vec3((rand() % 101) / 100.f, (rand() % 101) / 100.f, (rand() % 101) / 100.f);
				shoulderMovement2 = 0;
				shoulderRotation2 = 1;
			}
		}

		//TODO operations on matrix
		// On réinitialise les données des figure principales (les corps, la balle, la table)

		if (t % 120 - 60 >= 0) {
			bodyMovement -= 0.0005;
			legMovement = -abs(legMovement);
		}
		else {
			bodyMovement += 0.0005;
			legMovement = +abs(legMovement);
		}

		bodyMatrix = getMatrix(-1.9, 0.3, -40, -M_PI / 2.f, 1, 0, 0);
		bodyMatrix = glm::rotate(bodyMatrix, (float)(-M_PI / 2.f), glm::vec3(0, 0, 1));
		bodyMatrix = glm::rotate(bodyMatrix, (float)(-M_PI / 12.f), glm::vec3(1, 0, 0));
		bodyMatrix = glm::translate(bodyMatrix, glm::vec3(0, 1/3.f * bodyMovement, 2/3.f * bodyMovement));

		bodyMatrix2 = getMatrix(1.9, 0.3, -40, -M_PI / 2.f, 1, 0, 0);
		bodyMatrix2 = glm::rotate(bodyMatrix2, (float)(M_PI / 2.f), glm::vec3(0, 0, 1));
		bodyMatrix2 = glm::rotate(bodyMatrix2, (float)(-M_PI / 12.f), glm::vec3(1, 0, 0));
		bodyMatrix2 = glm::translate(bodyMatrix2, glm::vec3(0, 1 / 3.f * bodyMovement, 2 / 3.f * bodyMovement));
		

		tableMatrix = getMatrix(0, 0, -40, 0 * (M_PI / 2.f), 0, 1, 0);
		ballMatrix = getMatrix(0.9 - x, y, z - 40, 0, 1, 0, 0);

		// On recalcule toutes les matrices en fonction de la nouvelle position du corps et de la rotation des articulations 
		listeMvp[0] = projectionMatrix * cameraMatrix * bodyMatrix;
		listeMvp[1] = projectionMatrix * cameraMatrix * bodyMatrix * headMatrix;
		listeMvp[2] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix;
		listeMvp[3] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix * arm1Matrix;
		listeMvp[4] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix * arm1Matrix * elbow1Matrix;
		listeMvp[5] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder1Matrix * arm1Matrix * elbow1Matrix * forearm1Matrix;
		shoulder2Matrix = glm::rotate(shoulder2Matrix, (float)(shoulderMovement * M_PI/40.f), glm::vec3(0, 1, 0));
		shoulder2Matrix = glm::rotate(shoulder2Matrix, (float)(shoulderRotation * M_PI /40.f), glm::vec3(1, 0, 0));
		shoulder2Matrix = glm::rotate(shoulder2Matrix, (float)(shoulderTurning * M_PI / 40.f), glm::vec3(0, 0, 1));
		listeMvp[6] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix;
		listeMvp[7] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix;
		listeMvp[8] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix;
		listeMvp[9] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix;
		listeMvp[10] = projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix;
		knee1Matrix = glm::rotate(knee1Matrix, legMovement, glm::vec3(1, 0, 0));
		listeMvp[11] = projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix * knee1Matrix;
		listeMvp[12] = projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix * knee1Matrix * leg1Matrix;
		listeMvp[13] = projectionMatrix * cameraMatrix * bodyMatrix * thigh1Matrix * knee1Matrix * leg1Matrix * foot1Matrix;
		listeMvp[14] = projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix;
		knee2Matrix = glm::rotate(knee2Matrix, legMovement, glm::vec3(1, 0, 0));
		listeMvp[15] = projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix * knee2Matrix;
		listeMvp[16] = projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix * knee2Matrix * leg2Matrix;
		listeMvp[17] = projectionMatrix * cameraMatrix * bodyMatrix * thigh2Matrix * knee2Matrix * leg2Matrix * foot2Matrix;

		listeMvp[18] = projectionMatrix * cameraMatrix * bodyMatrix2;
		listeMvp[19] = projectionMatrix * cameraMatrix * bodyMatrix2 * headMatrix2;
		shoulder2Matrix2 = glm::rotate(shoulder2Matrix2, (float)(shoulderMovement2 * M_PI / 40.f), glm::vec3(0, 1, 0));
		shoulder2Matrix2 = glm::rotate(shoulder2Matrix2, (float)(shoulderRotation2 * M_PI / 40.f), glm::vec3(1, 0, 0));
		shoulder2Matrix2 = glm::rotate(shoulder2Matrix2, (float)(shoulderTurning2 * M_PI / 40.f), glm::vec3(0, 0, 1));
		listeMvp[20] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2;
		listeMvp[21] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2 * arm1Matrix2;
		listeMvp[22] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2 * arm1Matrix2 * elbow1Matrix2;
		listeMvp[23] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder1Matrix2 * arm1Matrix2 * elbow1Matrix2 * forearm1Matrix2;
		listeMvp[24] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2;
		listeMvp[25] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2;
		listeMvp[26] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2;
		listeMvp[27] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2;
		listeMvp[28] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2;
		knee1Matrix2 = glm::rotate(knee1Matrix2, legMovement, glm::vec3(1, 0, 0));
		listeMvp[29] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2 * knee1Matrix2;
		listeMvp[30] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2 * knee1Matrix2 * leg1Matrix2;
		listeMvp[31] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh1Matrix2 * knee1Matrix2 * leg1Matrix2 * foot1Matrix2;
		listeMvp[32] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2;
		knee2Matrix2 = glm::rotate(knee2Matrix2, legMovement, glm::vec3(1, 0, 0));
		listeMvp[33] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2 * knee2Matrix2;
		listeMvp[34] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2 * knee2Matrix2 * leg2Matrix2;
		listeMvp[35] = projectionMatrix * cameraMatrix * bodyMatrix2 * thigh2Matrix2 * knee2Matrix2 * leg2Matrix2 * foot2Matrix2;

		listeMvp[36] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix * raquette1Matrix;
		listeMvp[37] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix * raquette1Matrix * face1Matrix;
		listeMvp[38] = projectionMatrix * cameraMatrix * bodyMatrix * shoulder2Matrix * arm2Matrix * elbow2Matrix * forearm2Matrix * raquette1Matrix * manche1Matrix;

		listeMvp[39] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2 * raquette2Matrix;
		listeMvp[40] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2 * raquette2Matrix * face2Matrix;
		listeMvp[41] = projectionMatrix * cameraMatrix * bodyMatrix2 * shoulder2Matrix2 * arm2Matrix2 * elbow2Matrix2 * forearm2Matrix2 * raquette2Matrix * manche2Matrix;

		listeMvp[42] = projectionMatrix * cameraMatrix * tableMatrix;
		listeMvp[43] = projectionMatrix * cameraMatrix * tableMatrix * filetMatrix;
		listeMvp[44] = projectionMatrix * cameraMatrix * tableMatrix * supportMatrix;
		listeMvp[45] = projectionMatrix * cameraMatrix * tableMatrix * supportMatrix * socleMatrix;

		listeMvp[46] = projectionMatrix * cameraMatrix * ballMatrix;

		listeMvp[47] = projectionMatrix * cameraMatrix * worldMatrix;

		//on oublie pas de rescale

		listeMvp[0] = listeMvp[0] * scaleMatrix(0.5, 0.25, 0.8);
		listeMvp[1] = listeMvp[1] * scaleMatrix(0.3, 0.3, 0.3);
		listeMvp[2] = listeMvp[2] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[3] = listeMvp[3] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[4] = listeMvp[4] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[5] = listeMvp[5] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[6] = listeMvp[6] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[7] = listeMvp[7] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[8] = listeMvp[8] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[9] = listeMvp[9] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[10] = listeMvp[10] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[11] = listeMvp[11] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[12] = listeMvp[12] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[13] = listeMvp[13] * scaleMatrix(0.2, 0.4, 0.2);
		listeMvp[14] = listeMvp[14] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[15] = listeMvp[15] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[16] = listeMvp[16] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[17] = listeMvp[17] * scaleMatrix(0.2, 0.4, 0.2);

		listeMvp[18] = listeMvp[18] * scaleMatrix(0.5, 0.25, 0.8);
		listeMvp[19] = listeMvp[19] * scaleMatrix(0.3, 0.3, 0.3);
		listeMvp[20] = listeMvp[20] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[21] = listeMvp[21] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[22] = listeMvp[22] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[23] = listeMvp[23] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[24] = listeMvp[24] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[25] = listeMvp[25] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[26] = listeMvp[26] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[27] = listeMvp[27] * scaleMatrix(0.1, 0.1, 0.25);
		listeMvp[28] = listeMvp[28] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[29] = listeMvp[29] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[30] = listeMvp[30] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[31] = listeMvp[31] * scaleMatrix(0.2, 0.4, 0.2);
		listeMvp[32] = listeMvp[32] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[33] = listeMvp[33] * scaleMatrix(0.2, 0.2, 0.2);
		listeMvp[34] = listeMvp[34] * scaleMatrix(0.15, 0.15, 0.38);
		listeMvp[35] = listeMvp[35] * scaleMatrix(0.2, 0.4, 0.2);

		listeMvp[36] = listeMvp[36] * scaleMatrix(0.2, 0.2, 0.02);
		listeMvp[37] = listeMvp[37] * scaleMatrix(0.2, 0.2, 0.02);
		listeMvp[38] = listeMvp[38] * scaleMatrix(0.035, 0.02, 0.1);

		listeMvp[39] = listeMvp[39] * scaleMatrix(0.2, 0.2, 0.02);
		listeMvp[40] = listeMvp[40] * scaleMatrix(0.2, 0.2, 0.02);
		listeMvp[41] = listeMvp[41] * scaleMatrix(0.035, 0.02, 0.1);

		listeMvp[42] = listeMvp[42] * scaleMatrix(1.8, 0.05, 1.0);
		listeMvp[43] = listeMvp[43] * scaleMatrix(0.02, 0.15, 0.98);
		listeMvp[44] = listeMvp[44] * scaleMatrix(0.2, 0.65, 0.95);
		listeMvp[45] = listeMvp[45] * scaleMatrix(1.0, 0.1, 1.0);

		listeMvp[46] = listeMvp[46] * scaleMatrix(0.075, 0.075, 0.075);
		listeMvp[47] = listeMvp[47] * scaleMatrix(100, 100, 100);

        //TODO rendering
        glUseProgram(shader->getProgramID());
        {
			std::vector<GLint> glValues;
			//on instancie les GLint pour l'affichage de nos figures
			GLint vPosition = glGetAttribLocation(shader->getProgramID(), "vPosition");
			glValues.push_back(vPosition);
			GLint vNormal = glGetAttribLocation(shader->getProgramID(), "vNormal");
			glValues.push_back(vNormal);
			GLint uK = glGetUniformLocation(shader->getProgramID(), "uK");
			glValues.push_back(uK);
			GLint uMVP = glGetUniformLocation(shader->getProgramID(), "uMVP");
			glValues.push_back(uMVP);
			GLint uModelView = glGetUniformLocation(shader->getProgramID(), "uModelView");
			glValues.push_back(uModelView);
			GLuint uColor = glGetUniformLocation(shader->getProgramID(), "uColor");
			glValues.push_back(uColor);
			GLuint uLightColor = glGetUniformLocation(shader->getProgramID(), "uLightColor");
			glValues.push_back(uLightColor);
			GLuint uLightPosition = glGetUniformLocation(shader->getProgramID(), "uLightPosition");
			glValues.push_back(uLightPosition);
			GLuint uCameraPosition = glGetUniformLocation(shader->getProgramID(), "uCameraPosition");
			glValues.push_back(uCameraPosition);
			glActiveTexture(GL_TEXTURE0);
			GLint uTexture = glGetUniformLocation(shader->getProgramID(), "uTexture");
			glValues.push_back(uTexture);
			GLint vUV = glGetAttribLocation(shader->getProgramID(), "vUV");
			glValues.push_back(vUV);

			//on dessine toutes nos figures. Notez l'importance d'avoir les bons index et le même nombre d'éléments dans chaque liste, pratique risquée mais ici controllée
			for (int i = 0; i < listeFigures.size(); i++)
			{
				try
				{
					if (i != 46) { //lumière classique
						draw(listeTexture[i], listeBuffer[i], listeBuffer2[i], listeFigures[i], shader, listeMvp[i], listeMaterial[i], myLight, glValues);
					}
					else { //lumière spécifique à la balle, pour donner un effet sympatique
						draw(listeTexture[i], listeBuffer[i], listeBuffer2[i], listeFigures[i], shader, listeMvp[i], listeMaterial[i], ballLight, glValues);
					}
				}
				catch (...)
				{
					return EXIT_FAILURE;
				}
			}
        }

        glUseProgram(0);

		//////////////////////////////////////////////////////////////////////////////////////FIN_PARTIE_ELEVE////////////////////////////////////////////////////////////////////////////////////

        //Display on screen (swap the buffer on screen and the buffer you are drawing on)
        SDL_GL_SwapWindow(window);

        //Time in ms telling us when this frame ended. Useful for keeping a fix framerate
        uint32_t timeEnd = SDL_GetTicks();

        //We want FRAMERATE FPS
        if(timeEnd - timeBegin < TIME_PER_FRAME_MS)
            SDL_Delay(TIME_PER_FRAME_MS - (timeEnd - timeBegin));
    }
    
    //Free everything
	delete(shader);
	for (int i = 0; i < listeBuffer.size(); i++) {
		glDeleteBuffers(1, &listeBuffer[i]);
	}
	for (int i = 0; i < listeTexture.size(); i++) {
		glDeleteTextures(1, &listeTexture[i]);
	}
    if(context != NULL)
        SDL_GL_DeleteContext(context);
    if(window != NULL)
        SDL_DestroyWindow(window);

    return 0;
}
