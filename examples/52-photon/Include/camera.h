#pragma once

#include "ray.h"
#include "vector.h"

class Camera
{
public:
	Camera();
	Camera(const Vector& up, const Vector &right, const Vector& towards, const Vector& focalPoint, const float fov, const float viewplaneDistance, int width, int height);
public:
	void CalculatePixelSize();
	virtual Vector GetFirstPixel() const = 0;

	Vector GetUp() const;
	Vector GetRight() const;
	Vector GetTowards() const;
	Vector GetFocalPoint() const;
	void SetImageDimensions(int width, int height);
	int GetWidth() const;
	int GetHeight() const;
	float GetPixelSize() const;

private:
	Vector m_up;
	Vector m_right;
	Vector m_towards;

	Vector m_focalPoint;
	float m_fov;
	float m_viewplaneDistance;
	int m_width, m_height;
	float m_pixelSize;
};
