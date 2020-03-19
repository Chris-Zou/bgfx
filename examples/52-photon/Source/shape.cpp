#include "../Include/shape.h"

Shape::Shape()
{
	m_material = LAMBERTIAN;
}
Vector Shape::Reflect(const Vector& in, const Vector& normal)
{
	return in - normal * in.DotProduct(normal) * 2;
}

Ray Shape::Refract(const Ray& in, const Vector& point, const Vector& visibleNormal) const
{

}
