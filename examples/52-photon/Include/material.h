#pragma once

#include "color.h"
#include <memory>
#include "vector.h"

class Material
{
public:
	Material(const Color& diffuse, const Color& specular, const Color& reflectance, const Color& transmittance, const float shininess);
	Color PhongBRDF(const Vector& from, const Vector& light, const Vector&& normal, const Vector& point) const;

	virtual Color GetDiffuse(const Vector& point) const;
	Color GetSpecular() const;
	Color GetReflectance() const;
	Color GetTransmittance() const;
	float GetShininess() const;
private:
	Color m_kd;
	Color m_ks;
	Color m_kr;
	Color m_kt;
	float m_shininess;
};

static Material* NONE = &Material(Material(BLACK, BLACK, BLACK, BLACK, 0.0f));
static Material* MIRROR = &Material(Material(BLACK, BLACK, WHITE, BLACK, 0.0f));
static Material* LAMBERTIAN = &Material(Material(WHITE, BLACK, BLACK, BLACK, 0.0f));
static Material* SPECKLED_LAMBERTIAN = &Material(Material(WHITE / 2, GRAY / 4, BLACK, BLACK, 20.0f));
static Material* GLASS = &Material(Material(BLACK, BLACK, BLACK, WHITE, 0.0f));
