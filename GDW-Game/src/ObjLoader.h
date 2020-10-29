#pragma once

#include "MeshBuilder.h"
#include "MeshFactory.h"

class ObjLoader
{
public:
	static VertexArrayObject::sptr LoadFromFile(const std::string& filename);

protected:
	ObjLoader() = default;
	~ObjLoader() = default;
};