#ifndef __MTLLOADER_H__
#define __MTLLOADER_H__

#include <string>
#include <map>

struct MaterialDef
{
	MaterialDef() : m_specExp(1.0f), m_ior(1.5f), m_diffuseMap(""), m_bmpMap("")
	{
		m_diffuseTint[0] = m_diffuseTint[1] = m_diffuseTint[2] = 1.0f;
		m_specTint[0] = m_specTint[1] = m_specTint[2] = 1.0f;
	}

	float m_specExp;
	float m_ior;
	float m_diffuseTint[3];
	float m_specTint[3];
	std::string m_metallicMap;
	std::string m_diffuseMap;
	std::string m_roughnessMap;
	std::string m_bmpMap;
};

std::map<std::string, MaterialDef> LoadMaterialFile(const std::string& filePath);

#endif
