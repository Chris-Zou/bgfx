#pragma once

#include "camera.h"
#include "ray.h"

class Pinhole : public Camera
{
public:
	Pinhole();
	Pinhole(const Vector& up, const Vector &right, const Vector& towards, const Vector& focalPoint, const float fov, const float viewplaneDistance, int width, int height);
	Vector GetFirstPixel() const;
};