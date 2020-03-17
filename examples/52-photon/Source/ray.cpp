
#include "../Include/ray.h"

Ray::Ray()
{

}

Ray::Ray(const Vector& pos, const Vector& dir)
	: m_pos(pos)
	, m_dir(dir)
{}

std::tuple<float, float> Ray::Distance(const Vector& to) const
{

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
