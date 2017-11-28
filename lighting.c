/*
 * deals with lights/shading functions
 *
 *	John Amanatides, Oct 2017
 */


#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include "artInternal.h"

#define CHECKERBOARD    1
#define ZONE_PLATE      2


#define MAX_RECURSION	10

extern double	Normalize(Vector *);
extern Vector	ReflectRay(Vector, Vector);
extern int	IntersectScene(Ray *, double *, Vector *, Material *);
extern int	ShadowProbe(Ray *, double);
extern int TransmitRay(Vector, Vector, double, double, Vector *);

typedef struct LightNode {
	Point position;
	double intensity;
	double radius;
	struct LightNode *next;
} LightNode;

static LightNode *lights;
static Color	background;
static Material	currentMaterial;
static Color black= {0.0, 0.0, 0.0}, white= {1.0, 1.0, 1.0};


char *
art_Light(double x, double y, double z, double intensity, double radius)
{
	LightNode *newLight;

	if(intensity <= 0.0 || radius < 0.0)
		return "art_Light: domain error";
	newLight= (LightNode *) malloc(sizeof(LightNode));
	newLight->position.v[0]= x;
	newLight->position.v[1]= y;
	newLight->position.v[2]= z;
	newLight->intensity= intensity;
	newLight->radius= radius;
	newLight->next= lights;
	lights= newLight;

	return NULL;
}


char *
art_Material(Material material)
{
	currentMaterial= material; /* should really check for mistakes */
	return NULL;
}


Material
GetCurrentMaterial(void)
{
	return currentMaterial;
}


char *
art_Background(Color color)
{
	if(color.v[0] < 0.0 || color.v[0] > 1.0 || color.v[1] < 0.0 || color.v[1] > 1.0 || color.v[2] < 0.0 || color.v[2] > 1.0)
		return "art_Background: domain error";
	background= color;
	return NULL;
}

/* for A4 */
static Color
Texture(Material *material, Point position)
{
	int funnySum;
	double EPSILON= 0.0001;
	double contribution;
	Color result;

	switch(material->texture) {

	case CHECKERBOARD:
		funnySum= floor(position.v[0]+EPSILON)
			+ floor(position.v[1]+EPSILON)
			+ floor(position.v[2]+EPSILON);
		if(funnySum % 2)
			return white;
		else    return material->col;
	case ZONE_PLATE:
		contribution= 0.5*cos(DOT(position, position))+0.5;
		TIMES(result, material->col, contribution);
		return result;
	default:
	return material->col;
	}
}

static Vector
reflect(Vector inc, Vector normal){
	Vector out;

	double factor=DOT(inc,normal);
	factor=factor*2;
	Vector temp;
	TIMES(temp,normal,factor);
	MINUS(out,inc,temp);

	return 	out;
}


/*
 * a simple shader
 */
static Color
ComputeRadiance(Ray *ray, double t, Vector normal, Material material)
{
	(void) Normalize(&normal);

	LightNode* light = lights;

	Point intersection;
	TIMES(intersection, ray->direction, t);
	PLUS(intersection, intersection, ray->origin);


	Color diffuseColor = black;
	Color specularColor = black;
	Material materialColor = material;
	Color textureColor = Texture(&materialColor, intersection);

	double intensity = 0;
	while (light != NULL) {
		Vector lightRay;
		MINUS(lightRay, light->position, intersection);
		double lightLength = Normalize(&lightRay);

		Ray ShadowRay;
		ShadowRay.direction = lightRay;
		ShadowRay.origin = intersection;

		intensity += light->intensity / pow(lightLength - light->radius, 2);

		int isInShadow = ShadowProbe(&ShadowRay, lightLength);
		if (!isInShadow) {
			Color currentDiffuse = black;
			TIMES(currentDiffuse, textureColor, material.Kd);
			if (DOT(normal, lightRay) > 0) {
				TIMES(
					currentDiffuse,
					currentDiffuse,
					DOT(normal, lightRay) * intensity);
			}
			PLUS(diffuseColor, diffuseColor, currentDiffuse);

			Vector reflected = ReflectRay(lightRay, normal);
			double specDOT = DOT(reflected, ray->direction);
			double specPOW = 0;
			if (specDOT < 0) {
				specPOW = 0;
			} else {
				specPOW = pow(specDOT, material.n);
			}
			Color currentSpec = black;
			TIMES(currentSpec, white, specPOW * intensity * material.Ks);
			PLUS(specularColor, specularColor, currentSpec);
		}
		light = light->next;
	}

	Color ambientColor;
	TIMES(ambientColor, textureColor, material.Ka * intensity);

	Color fColor = black;
	PLUS(fColor, ambientColor, diffuseColor);
	PLUS(fColor, fColor, specularColor);
	
	return fColor;
}


Color
GetRadiance(Ray *ray)
{
	double t;
	Vector normal;
	Material material;

	if(IntersectScene(ray, &t, &normal, &material) == HIT)
		return ComputeRadiance(ray, t, normal, material);
	else	return background;
}


void InitLighting()
{
	Material material;

	material.col= white;
	material.Ka= 0.2;
	material.Kd= 0.6;
	material.Ks= 0.7;
	material.n= 50.0;
	material.Kr= 0.0;
	material.Kt= 0.0;
	material.index= 1.0;
	(void) art_Material(material);
	(void) art_Background(black);

	lights= NULL;
}


void FinishLighting()
{
	LightNode *node;

	while(lights) {
		node= lights;
		lights= lights->next;

		free((void *) node);
	}
}
