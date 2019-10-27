#include <iostream>
#include <sstream>
#include <fstream>

#include "mtlloader.h"

using namespace std;

std::map<std::string, MaterialDef> LoadMaterialFile(const std::string& fileName)
{
	map<string, MaterialDef> mtls;

	ifstream file;
	file.open(fileName);
	if (!file.is_open())
		return mtls;

	MaterialDef newMtl;
	string mtlName;

	bool first = true;

	string line;
	while (getline(file, line))
	{
		if (line.size() <= 1)
			continue;

		istringstream stream(line);
		string keyword;
		stream >> keyword;

		if (keyword == "newmtl")
		{
			if (first)
				first = false;
			else
			{
				mtls[mtlName] = newMtl;
				newMtl = MaterialDef();
			}

			stream >> mtlName;
		}
		else if (keyword == "Ns")
		{
			stream >> newMtl.m_specExp;
		}
		else if (keyword == "Ni")
		{
			stream >> newMtl.m_ior;
		}
		else if (keyword == "Kd")
		{
			stream >> newMtl.m_diffuseTint[0] >> newMtl.m_diffuseTint[1] >> newMtl.m_diffuseTint[2];
		}
		else if (keyword == "Ks")
		{
			stream >> newMtl.m_specTint[0] >>
				newMtl.m_specTint[1] >>
				newMtl.m_specTint[2];
		}
		else if (keyword == "map_Ka")
		{
			stream >> newMtl.m_metallicMap;
		}
		else if (keyword == "map_Kd")
		{
			stream >> newMtl.m_diffuseMap;
		}
		else if (keyword == "map_Ns")
		{
			stream >> newMtl.m_roughnessMap;
		}
		else if (keyword == "map_bump")
		{
			stream >> newMtl.m_bmpMap;
		}
	}

	file.close();

	return mtls;
}
