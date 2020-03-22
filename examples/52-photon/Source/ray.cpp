
#include "../Include/ray.h"
#include "../Include/plane.h"

Ray::Ray()
{

}

Ray::Ray(const Vector& pos, const Vector& dir)
	: m_pos(pos)
	, m_dir(dir)
{}

std::tuple<float, float> Ray::Distance(const Vector& to) const
{
	Plane plane(to, m_dir);
	float tProj = plane.Intersect(*this);
	Vector intersection = GetScaledPosition(tProj);
	float dist = intersection.Distance(to);

	return std::make_tuple(dist, tProj);
}

Vector Ray::GetScaledPosition(const float s) const
{
	return m_pos + m_dir * s;
}

Vector Ray::GetPosition() const
{
	return m_pos;
}

Vector Ray::GetDirection() const
{
	return m_dir;
}
