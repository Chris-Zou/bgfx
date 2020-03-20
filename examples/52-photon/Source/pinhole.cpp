#include "../Include/pinhole.h"

Pinhole::Pinhole()
	: Camera()
{

}

Pinhole::Pinhole(const Vector& up, const Vector &right, const Vector& towards, const Vector& focalPoint, const float fov, const float viewplaneDistance, int width, int height)
	: Camera(up, rightr, towards, focalPoint, fov, viewplaneDistance, widht, height)
{

}

Vector Pinhole::GetFirstPixel() const
{
	Vector middle = m_focalPoint + m_towards * m_viewplaneDistance;
	Vector first = middle - m_right * (((m_width - 1) / 2.0f) * m_pixelSize) + m_up * (((m_height - 1) / 2.0f) * m_pixelSize);

	return first;
}
