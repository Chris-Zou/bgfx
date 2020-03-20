#include "../Include/camera.h"
#include "../Include/utils.h"

Camera::Camera()
	:m_up(0, 1, 0)
	, m_right(1, 0, 0)
	, m_towards(0, 0, 1)
	, m_focalPoint(0, 0, 0)
	, m_fov(PI / 3)
	, m_viewplaneDistance(1.0f)
	, m_width(256)
	, m_height(256)
{
	CalculatePixelSize();
}

Camera::Camera(const Vector& up, const Vector& right, const Vector& towards, const Vector& focalPoint, const float fov, const float viewplaneDistance, int width, int height)
	: m_up(up)
	, m_right(right)
	, m_towards(towards)
	, m_focalPoint(focalPoint)
	, m_fov(fov)
	, m_viewplaneDistance(viewplaneDistance)
	, m_width(width)
	, m_height(height)
{
	CalculatePixelSize();
}

void Camera::CalculatePixelSize()
{
	m_pixelSize = static_cast<float>(2.0f * tan(m_fov / 2.0f) / m_height);
}

Vector Camera::GetUp() const
{
	return m_up;
}

Vector Camera::GetRight() const
{
	return m_right;
}

Vector Camera::GetTowards() const
{
	return m_towards;
}

Vector Camera::GetFocalPoint() const
{
	return m_focalPoint;
}

int Camera::GetWidht() const
{
	return m_width;
}

int Camera::GetHeight() const
{
	return m_height;
}

float Camera::GetPixelSize() const
{
	return m_pixelSize;
}

void Camera::SetImageDimensions(int widht, int height)
{
	m_width = widht;
	m_height = height;
	CalculatePixelSize();
}
