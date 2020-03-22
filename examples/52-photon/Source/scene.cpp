#include "../Include/scene.h"
#include "../Include/image.h"
#include "../Include/sphere.h"
#include "../Include/matrix.h"
#include <cfloat>
#include <iostream>
#include <thread>

void printProgressBar(unsigned int pixel, unsigned int total)
{
	int percentCompleted = int((float)pixel / (float)total * 100.0f);

	cout << '[';
	for (int i = 0; i < round(percentCompleted / 1.5); ++i)
	{
		cout << '=';
	}
	for (int i = 0; i < static_cast<int>(100 / 1.5) - round(percentCompleted / 1.5); ++i)
	{
		cout << ' ';
	}
	cout << "] " << percentCompleted << "% \r" << std::flush;
}

Image* Scene::Render() const
{
	Image* rendered = new Image(m_camera->GetWidth(), m_camera->GetHeight());
	Vector currentPixel = m_camera->GetFirstPixel();
	Vector currentRow = currentPixel;
	Vector advanceX(m_camera->GetRight() * m_camera->GetPixelSize());
	Vector advanceY(m_camera->GetUp * m_camera->GetPixelSize());

	for (int i = 0; i < m_camera->GetHeight(); ++i)
	{
		printProgressBar(i, m_camera->GetHeight());
		for (int j = 0; j < m_camera->GetWidth(); ++j)
		{
			currentPixel += advanceX;
			(*rendered)[i][j] = GetLightRayColor(Ray(m_camera->GetFocalPoint(), currentPixel), m_specularSteps);
		}
		currentRow -= advanceY;
		currentPixel = currentRow;
	}

	printProgressBar(1, 1);

	return rendered;
}

Image* Scene::RenderMultiThread(const unsigned int threadCount) const
{
	Image *img = new Image(m_camera->GetWidth(), m_camera->GetHeight());

	printProgressBar(0, 1);

	RenderPixelRange(img);

	printProgressBar(1, 1);

	return img;
}

void Scene::RenderPixelRange(Image* img) const
{
	const Vector firstPixel = m_camera->GetFirstPixel();
	Vector advanceX(m_camera->GetRight() * m_camera->GetPixelSize());
	Vector advanceY(m_camera->GetUp() * m_camera->GetPixelSize());

	Vector currentPixel;
	for (int i = 0; i < m_camera->GetHeight(); ++i)
	{
		currentPixel = firstPixel - advanceY * i;
		for (int j = 0; j < m_camera->GetWidth(); ++j)
		{
			currentPixel += advanceX;
			(*img)[i][j] = GetLightRayColor(Ray(m_camera->GetFocalPoint(), currentPixel), m_specularSteps);
		}

		printProgressBar(i, (int)m_camera->GetHeight());
	}
}

void Scene::EmitPhotons()
{
	for (auto light : m_lightSource)
	{
		for (Vector pointLight : light->GetLights())
		{
			PoseTransformationMatrix fromLocalToGlobal = PoseTransformationMatrix::GetPoseTransformation(pointLight, Vector(0, 0, 1));

			for (int i = 0; i < m_photonEmitted / light->GetLights().size() / m_lightSource.size(); ++i)
			{
				float inclination, azimuth;
				tie(inclination, azimuth) = UniformSphereSampling();
				Vector localRay(sin(inclination) * cos(azimuth), sin(inclination) * cos(azimuth), cos(inclination));
				ColoredRay lightRay(pointLight, fromLocalToGlobal * localRay, light->GetBaseColor() / m_photonEmitted / light->GetLights().size() * 4 * PI);
				PhotonInteraction(lightRay, false);
			}
		}
	}
}

void Scene::PhotonInteraction(const ColoredRay& ray, bool save)
{
	float mint_shape = FLT_MAX;
	Shape* nearestShape = nullptr;

	for (int i = 0; i < m_shapes.size(); ++i)
	{
		m_shapes[i]->Intersect(ray, mint_shape, nearestShape, m_shapes[i]);
	}

	if (mint_shape == FLT_MAX)
		return;

	GeometryInteraction(ray, nearestShape, ray.GetScaledPosition(mint_shape), save);
}

void Scene::GeometryInteraction(const ColoredRay& lightRay, const Shape* shape, const Vector& intersection, bool save)
{
	auto mat = shape->GetMaterial();
	save &= !((mat->GetDiffuse(intersection) == BLACK) && ((mat->GetSpecular() != BLACK) | (mat->GetTransmittance() != BLACK)));

	float cosine = abs(lightRay.GetDirection().DotProduct(shape->GetNormal(intersection)));
	ColoredRay in(lightRay.GetPosition(), lightRay.GetDirection(), lightRay.GetColor() * cosine);

	if (save)
	{
		m_diffusePhotonMap.Store(intersection, Photon(in));
	}

	ColoredRay bouncedRay;
	bool isAlive = shape->RussianRoulette(in, intersection, bouncedRay);
	if (isAlive)
		PhotonInteraction(bouncedRay, true);
}

Color Scene::GetLightRayColor(const Ray& lightRay, const int specularSteps) const
{
	if (specularSteps == 0)
		return BLACK;

	float mint = FLT_MAX;
	Shape* nearestShape;
	for (int i = 0; i < m_shapes.size(); ++i)
	{
		m_shapes[i]->Intersect(lightRay, mint, nearestShape, m_shapes[i]);
	}

	if (mint == FLT_MAX)
		return BLACK;

	Vector intersection(lightRay.GetScaledPosition(mint));
	Vector normal = nearestShape->GetVisibleNormal(intersection, lightRay);
	Color emittedLight = nearestShape->GetEmittedLight();

	return (DirectLight(intersection, normal, lightRay, *nearestShape) + SpecularLight(intersection, normal, lightRay, *nearestShape, specularSteps) + GeometryEstimateRadiance(intersection, normal, lightRay, *nearestShape) + emittedLight) * PathTransmittance(lightRay, mint);
}

Color Scene::DirectLight(const Vector& point, const Vector& normal, const Ray& from, const Shape& shape) const
{
	Color ret = BLACK;

	for (int i = 0; i < m_lightSource.size(); ++i)
	{
		vector<Vector> lights = m_lightSource[i]->GetLights();
		for (int j = 0; j < lights.size(); ++j)
		{
			Ray lightRay = Ray(point, lights[j]);
			if (!IsShadow(lightRay, lights[j]))
			{
				float multiplier = lightRay.GetDirection().DotProduct(normal);
				if (multiplier > 0.0f)
				{
					ret += m_lightSource[i]->GetColor(point) * shape.GetMaterial()->PhongBRDF(from.GetDirection() * -1.0f, lightRay.GetDirection(), normal, point) * multiplier * PathTransmittance(Ray(point, lights[j]), point.Distance(lights[j]));
				}
			}
		}
	}

	return ret;
}

Color Scene::SpecularLight(const Vector& point, const Vector& normal, const Ray& in, const Shape& shape, const int specularSteps) const
{
	Color ret = BLACK;

	if (specularSteps <= 0)
		return ret;

	if (shape.GetMaterial()->GetReflectance() != BLACK)
	{
		Vector reflectedDir = Shape::Reflect(in.GetDirection(), normal);
		Ray reflectedRay = Ray(point, reflectedDir);

		ret += GetLightRayColor(reflectedRay, specularSteps - 1) * shape.GetMaterial()->GetReflectance();
	}

	if (shape.GetMaterial()->GetTransmittance() != BLACK)
	{
		Ray refractedRay = shape.Refract(in, point, normal);
		ret += GetLightRayColor(refractedRay, specularSteps - 1) * shape.GetMaterial()->GetTransmittance();
	}

	return ret;
}

Color Scene::GeometryEstimateRadiance(const Vector& point, const Vector& normal, const Ray& in, const Shape& shape) const
{
	if ((shape.GetMaterial()->GetDiffuse(point) == BLACK) && (shape.GetMaterial()->GetSpecular() == BLACK))
		return BLACK;

	Color ret = BLACK;

	vector<const Node*> nodeList;
	float radius;
	m_diffusePhotonMap.Find(point, m_photonsNeighbours, nodeList, radius);

	for (auto nodeIt = nodeList.begin(); nodeIt < nodeList.end(); ++nodeIt)
	{
		Photon tmpPhoton = (*nodeIt)->GetPhoton();
		float cosine = tmpPhoton.GetIncidence().DotProduct(normal);
		if (cosine < 0.0f)
		{
			ret += tmpPhoton.GetFlux() * shape.GetMaterial()->PhongBRDF(in.GetDirection() * -1.0f, tmpPhoton.GetIncidence(), normal, point) * GaussianKernel(point, (*nodeIt)->GetPoint(), radius);
		}
	}

	return ret / Sphere::Area(radius);
}

float Scene::GaussianKernel(const Vector& point, const Vector& photon, const float radius)
{
	constexpr static float ALPHA = 0.918f, BETA = 1.953f;
	float distance = point.Distance(photon);
	return ALPHA * (1 - ((1 - exp(-BETA * (distance*distance / 2 * radius*radius))) / (1 - exp(-BETA))));
}

float Scene::SilvermanKernel(const float x)
{
	return 3 / PI * pow(1 - x * x, 2);
}

float Scene::PathTransmittance(const Ray &lightRay, float tIntersection) const
{
	return 1.0f;
}

bool Scene::IsShadow(const Ray &lightRay, const Vector &light) const
{
	float tLight = lightRay.GetPosition().Distance(light);
	for (unsigned int i = 0; i < m_shapes.size(); ++i)
	{
		float tShape = m_shapes[i]->Intersect(lightRay);
		if (tShape < tLight) return true;
	}

	return false;
}
