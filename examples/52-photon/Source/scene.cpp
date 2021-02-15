#include "../Include/scene.h"
#include "../Include/image.h"
#include "../Include/sphere.h"
#include "../Include/matrix.h"
#include "bx/bx.h"
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
	Vector advanceY(m_camera->GetUp() * m_camera->GetPixelSize());

	for (int i = 0; i < m_camera->GetHeight(); ++i)
	{
		printProgressBar(i, m_camera->GetHeight());
		for (int j = 0; j < m_camera->GetWidth(); ++j)
		{
			currentPixel += advanceX;
			(*rendered)[i][j] = GetLightRayColor(PhotonRay(m_camera->GetFocalPoint(), currentPixel), m_specularSteps);
		}
		currentRow -= advanceY;
		currentPixel = currentRow;
	}

	printProgressBar(1, 1);

	return rendered;
}

Image* Scene::RenderMultiThread() const
{
	const unsigned int MAX_CORES = thread::hardware_concurrency();
	Image *img = new Image(m_camera->GetWidth(), m_camera->GetHeight());

	vector<vector<int> > linesPerThreads(MAX_CORES);

	for (int i = 0; i < m_camera->GetHeight(); ++i)
	{
		linesPerThreads[i % MAX_CORES].push_back(i);
	}

	vector<thread> threads(MAX_CORES);
	for (unsigned int i = 0; i < MAX_CORES; ++i)
	{
		threads[i] = thread(&Scene::RenderPixelRange, this, img, linesPerThreads[i]);
	}

	for (unsigned int i = 0; i < MAX_CORES; ++i)
	{
		threads[i].join();
	}

	//RenderPixelRange(img);


	return img;
}

void Scene::RenderPixelRange(Image* img, const vector<int>& lines) const
{
	const Vector firstPixel = m_camera->GetFirstPixel();
	Vector advanceX(m_camera->GetRight() * m_camera->GetPixelSize());
	Vector advanceY(m_camera->GetUp() * m_camera->GetPixelSize());

	Vector currentPixel;
	for (int i = 0; i < lines.size(); ++i)
	{
		int currentLine = lines[i];
		currentPixel = firstPixel - advanceY * (float)currentLine;
		for (int j = 0; j < m_camera->GetWidth(); ++j)
		{
			currentPixel += advanceX;
			(*img)[currentLine][j] = GetLightRayColor(PhotonRay(m_camera->GetFocalPoint(), currentPixel), m_specularSteps);
		}

		//printProgressBar(i, (int)m_camera->GetHeight());
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
				ColoredRay lightRay(pointLight, fromLocalToGlobal * localRay, light->GetBaseColor() / (float)m_photonEmitted / float(light->GetLights().size()) * 4.0f * PI);

				Color tmpColor = light->GetBaseColor() / (float)m_photonEmitted / float(light->GetLights().size()) * 4.0f * PI;
				if (tmpColor.Isnan())
				{
					std::cout << "Photon::Flux is nan" << std::endl;
				}
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
	save &= !((mat->GetDiffuse(intersection) == BLACK) & ((mat->GetSpecular() != BLACK) | (mat->GetTransmittance() != BLACK)));

	float cosine = abs(lightRay.GetDirection().DotProduct(shape->GetNormal(intersection)));
	ColoredRay in(lightRay.GetPosition(), lightRay.GetDirection(), lightRay.GetColor() * cosine);

	if (save)
	{
		if ((lightRay.GetColor() * cosine).Isnan())
		{
			std::cout << "Photon::Flux is nan" << std::endl;
		}
		m_diffusePhotonMap.Store(intersection, Photon(in));
	}

	ColoredRay bouncedRay;
	bool isAlive = shape->RussianRoulette(in, intersection, bouncedRay);
	if (isAlive)
		PhotonInteraction(bouncedRay, true);
}

Color Scene::GetLightRayColor(const PhotonRay& lightRay, const int specularSteps) const
{
	if (specularSteps == 0)
		return BLACK;

	float mint = FLT_MAX;
	Shape* nearestShape = nullptr;
	for (int i = 0; i < m_shapes.size(); ++i)
	{
		m_shapes[i]->Intersect(lightRay, mint, nearestShape, const_cast<Shape*>(m_shapes[i]));
	}

	if (mint == FLT_MAX)
		return BLACK;

	Vector intersection(lightRay.GetScaledPosition(mint));
	Vector normal = nearestShape->GetVisibleNormal(intersection, lightRay);
	Color emittedLight = nearestShape->GetEmittedLight();

	return (DirectLight(intersection, normal, lightRay, *nearestShape) + SpecularLight(intersection, normal, lightRay, *nearestShape, specularSteps) + GeometryEstimateRadiance(intersection, normal, lightRay, *nearestShape) + emittedLight) * PathTransmittance(lightRay, mint);

	//Vector n = normal.Normalize();
	//n = n * 0.5f + Vector(0.5f, 0.5f, 0.5f);

	//return Color(n.GetX(), n.GetY(), n.GetZ());
}

Color Scene::GetRayDepth(const PhotonRay& lightRay) const
{
	float mint = FLT_MAX;
	Shape* nearestShape = nullptr;
	for (int i = 0; i < m_shapes.size(); ++i)
	{
		m_shapes[i]->Intersect(lightRay, mint, nearestShape, const_cast<Shape*>(m_shapes[i]));
	}

	if (mint == FLT_MAX)
		return BLACK;

	return Color(mint, mint, mint);
}

Image* Scene::RenderSceneDepth() const
{
	Image *img = new Image(m_camera->GetWidth(), m_camera->GetHeight());

	const Vector firstPixel = m_camera->GetFirstPixel();
	Vector advanceX(m_camera->GetRight() * m_camera->GetPixelSize());
	Vector advanceY(m_camera->GetUp() * m_camera->GetPixelSize());

	Color maxColor(FLT_MIN, FLT_MIN, FLT_MIN);

	Vector currentPixel;
	for (int i = 0; i < m_camera->GetHeight(); ++i)
	{
		int currentLine = i;
		currentPixel = firstPixel - advanceY * (float)currentLine;
		for (int j = 0; j < m_camera->GetWidth(); ++j)
		{
			currentPixel += advanceX;
			Color currColor = GetRayDepth(PhotonRay(m_camera->GetFocalPoint(), currentPixel));
			if (currColor != BLACK)
			{
				if (currColor.GetR() > maxColor.GetR())
				{
					maxColor = currColor;
				}
			}
			
			(*img)[currentLine][j] = currColor;
		}
	}

	for (int i = 0; i < m_camera->GetHeight(); ++i)
	{
		for (int j = 0; j < m_camera->GetWidth(); ++j)
		{
			(*img)[i][j] = (*img)[i][j] / maxColor.GetR();
		}
	}

	return img;
}

Color Scene::DirectLight(const Vector& point, const Vector& normal, const PhotonRay& from, const Shape& shape) const
{
	Color ret = BLACK;

	for (int i = 0; i < m_lightSource.size(); ++i)
	{
		vector<Vector> lights = m_lightSource[i]->GetLights();
		for (int j = 0; j < lights.size(); ++j)
		{
			PhotonRay lightRay = PhotonRay(point, lights[j]);
			if (!IsShadow(lightRay, lights[j]))
			{
				float multiplier = lightRay.GetDirection().DotProduct(normal);
				multiplier *= -1.0f;
				if (multiplier > 0.0f)
				{
					ret += m_lightSource[i]->GetColor(point) * shape.GetMaterial()->PhongBRDF(from.GetDirection() * -1.0f, lightRay.GetDirection(), normal, point) * multiplier * PathTransmittance(PhotonRay(point, lights[j]), point.Distance(lights[j]));
				}
			}
		}
	}

	return ret;
}

Color Scene::SpecularLight(const Vector& point, const Vector& normal, const PhotonRay& in, const Shape& shape, const int specularSteps) const
{
	Color ret = BLACK;

	if (specularSteps <= 0)
		return ret;

	if (shape.GetMaterial()->GetReflectance() != BLACK)
	{
		Vector reflectedDir = Shape::Reflect(in.GetDirection(), normal);
		PhotonRay reflectedRay = PhotonRay(point, reflectedDir);

		ret += GetLightRayColor(reflectedRay, specularSteps - 1) * shape.GetMaterial()->GetReflectance();
	}

	if (shape.GetMaterial()->GetTransmittance() != BLACK)
	{
		PhotonRay refractedRay = shape.Refract(in, point, normal);
		ret += GetLightRayColor(refractedRay, specularSteps - 1) * shape.GetMaterial()->GetTransmittance();
	}

	return ret;
}

Color Scene::GeometryEstimateRadiance(const Vector& point, const Vector& normal, const PhotonRay& in, const Shape& shape) const
{
	if ((shape.GetMaterial()->GetDiffuse(point) == BLACK) && (shape.GetMaterial()->GetSpecular() == BLACK))
		return BLACK;

	Color ret = BLACK;

	vector<Node*> nodeList;
	float radius;
	m_diffusePhotonMap.FindKNN_BruteForce(point, m_photonsNeighbours, nodeList, radius);

	for (auto nodeIt = nodeList.begin(); nodeIt < nodeList.end(); ++nodeIt)
	{
		Photon tmpPhoton = (*nodeIt)->GetPhoton();
		float cosine = tmpPhoton.GetIncidence().DotProduct(normal);
		if (cosine < 0.0f)
		{
			Color flux = tmpPhoton.GetFlux();
			if (flux.Isnan())
			{
				std::cout << "flux is nan" << endl;
			}
			Color brdf = shape.GetMaterial()->PhongBRDF(in.GetDirection() * -1.0f, tmpPhoton.GetIncidence(), normal, point);
			if (brdf.Isnan())
			{
				std::cout << "brdf is nan" << endl;
			}

			float gaussianKernel = GaussianKernel(point, (*nodeIt)->GetPoint(), radius);
			if (isnan(gaussianKernel))
			{
				std::cout << "gaussianKernel is nan" << endl;
			}

			ret += flux * brdf * gaussianKernel;

			if (ret.Isnan())
			{
				std::cout << "ret is nan" << endl;
			}
			//ret += tmpPhoton.GetFlux() * shape.GetMaterial()->PhongBRDF(in.GetDirection() * -1.0f, tmpPhoton.GetIncidence(), normal, point) * GaussianKernel(point, (*nodeIt)->GetPoint(), radius);
		}
	}

	return ret / PhotonSphere::Area(radius);
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

float Scene::PathTransmittance(const PhotonRay &lightRay, float tIntersection) const
{
	BX_UNUSED(lightRay);
	BX_UNUSED(tIntersection);

	return 1.0f;
}

bool Scene::IsShadow(const PhotonRay &lightRay, const Vector &light) const
{
	float tLight = lightRay.GetPosition().Distance(light);
	for (unsigned int i = 0; i < m_shapes.size(); ++i)
	{
		float tShape = m_shapes[i]->Intersect(lightRay);
		if (tShape < tLight) return true;
	}

	return false;
}