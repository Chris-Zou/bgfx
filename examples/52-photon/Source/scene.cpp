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
}
