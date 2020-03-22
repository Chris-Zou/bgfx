#pragma once

#include "color.h"
#include <string>
#include <vector>

enum SaveMode
{
	DIM_TO_WHITE,
	GAMMA,
	CLAMP,
};

class Image
{
public:
	Image(const unsigned int width, const unsigned int height);
	Image(const std::string& filename);
	void Save(const std::string& filename, SaveMode mode = DIM_TO_WHITE) const;
	unsigned int GetWidth() const;
	unsigned int GetHeight() const;
	std::vector<Color>& operator[](const unsigned int i);
private:
	std::vector<std::vector<Color> > m_image;
};
