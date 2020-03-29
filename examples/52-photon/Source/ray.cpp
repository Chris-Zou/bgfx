
#include "../Include/ray.h"
#include "../Include/plane.h"

PhotonRay::PhotonRay()
{

}

PhotonRay::PhotonRay(const Vector& pos, const Vector& dir)
	: m_pos(pos)
	, m_dir((dir - pos).Normalize())
{}

std::tuple<float, float> PhotonRay::Distance(const Vector& to) const
{
	Plane plane(to, m_dir);
	float tProj = plane.Intersect(*this);
	Vector intersection = GetScaledPosition(tProj);
	float dist = intersection.Distance(to);

	return std::make_tuple(dist, tProj);
}

Vector PhotonRay::GetScaledPosition(const float s) const
{
	return m_pos + m_dir * s;
}

Vector PhotonRay::GetPosition() const
{
	return m_pos;
}

Vector PhotonRay::GetDirection() const
{
	return m_dir;
}
