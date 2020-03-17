
#include <cmath>
#include "../Include/material.h"
#include "../Include/shape.h"
#include "../Include/utils.h"

Material::Material(const Color& diffuse, const Color& specular, const Color& reflectance, const Color& transmittance, const float shininess)
	: m_kd(diffuse)
	, m_ks(specular)
	, m_kr(reflectance)
	, m_kt(transmittance)
	, m_shininess(shininess)
{
}

Color Material::PhongBRDF(const Vector& from, const Vector& light, const Vector&& normal, const Vector& point) const
{
	Vector reflectedLight = Shape::Reflect(light * -1, normal);
	float cosine = from.DotProduct(reflectedLight);
	if (cosine < 0.0f)
		cosine = 0.0f;

	return (GetDiffuse(point) / PI) + m_ks * ((m_shininess + 2) / (2 * PI) * pow(cosine, m_shininess));
}

Color Material::GetDiffuse() const
{
	return m_kd;
}

Color Material::GetSpecular() const
{
	return m_ks;
}

Color Material::GetReflectance() const
{
	return m_kr;
}

Color Material::GetTransmittance() const
{
	return m_kt;
}

float Material::GetShininess() const
{
	return m_shininess;
}
