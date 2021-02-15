
#pragma once

#include <cmath>
#include <random>

using namespace std;

const float M_PI = 3.1415926f;

default_random_engine RE;
uniform_real_distribution<float> URD(0.0f, 1.0f);

struct Vector3
{
	Vector3(const float _x, const float _y, const float _z)
		: x(_x)
		, y(_y)
		, z(_z)
	{}

	Vector3()
		: x(0.0f)
		, y(0.0f)
		, z(0.0f)
	{}

	float x;
	float y;
	float z;
};

float dot(const Vector3& a, const Vector3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 cross(const Vector3& a, const Vector3& b)
{
	return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

struct Spherical
{
	float theta;
	float phi;
};

struct Sample
{
	Spherical spherical_coord;
	Vector3 cartesian_coord;
	float* sh_functions;
};

struct Sampler
{
	Sample* samples;
	int number_of_samplers;
};

float Random()
{
	return URD(RE);
}

void GenerateSamples(Sampler* sampler, int N)
{
	Sample* samples = new Sample[N * N];
	sampler->samples = samples;
	sampler->number_of_samplers = N * N;

	for (int i = 0; i < N; ++i)
	{
		for (int j = 0; j < N; ++j)
		{
			float a = ((float)i + Random()) / (float)N;
			float b = ((float)j + Random()) / (float)N;

			float theta = 2 * acos(sqrt(1 - a));
			float phi = 2 * M_PI * b;

			float x = sin(theta) * cos(phi);
			float y = cos(theta) * cos(phi);
			float z = cos(phi);

			int k = i * N + j;
			sampler->samples[k].spherical_coord.theta = theta;
			sampler->samples[k].spherical_coord.phi = phi;
			sampler->samples[k].cartesian_coord.x = x;
			sampler->samples[k].cartesian_coord.y = y;
			sampler->samples[k].cartesian_coord.z = z;
			sampler->samples[k].sh_functions = NULL;
			
		}
	}
}

float DoubleFactorial(int n)
{
	if (n <= 1)
		return 1;
	else
		return n * DoubleFactorial(n - 2);
}

float Legendre(int l, int m, float x)
{
	if (l == m + 1)
	{
		return x * (2 * m + 1) * Legendre(m, m, x);
	}
	else if (l == m)
	{
		return pow(-1.0f, m) * DoubleFactorial(2 * m - 1) * pow((1.0f - x * x), m / 2);
	}
	else
	{
		return (x * (2.0f * l - 1.0f) * Legendre(l - 1, m, x) - (l + m - 1) * Legendre(l - 2, m, x)) / (l - m);
	}
}

int factorial(int n)
{
	if (n <= 1)
		return 1;
	else
		return n * factorial(n - 1);
}

float K(int l, int m)
{
	float num = (float)(2 * l + 1) * factorial(l - abs(m));
	float denom = 4 * M_PI * factorial(l + abs(m));
	return sqrt(num / denom);
}

float SphericalHarmonic(int l, int m, float theta, float phi)
{
	if (m > 0)
		return sqrt(2.0f) * K(l, m) * cos(m * phi) * Legendre(l, m, cos(theta));
	else if (m < 0)
		return sqrt(2.0f) * K(l, m) * sin(-m * phi) * Legendre(l, -m, cos(theta));
	else
		return K(l, m) * Legendre(l, 0, cos(theta));
}


void PrecomputeSHFunctions(Sampler* sampler, int bands)
{
	for (int i = 0; i < sampler->number_of_samplers; ++i)
	{
		float* sh_functions = new float[bands * bands];
		sampler->samples[i].sh_functions = sh_functions;

		float theta = sampler->samples[i].spherical_coord.theta;
		float phi = sampler->samples[i].spherical_coord.phi;

		for (int l = 0; l < bands; ++l)
		{
			for (int m = -l; m <= l; ++m)
			{
				int j = l * (l + 1) + m;
				sh_functions[j] = SphericalHarmonic(l, m, theta, phi);
			}
		}
	}
}

struct Color
{
	Color(float _r, float _g, float _b)
		: r(_r)
		, g(_g)
		, b(_b)
	{}

	float r;
	float g;
	float b;
};

Color GetLightColor(float theta, float phi)
{
	float intensity = max(0.0f, 5 * cos(theta) - 4) + max(0.0f, -4 * sin(theta - M_PI) * cos(phi - 2.0f) - 3.0f);

	return Color(intensity, intensity, intensity);
}

void ProjectLightFunction(Color* coeffs, Sampler* sampler, int bands)
{
	for (int i = 0; i < bands * bands; ++i)
	{
		coeffs[i].r = 0.0f;
		coeffs[i].g = 0.0f;
		coeffs[i].b = 0.0f;
	}

	for (int i = 0; i < sampler->number_of_samplers; ++i)
	{
		float theta = sampler->samples[i].spherical_coord.theta;
		float phi = sampler->samples[i].spherical_coord.phi;

		for (int j = 0; j < bands * bands; ++j)
		{
			Color light_color = GetLightColor(theta, phi);
			float sh_function = sampler->samples[i].sh_functions[j];

			coeffs[j].r += (light_color.r * sh_function);
			coeffs[j].g += (light_color.g * sh_function);
			coeffs[j].b += (light_color.b * sh_function);
		}
	}

	float weight = 4.0f * M_PI;
	float scale = weight / sampler->number_of_samplers;

	for (int i = 0; i < bands * bands; ++i)
	{
		coeffs[i].r *= scale;
		coeffs[i].g *= scale;
		coeffs[i].b *= scale;
	}
}

struct Triangle
{
	int a;
	int b;
	int c;
};

struct Scene
{
	Vector3* vertices;
	Vector3* normals;
	int* material;
	Triangle* triangles;
	Color* albedo;
	int number_of_triangles;
};

void ProjectUnshadowed(Color** coeffs, Sampler* sampler, Scene* scene, int bands)
{
	for (int i = 0; i < scene->number_of_triangles; ++i)
	{
		for (int j = 0; j < bands * bands; ++j)
		{
			coeffs[i][j].r = 0.0f;
			coeffs[i][j].g = 0.0f;
			coeffs[i][j].b = 0.0f;
		}
	}

	for (int i = 0; i < scene->number_of_triangles; ++i)
	{
		for (int j = 0; j < sampler->number_of_samplers; ++j)
		{
			Sample& sample = sampler->samples[j];
			float cosine_term = dot(scene->normals[i], sample.cartesian_coord);

			for (int k = 0; k < bands * bands; ++k)
			{
				float sh_function = sample.sh_functions[k];
				int material_idx = scene->material[i];
				Color& albedo = scene->albedo[material_idx];

				coeffs[i][k].r += (albedo.r * sh_function * cosine_term);
				coeffs[i][k].g += (albedo.g * sh_function * cosine_term);
				coeffs[i][k].b += (albedo.b * sh_function * cosine_term);
			}
		}
	}

	float weight = 4.0f * M_PI;
	float scale = weight / sampler->number_of_samplers;

	for (int i = 0; i < scene->number_of_triangles; ++i)
	{
		for (int j = 0; j < bands * bands; ++j)
		{
			coeffs[i][j].r *= scale;
			coeffs[i][j].g *= scale;
			coeffs[i][j].b *= scale;
		}
	}
}

bool RayIntersectsTriangle(Vector3* p, Vector3* d,
	Vector3* v0, Vector3* v1, Vector3* v2)
{
	Vector3 e1(v1->x - v0->x, v1->y - v0->y, v1->z - v0->z);
	Vector3 e2(v2->x - v0->x, v2->y - v0->y, v2->z - v0->z);

	Vector3 h = cross(*d, e2);
	float a = dot(e1, h);
	if (a > -0.00001f && a < 0.00001f)
		return false;

	float f = 1.0f / a;
	Vector3 s(p->x - v0->x, p->y - v0->y, p->z - v0->z );
	float u = f * dot(s, h);
	if (u < 0.0f || u > 1.0f)
		return false;

	Vector3 q = cross(s, e1);
	float v = f * dot(*d, q);
	if (v < 0.0f || u + v > 1.0f)
		return(false);
	float t = dot(e2, q)*f;
	if (t < 0.0f)
		return(false);
	return(true);
}


bool Visibility(Scene* scene, int vertexidx, Vector3* direction)
{
	bool visible(true);
	Vector3& p = scene->vertices[vertexidx];
	for (int i = 0; i < scene->number_of_triangles; i++)
	{
		Triangle& t = scene->triangles[i];
		if ((vertexidx != t.a) && (vertexidx != t.b) && (vertexidx != t.c))
		{
			Vector3& v0 = scene->vertices[t.a];
			Vector3& v1 = scene->vertices[t.b];
			Vector3& v2 = scene->vertices[t.c];
			visible = !RayIntersectsTriangle(&p, direction, &v0, &v1, &v2);
			if (!visible)
				break;
		}
	}

	return visible;
}

void ProjectShadowed(Color** coeffs, Sampler* sampler, Scene* scene, int bands)
{
	for (int i = 0; i < scene->number_of_triangles; ++i)
	{
		for (int j = 0; j < bands * bands; ++j)
		{
			coeffs[i][j].r = 0.0f;
			coeffs[i][j].g = 0.0f;
			coeffs[i][j].b = 0.0f;
		}
	}

	for (int i = 0; i < scene->number_of_triangles; ++i)
	{
		for (int j = 0; j < sampler->number_of_samplers; ++j)
		{
			Sample& sample = sampler->samples[j];
			if (Visibility(scene, i, &sample.cartesian_coord))
			{
				float cosine_term = dot(scene->normals[i], sample.cartesian_coord);
				for (int k = 0; k < bands * bands; ++k)
				{
					float sh_function = sample.sh_functions[k];
					int material_idx = scene->material[i];
					Color& albedo = scene->albedo[material_idx];

					coeffs[i][k].r += (albedo.r * sh_function * cosine_term);
					coeffs[i][k].g += (albedo.g * sh_function * cosine_term);
					coeffs[i][k].b += (albedo.b * sh_function * cosine_term);
				}
			}
		}
	}
}
