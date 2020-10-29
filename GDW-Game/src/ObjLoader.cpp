#include "ObjLoader.h"

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>


// Borrowed from https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
#pragma region String Trimming

// trim from start (in place)
static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

#pragma endregion 

VertexArrayObject::sptr ObjLoader::LoadFromFile(const std::string& filename)
{
	// Open our file in binary mode
	std::ifstream file;
	file.open(filename, std::ios::binary);

	// If our file fails to open, we will throw an error
	if (!file) {
		throw std::runtime_error("Failed to open file");
	}

	MeshBuilder<VertexPosNormTexCol> mesh;
	std::string line;

	GLint temp_GLint = 0;

	std::vector<glm::fvec3> vertexPos;
	std::vector<glm::fvec2> vertexTex;
	std::vector<glm::fvec3> vertexNor;

	std::vector<GLint>vertexPosInd;
	std::vector<GLint>vertexTexInd;
	std::vector<GLint>vertexNorInd;
	std::vector<VertexPosNormTexCol> VertexArray;
	glm::vec3 temp_vec3;
	glm::vec2 temp_vec2;
	


	// TODO: Load from file
	while (std::getline(file, line)) {
		trim(line);
		if (line.substr(0, 1) == "#")
		{
			// Comment, no-op
		}
		// V
		else if (line.substr(0, 2) == "v ")
		{
			std::istringstream ss = std::istringstream(line.substr(2));
			ss >> temp_vec3.x >> temp_vec3.y >> temp_vec3.z;
			vertexPos.push_back(temp_vec3);
		}
		// VT
		else if (line.substr(0, 3) == "vt ")
		{
			std::istringstream ss = std::istringstream(line.substr(3));
			ss >> temp_vec2.x >> temp_vec2.y;
			vertexTex.push_back(temp_vec2);
		}
		//VN
		else if (line.substr(0, 3) == "vn ")
		{
			std::istringstream ss = std::istringstream(line.substr(3));
			ss >> temp_vec3.x >> temp_vec3.y >> temp_vec3.z;
			vertexNor.push_back(temp_vec3);
		}
		//F
		else if (line.substr(0, 2) == "f ")
		{
			std::istringstream ss = std::istringstream(line.substr(2));
			int Count = 0;
			while (ss >> temp_GLint)
			{
				if (Count == 0)
				{
					vertexPosInd.push_back(temp_GLint);
				}
				else if (Count == 1)
				{
					vertexTexInd.push_back(temp_GLint);
				}
				else if (Count == 2)
				{
					vertexNorInd.push_back(temp_GLint);
				}

				if (ss.peek() == '/')
				{
					++Count;
					ss.ignore(1, '/');
				}
				else if (ss.peek() == ' ')
				{
					++Count;
					ss.ignore(1, ' ');
				}

				if (Count > 2)
				{
					Count = 0;
				}
			}
		}

		VertexArray.resize(vertexPosInd.size(), VertexPosNormTexCol());

	
		for (size_t i = 0; i < VertexArray.size(); ++i)
		{
			
			VertexArray[i].Position = vertexPos[vertexPosInd[i] - 1];
			VertexArray[i].UV = vertexTex[vertexTexInd[i] - 1];
			VertexArray[i].Normal = vertexNor[vertexNorInd[i] - 1];
			VertexArray[i].Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
			mesh.AddVertex(VertexArray[i].Position, VertexArray[i].Normal, VertexArray[i].UV, VertexArray[i].Color);
			mesh.AddIndex(i);
			
		}

	}
	
	return mesh.Bake();
}

