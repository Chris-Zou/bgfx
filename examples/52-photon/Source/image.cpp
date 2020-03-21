#include "../Include/image.h"

Image::Image(const unsigned int width, const unsigned int height)
{

}

Image::Image(const std::string& filename)
{

}

void Image::Save(const std::string& filename, SaveMode mode) const
{

}

unsigned int Image::GetWidth() const
{
	return m_image[0].size();
}

unsigned int Image::GetHeight() const
{
	return m_image.size();
}
