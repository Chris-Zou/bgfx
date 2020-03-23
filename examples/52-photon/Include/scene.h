#pragma once

#include "camera.h"
#include "coloredRay.h"
#include "kdtree.h"
#include "lightSource.h"
#include "shape.h"
#include "image.h"
#include <vector>
#include <memory>

using namespace std;

class Scene
{
public:

	void SetCamera(Camera& c)
	{
		m_camera = &c;
	}

	void AddLightSource(LightSource& ls)
	{
		m_lightSource.push_back(&ls);
	}

	void AddShape(Shape& shape)
	{
		m_shapes.push_back(&shape);
	}

	void SetImageDimensions(unsigned int width, unsigned int height)
	{
		m_camera->SetImageDimensions(width, height);
	}

	void SetSpecularSteps(unsigned int steps)
	{
		m_specularSteps = steps;
	}

	void SetEmitedPhotons(unsigned int photonCount)
	{
		m_photonEmitted = photonCount;
	}

	void SetKNearestNeighbours(unsigned int kNeighbours)
	{
		m_photonsNeighbours = kNeighbours;
	}

	Image * Render() const;
	Image* RenderMultiThread(const unsigned int threads) const;
	void EmitPhotons();

private:
	void RenderPixelRange(Image* image) const;
	void PhotonInteraction(const ColoredRay& lightRay, const bool save);
	void GeometryInteraction(const ColoredRay& lightRay, const Shape* shape, const Vector& intersection, bool save);
	Color GetLightRayColor(const PhotonRay& lightRay, const int specularSteps) const;
	Color DirectLight(const Vector& point, const Vector& normal, const PhotonRay& from, const Shape& shape) const;
	Color SpecularLight(const Vector& point, const Vector& normal, const PhotonRay& in, const Shape& shape, const int specularSteps) const;
	Color GeometryEstimateRadiance(const Vector& point, const Vector& normal, const PhotonRay& in, const Shape& shape) const;
	static float GaussianKernel(const Vector& point, const Vector& photon, const float radius);
	static float SilvermanKernel(const float x);
	float PathTransmittance(const PhotonRay& lightRay, float tIntersection) const;
	bool IsShadow(const PhotonRay& lightRay, const Vector& light) const;

private:
	unsigned int m_specularSteps = 4;
	unsigned int m_photonEmitted = 100000;
	unsigned int m_photonsNeighbours = 5000;
	float m_beamRadius = 0.05f;
	Camera* m_camera;
	vector<LightSource*> m_lightSource;
	vector<Shape*> m_shapes;
	KDTree m_diffusePhotonMap;
};
