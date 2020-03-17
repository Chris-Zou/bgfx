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

static const shared_ptr<Material> NONE = make_shared<Material>(Material(BLACK, BLACK, BLACK, BLACK, 0.0f));
static const shared_ptr<Material> MIRROR = make_shared<Material>(Material(BLACK, BLACK, WHITE, BLACK, 0.0f));
static const shared_ptr<Material> LAMBERTIAN = make_shared<Material>(Material(WHITE, BLACK, BLACK, BLACK, 0.0f));
static const shared_ptr<Material> SPECKLED_LAMBERTIAN = make_shared<Material>(Material(WHITE / 2, GRAY / 4, BLACK, BLACK, 20.0f));
static const shared_ptr<Material> GLASS = make_shared<Material>(Material(BLACK, BLACK, BLACK, WHITE, 0.0f));
