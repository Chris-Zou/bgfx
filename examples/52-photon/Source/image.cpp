#include "../Include/image.h"
#include "../Include/bmp.h"
#include <fstream>
#include <iostream>

using namespace std;

Image::Image(const unsigned int width, const unsigned int height)
{
	m_image.resize(height);
	for (unsigned int i = 0; i < height; ++i)
	{
		m_image[i].resize(width);
	}
}

Image::Image(const std::string& filename)
{
	ifstream inputFile(filename);
	string buffer;

	if (!inputFile.good())
	{
		cout << "Can't read the file " << filename << '\n';
		throw 1;
	}

	while (true)
	{
		if (inputFile.peek() == '#')
		{
			// Ignore the line as a comment.
			getline(inputFile, buffer);
			continue;
		}
		inputFile >> buffer;
		if (buffer == "P3") break;
		else
		{
			cout << "Can't find the ppm header for the file " << filename << '\n';
			throw 1;
		}
	}

	unsigned int width = 0, height = 0;
	while (true)
	{
		if (inputFile.peek() == '#' || inputFile.peek() == '\n')
		{
			// Ignore the line as a comment.
			getline(inputFile, buffer);
			continue;
		}
		else
		{
			inputFile >> width >> height;
			break;
		}
	}

	if ((width == 0) | (height == 0))
	{
		cout << "Didn't find correct width and height in the input file " << filename << '\n';
		throw 1;
	}

	unsigned int maxColorValue = 0;
	while (true)
	{
		if ((inputFile.peek() == '#') | (inputFile.peek() == '\n'))
		{
			// Ignore the line as a comment.
			getline(inputFile, buffer);
			continue;
		}
		else
		{
			inputFile >> maxColorValue;
			break;
		}
	}

	if (maxColorValue == 0)
	{
		cout << "Couldn't find maximum color value in " << filename << '\n';
		throw 1;
	}

	float r = 0.0f, g = 0.0f, b = 0.0f;

	m_image.resize(height);

	for (unsigned int i = 0; i < height; ++i)
	{
		m_image[i].resize(width);
	}

	for (unsigned int i = 0; i < height; ++i)
	{
		for (unsigned int j = 0; j < width; ++j)
		{
			inputFile >> r >> g >> b;
			r /= maxColorValue;
			g /= maxColorValue;
			b /= maxColorValue;

			m_image[i][j] = Color(r, g, b);
		}
	}
}

void Image::Save(const std::string& filename, SaveMode mode) const
{
	ofstream outputFile(filename);

	outputFile << "P3" << '\n' <<
		"# " << filename << '\n' <<
		m_image[0].size() << ' ' << m_image.size() << '\n' <<
		255 << '\n';

	float largest = -1;
	for (unsigned int i = 0; i < m_image.size(); ++i)
	{
		for (unsigned int j = 0; j < m_image.at(i).size(); ++j)
		{
			if (m_image[i][j].GetR() > largest) largest = m_image[i][j].GetR();
			else if (m_image[i][j].GetG() > largest) largest = m_image[i][j].GetG();
			else if (m_image[i][j].GetB() > largest) largest = m_image[i][j].GetB();
		}
	}

	largest = largest < 1.0f ? 1.0f : largest;

	for (unsigned int i = 0; i < m_image.size(); ++i)
	{
		for (unsigned int j = 0; j < m_image[0].size(); ++j)
		{
			Color tmp = m_image[i][j];

			switch (mode)
			{
			case DIM_TO_WHITE:
				tmp = tmp / largest;
				break;
			case GAMMA:
				tmp = tmp.GammaCorrect().Clamp();
				break;
			case CLAMP:
				tmp = tmp.Clamp();
				break;
			}
			outputFile << static_cast<unsigned int>(static_cast<unsigned char>(255 * tmp.GetR())) << ' ' <<
				static_cast<unsigned int>(static_cast<unsigned char>(255 * tmp.GetG())) << ' ' <<
				static_cast<unsigned int>(static_cast<unsigned char>(255 * tmp.GetB())) << '\t';
		}
		outputFile << '\n';
	}
	outputFile.close();
}

void Image::SaveBMP(const string& filename) const
{
	BMP bmp(m_image[0].size(), m_image.size());
	for (unsigned int i = 0; i < m_image[0].size(); ++i)
	{
		for (unsigned int j = 0; j < m_image.size(); ++j)
		{
			Color tmp = m_image[j][i];		
			bmp.fill_point(i, j, 255 * tmp.GetR(), 255 * tmp.GetG(), 255 * tmp.GetB(), 255);
		}
	}

	bmp.write(filename.c_str());
}

unsigned int Image::GetWidth() const
{
	return m_image[0].size();
}

unsigned int Image::GetHeight() const
{
	return m_image.size();
}

std::vector<Color>& Image::operator[](const unsigned int i)
{
	return m_image[i];
}
