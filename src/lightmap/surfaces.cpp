//-----------------------------------------------------------------------------
// Note: this is a modified version of dlight. It is not the original software.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2013-2014 Samuel Villarreal
// svkaiser@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
//
//    3. This notice may not be removed or altered from any source
//    distribution.
//

#include "math/mathlib.h"
#include "level/level.h"
#include "surfaces.h"
#include <map>

#ifdef _MSC_VER
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: '=': conversion from '__int64' to 'int', possible loss of data
#endif

LevelMesh::LevelMesh(FLevel &doomMap)
{
	printf("------------- Building side surfaces -------------\n");

	for (unsigned int i = 0; i < doomMap.Sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.Sides[i]);
		printf("sides: %i / %i\r", i + 1, doomMap.Sides.Size());
	}

	printf("\nSide surfaces: %i\n", (int)surfaces.size());

	CreateSubsectorSurfaces(doomMap);

	printf("Surfaces total: %i\n\n", (int)surfaces.size());

	printf("Building collision mesh..\n\n");

	for (size_t i = 0; i < surfaces.size(); i++)
	{
		const auto &s = surfaces[i];
		int numVerts = s->numVerts;
		unsigned int pos = MeshVertices.Size();

		for (int j = 0; j < numVerts; j++)
		{
			MeshVertices.Push(s->verts[j]);
			MeshUVIndex.Push(j);
		}

		if (s->type == ST_FLOOR || s->type == ST_CEILING)
		{
			for (int j = 2; j < numVerts; j++)
			{
				if (!IsDegenerate(s->verts[0], s->verts[j - 1], s->verts[j]))
				{
					MeshElements.Push(pos);
					MeshElements.Push(pos + j - 1);
					MeshElements.Push(pos + j);
					MeshSurfaces.Push(i);
				}
			}
		}
		else if (s->type == ST_MIDDLESIDE || s->type == ST_UPPERSIDE || s->type == ST_LOWERSIDE)
		{
			if (!IsDegenerate(s->verts[0], s->verts[1], s->verts[2]))
			{
				MeshElements.Push(pos + 0);
				MeshElements.Push(pos + 1);
				MeshElements.Push(pos + 2);
				MeshSurfaces.Push(i);
			}
			if (!IsDegenerate(s->verts[1], s->verts[2], s->verts[3]))
			{
				MeshElements.Push(pos + 3);
				MeshElements.Push(pos + 2);
				MeshElements.Push(pos + 1);
				MeshSurfaces.Push(i);
			}
		}
	}

	CollisionMesh = std::make_unique<TriangleMeshShape>(&MeshVertices[0], MeshVertices.Size(), &MeshElements[0], MeshElements.Size());
}

void LevelMesh::CreateSideSurfaces(FLevel &doomMap, IntSideDef *side)
{
	IntSector *front;
	IntSector *back;

	front = doomMap.GetFrontSector(side);
	back = doomMap.GetBackSector(side);

	if (front->controlsector)
		return;

	FloatVertex v1 = doomMap.GetSegVertex(side->line->v1);
	FloatVertex v2 = doomMap.GetSegVertex(side->line->v2);

	if (side->line->sidenum[0] != (ptrdiff_t)(side - &doomMap.Sides[0]))
	{
		std::swap(v1, v2);
	}

	float v1Top = front->ceilingplane.zAt(v1.x, v1.y);
	float v1Bottom = front->floorplane.zAt(v1.x, v1.y);
	float v2Top = front->ceilingplane.zAt(v2.x, v2.y);
	float v2Bottom = front->floorplane.zAt(v2.x, v2.y);

	int typeIndex = side - &doomMap.Sides[0];

	Vec2 dx(v2.x - v1.x, v2.y - v1.y);
	float distance = std::sqrt(dx.Dot(dx));

	if (back)
	{
		for (unsigned int j = 0; j < front->x3dfloors.Size(); j++)
		{
			IntSector *xfloor = front->x3dfloors[j];

			// Don't create a line when both sectors have the same 3d floor
			bool bothSides = false;
			for (unsigned int k = 0; k < back->x3dfloors.Size(); k++)
			{
				if (back->x3dfloors[k] == xfloor)
				{
					bothSides = true;
					break;
				}
			}
			if (bothSides)
				continue;

			float texWidth = 128.0f;
			float texHeight = 128.0f;

			auto surf = std::make_unique<Surface>();
			surf->material = "texture";
			surf->type = ST_MIDDLESIDE;
			surf->typeIndex = typeIndex;
			surf->controlSector = xfloor;
			surf->numVerts = 4;
			surf->verts.resize(4);
			surf->verts[0].x = surf->verts[2].x = v2.x;
			surf->verts[0].y = surf->verts[2].y = v2.y;
			surf->verts[1].x = surf->verts[3].x = v1.x;
			surf->verts[1].y = surf->verts[3].y = v1.y;
			surf->verts[0].z = xfloor->floorplane.zAt(v2.x, v2.y);
			surf->verts[1].z = xfloor->floorplane.zAt(v1.x, v1.y);
			surf->verts[2].z = xfloor->ceilingplane.zAt(v2.x, v2.y);
			surf->verts[3].z = xfloor->ceilingplane.zAt(v1.x, v1.y);
			surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
			surf->plane.SetDistance(surf->verts[0]);

			float texZ = surf->verts[0].z;

			surf->uvs.resize(4);
			surf->uvs[0].x = 0.0f;
			surf->uvs[1].x = distance / texWidth;
			surf->uvs[2].x = 0.0f;
			surf->uvs[3].x = distance / texWidth;
			surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
			surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
			surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
			surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

			surfaces.push_back(std::move(surf));
		}

		float v1TopBack = back->ceilingplane.zAt(v1.x, v1.y);
		float v1BottomBack = back->floorplane.zAt(v1.x, v1.y);
		float v2TopBack = back->ceilingplane.zAt(v2.x, v2.y);
		float v2BottomBack = back->floorplane.zAt(v2.x, v2.y);

		if (v1Top == v1TopBack && v1Bottom == v1BottomBack && v2Top == v2TopBack && v2Bottom == v2BottomBack)
		{
			return;
		}

		// bottom seg
		if (v1Bottom < v1BottomBack || v2Bottom < v2BottomBack)
		{
			if (side->bottomtexture[0] != '-')
			{
				float texWidth = 128.0f;
				float texHeight = 128.0f;

				auto surf = std::make_unique<Surface>();
				surf->material = side->bottomtexture;
				surf->numVerts = 4;
				surf->verts.resize(4);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1Bottom;
				surf->verts[1].z = v2Bottom;
				surf->verts[2].z = v1BottomBack;
				surf->verts[3].z = v2BottomBack;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_LOWERSIDE;
				surf->typeIndex = typeIndex;
				surf->controlSector = nullptr;

				float texZ = surf->verts[0].z;

				surf->uvs.resize(4);
				surf->uvs[0].x = 0.0f;
				surf->uvs[1].x = distance / texWidth;
				surf->uvs[2].x = 0.0f;
				surf->uvs[3].x = distance / texWidth;
				surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
				surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
				surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
				surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

				surfaces.push_back(std::move(surf));
			}

			v1Bottom = v1BottomBack;
			v2Bottom = v2BottomBack;
		}

		// top seg
		if (v1Top > v1TopBack || v2Top > v2TopBack)
		{
			bool bSky = false;

			if (front->skySector && back->skySector)
			{
				if (front->data.ceilingheight != back->data.ceilingheight && side->toptexture[0] == '-')
				{
					bSky = true;
				}
			}

			if (side->toptexture[0] != '-' || bSky)
			{
				float texWidth = 128.0f;
				float texHeight = 128.0f;

				auto surf = std::make_unique<Surface>();
				surf->material = side->toptexture;
				surf->numVerts = 4;
				surf->verts.resize(4);

				surf->verts[0].x = surf->verts[2].x = v1.x;
				surf->verts[0].y = surf->verts[2].y = v1.y;
				surf->verts[1].x = surf->verts[3].x = v2.x;
				surf->verts[1].y = surf->verts[3].y = v2.y;
				surf->verts[0].z = v1TopBack;
				surf->verts[1].z = v2TopBack;
				surf->verts[2].z = v1Top;
				surf->verts[3].z = v2Top;

				surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
				surf->plane.SetDistance(surf->verts[0]);
				surf->type = ST_UPPERSIDE;
				surf->typeIndex = typeIndex;
				surf->bSky = bSky;
				surf->controlSector = nullptr;

				float texZ = surf->verts[0].z;

				surf->uvs.resize(4);
				surf->uvs[0].x = 0.0f;
				surf->uvs[1].x = distance / texWidth;
				surf->uvs[2].x = 0.0f;
				surf->uvs[3].x = distance / texWidth;
				surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
				surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
				surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
				surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

				surfaces.push_back(std::move(surf));
			}

			v1Top = v1TopBack;
			v2Top = v2TopBack;
		}
	}

	// middle seg
	if (back == nullptr)
	{
		float texWidth = 128.0f;
		float texHeight = 128.0f;

		auto surf = std::make_unique<Surface>();
		surf->material = side->midtexture;
		surf->numVerts = 4;
		surf->verts.resize(4);

		surf->verts[0].x = surf->verts[2].x = v1.x;
		surf->verts[0].y = surf->verts[2].y = v1.y;
		surf->verts[1].x = surf->verts[3].x = v2.x;
		surf->verts[1].y = surf->verts[3].y = v2.y;
		surf->verts[0].z = v1Bottom;
		surf->verts[1].z = v2Bottom;
		surf->verts[2].z = v1Top;
		surf->verts[3].z = v2Top;

		surf->plane.SetNormal(surf->verts[0], surf->verts[1], surf->verts[2]);
		surf->plane.SetDistance(surf->verts[0]);
		surf->type = ST_MIDDLESIDE;
		surf->typeIndex = typeIndex;
		surf->controlSector = nullptr;

		float texZ = surf->verts[0].z;

		surf->uvs.resize(4);
		surf->uvs[0].x = 0.0f;
		surf->uvs[1].x = distance / texWidth;
		surf->uvs[2].x = 0.0f;
		surf->uvs[3].x = distance / texWidth;
		surf->uvs[0].y = (surf->verts[0].z - texZ) / texHeight;
		surf->uvs[1].y = (surf->verts[1].z - texZ) / texHeight;
		surf->uvs[2].y = (surf->verts[2].z - texZ) / texHeight;
		surf->uvs[3].y = (surf->verts[3].z - texZ) / texHeight;

		surfaces.push_back(std::move(surf));
	}
}

void LevelMesh::CreateFloorSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor)
{
	auto surf = std::make_unique<Surface>();
	surf->material = sector->data.floorpic;
	surf->numVerts = sub->numlines;
	surf->verts.resize(surf->numVerts);
	surf->uvs.resize(surf->numVerts);

	if (!is3DFloor)
	{
		surf->plane = sector->floorplane;
	}
	else
	{
		surf->plane = Plane::Inverse(sector->ceilingplane);
	}

	for (int j = 0; j < surf->numVerts; j++)
	{
		MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + (surf->numVerts - 1) - j];
		FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

		surf->verts[j].x = v1.x;
		surf->verts[j].y = v1.y;
		surf->verts[j].z = surf->plane.zAt(surf->verts[j].x, surf->verts[j].y);

		surf->uvs[j].x = v1.x / 64.0f;
		surf->uvs[j].y = v1.y / 64.0f;
	}

	surf->type = ST_FLOOR;
	surf->typeIndex = typeIndex;
	surf->controlSector = is3DFloor ? sector : nullptr;

	surfaces.push_back(std::move(surf));
}

void LevelMesh::CreateCeilingSurface(FLevel &doomMap, MapSubsectorEx *sub, IntSector *sector, int typeIndex, bool is3DFloor)
{
	auto surf = std::make_unique<Surface>();
	surf->material = sector->data.ceilingpic;
	surf->numVerts = sub->numlines;
	surf->verts.resize(surf->numVerts);
	surf->uvs.resize(surf->numVerts);
	surf->bSky = sector->skySector;

	if (!is3DFloor)
	{
		surf->plane = sector->ceilingplane;
	}
	else
	{
		surf->plane = Plane::Inverse(sector->floorplane);
	}

	for (int j = 0; j < surf->numVerts; j++)
	{
		MapSegGLEx *seg = &doomMap.GLSegs[sub->firstline + j];
		FloatVertex v1 = doomMap.GetSegVertex(seg->v1);

		surf->verts[j].x = v1.x;
		surf->verts[j].y = v1.y;
		surf->verts[j].z = surf->plane.zAt(surf->verts[j].x, surf->verts[j].y);

		surf->uvs[j].x = v1.x / 64.0f;
		surf->uvs[j].y = v1.y / 64.0f;
	}

	surf->type = ST_CEILING;
	surf->typeIndex = typeIndex;
	surf->controlSector = is3DFloor ? sector : nullptr;

	surfaces.push_back(std::move(surf));
}

void LevelMesh::CreateSubsectorSurfaces(FLevel &doomMap)
{
	printf("------------- Building subsector surfaces -------------\n");

	for (int i = 0; i < doomMap.NumGLSubsectors; i++)
	{
		printf("subsectors: %i / %i\r", i + 1, doomMap.NumGLSubsectors);

		MapSubsectorEx *sub = &doomMap.GLSubsectors[i];

		if (sub->numlines < 3)
		{
			continue;
		}

		IntSector *sector = doomMap.GetSectorFromSubSector(sub);
		if (!sector || sector->controlsector)
			continue;

		CreateFloorSurface(doomMap, sub, sector, i, false);
		CreateCeilingSurface(doomMap, sub, sector, i, false);

		for (unsigned int j = 0; j < sector->x3dfloors.Size(); j++)
		{
			CreateFloorSurface(doomMap, sub, sector->x3dfloors[j], i, true);
			CreateCeilingSurface(doomMap, sub, sector->x3dfloors[j], i, true);
		}
	}

	printf("\nLeaf surfaces: %i\n", (int)surfaces.size() - doomMap.NumGLSubsectors);
}

LevelTraceHit LevelMesh::Trace(const Vec3 &startVec, const Vec3 &endVec)
{
	TraceHit hit = TriangleMeshShape::find_first_hit(CollisionMesh.get(), startVec, endVec);

	LevelTraceHit trace;
	trace.start = startVec;
	trace.end = endVec;
	trace.fraction = hit.fraction;
	if (trace.fraction < 1.0f)
	{
		int elementIdx = hit.triangle * 3;
		trace.hitSurface = surfaces[MeshSurfaces[hit.triangle]].get();
		trace.indices[0] = MeshUVIndex[MeshElements[elementIdx]];
		trace.indices[1] = MeshUVIndex[MeshElements[elementIdx + 1]];
		trace.indices[2] = MeshUVIndex[MeshElements[elementIdx + 2]];
		trace.b = hit.b;
		trace.c = hit.c;
	}
	else
	{
		trace.hitSurface = nullptr;
		trace.indices[0] = 0;
		trace.indices[1] = 0;
		trace.indices[2] = 0;
		trace.b = 0.0f;
		trace.c = 0.0f;
	}
	return trace;
}

bool LevelMesh::TraceAnyHit(const Vec3 &startVec, const Vec3 &endVec)
{
	return TriangleMeshShape::find_any_hit(CollisionMesh.get(), startVec, endVec);
}

bool LevelMesh::IsDegenerate(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = v1.x - v0.x;
	float ay = v1.y - v0.y;
	float az = v1.z - v0.z;
	float bx = v2.x - v0.x;
	float by = v2.y - v0.y;
	float bz = v2.z - v0.z;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-6f;
}

void LevelMesh::Export(std::string filename)
{
	// This is so ugly! I had nothing to do with it! ;)
	std::string mtlfilename = filename;
	for (int i = 0; i < 3; i++) mtlfilename.pop_back();
	mtlfilename += "mtl";

	TArray<Vec3> outvertices;
	TArray<Vec2> outuv;
	TArray<Vec3> outnormal;
	TArray<int> outface;

	outvertices.Resize(MeshVertices.Size());
	outuv.Resize(MeshVertices.Size());
	outnormal.Resize(MeshVertices.Size());

	for (unsigned int surfidx = 0; surfidx < MeshElements.Size() / 3; surfidx++)
	{
		Surface* surface = surfaces[MeshSurfaces[surfidx]].get();
		for (int i = 0; i < 3; i++)
		{
			int elementidx = surfidx * 3 + i;
			int vertexidx = MeshElements[elementidx];
			int uvindex = MeshUVIndex[vertexidx];

			outvertices[vertexidx] = MeshVertices[vertexidx];
			outuv[vertexidx] = Vec2(surface->lightmapCoords[uvindex * 2], surface->lightmapCoords[uvindex * 2 + 1]);
			outnormal[vertexidx] = surface->plane.Normal();
			outface.Push(vertexidx);

			//surface->lightmapNum;
		}
	}

	std::string buffer;
	buffer.reserve(16 * 1024 * 1024);

	buffer += "# zdray exported mesh\r\n";

	buffer += "mtllib ";
	buffer += mtlfilename;
	buffer += "\r\n";

	buffer += "usemtl Textured\r\n";

	float scale = 0.01f;

	for (unsigned int i = 0; i < outvertices.Size(); i++)
	{
		buffer += "v ";
		buffer += std::to_string(-outvertices[i].x * scale);
		buffer += " ";
		buffer += std::to_string(outvertices[i].z * scale);
		buffer += " ";
		buffer += std::to_string(outvertices[i].y * scale);
		buffer += "\r\n";
	}

	for (unsigned int i = 0; i < outnormal.Size(); i++)
	{
		buffer += "vn ";
		buffer += std::to_string(-outnormal[i].x);
		buffer += " ";
		buffer += std::to_string(outnormal[i].z);
		buffer += " ";
		buffer += std::to_string(outnormal[i].y);
		buffer += "\r\n";
	}

	for (unsigned int i = 0; i < outuv.Size(); i++)
	{
		buffer += "vt ";
		buffer += std::to_string(outuv[i].x);
		buffer += " ";
		buffer += std::to_string(1.0f - outuv[i].y);
		buffer += "\r\n";
	}

	for (unsigned int i = 0; i < outface.Size(); i += 3)
	{
		std::string e0 = std::to_string(outface[i] + 1);
		std::string e1 = std::to_string(outface[i + 1] + 1);
		std::string e2 = std::to_string(outface[i + 2] + 1);
		buffer += "f ";
		buffer += e0;
		buffer += "/";
		buffer += e0;
		buffer += "/";
		buffer += e0;
		buffer += " ";
		buffer += e1;
		buffer += "/";
		buffer += e1;
		buffer += "/";
		buffer += e1;
		buffer += " ";
		buffer += e2;
		buffer += "/";
		buffer += e2;
		buffer += "/";
		buffer += e2;
		buffer += "\r\n";
	}

	FILE* file = fopen(filename.c_str(), "wb");
	if (file)
	{
		fwrite(buffer.data(), buffer.size(), 1, file);
		fclose(file);
	}

	std::string mtl = R"(newmtl Textured
   Ka 1.000 1.000 1.000
   Kd 1.000 1.000 1.000
   Ks 0.000 0.000 0.000
   map_Ka lightmap0.png
   map_Kd lightmap0.png
)";

	file = fopen(mtlfilename.c_str(), "wb");
	if (file)
	{
		fwrite(mtl.data(), mtl.size(), 1, file);
		fclose(file);
	}

#if 0
	// Convert model mesh:

	auto zmodel = std::make_unique<ZModel>();

	zmodel->Vertices.resize(MeshVertices.Size());
	for (unsigned int i = 0; i < MeshVertices.Size(); i++)
	{
		ZModelVertex &vertex = zmodel->Vertices[i];
		vertex.Pos.X = MeshVertices[i].x;
		vertex.Pos.Y = MeshVertices[i].z;
		vertex.Pos.Z = MeshVertices[i].y;
		vertex.BoneWeights.X = 0.0f;
		vertex.BoneWeights.Y = 0.0f;
		vertex.BoneWeights.Z = 0.0f;
		vertex.BoneWeights.W = 0.0f;
		vertex.BoneIndices.X = 0;
		vertex.BoneIndices.Y = 0;
		vertex.BoneIndices.Z = 0;
		vertex.BoneIndices.W = 0;
		vertex.Normal.X = 0.0f;
		vertex.Normal.Y = 0.0f;
		vertex.Normal.Z = 0.0f;
		vertex.TexCoords.X = 0.0f;
		vertex.TexCoords.Y = 0.0f;
	}

	std::map<std::string, std::vector<uint32_t>> materialRanges;

	for (unsigned int surfidx = 0; surfidx < MeshElements.Size() / 3; surfidx++)
	{
		Surface *surface = surfaces[MeshSurfaces[surfidx]].get();
		for (int i = 0; i < 3; i++)
		{
			int elementidx = surfidx * 3 + i;
			int vertexidx = MeshElements[elementidx];
			int uvindex = MeshUVIndex[vertexidx];

			ZModelVertex &vertex = zmodel->Vertices[vertexidx];
			vertex.Normal.X = surface->plane.Normal().x;
			vertex.Normal.Y = surface->plane.Normal().z;
			vertex.Normal.Z = surface->plane.Normal().y;
			vertex.TexCoords.X = surface->uvs[uvindex].x;
			vertex.TexCoords.Y = surface->uvs[uvindex].y;
			vertex.TexCoords2.X = surface->lightmapCoords[uvindex * 2];
			vertex.TexCoords2.Y = surface->lightmapCoords[uvindex * 2 + 1];
			vertex.TexCoords2.Z = surface->lightmapNum;

			std::string matname = surface->material;

			size_t lastslash = matname.find_last_of('/');
			if (lastslash != std::string::npos)
				matname = matname.substr(lastslash + 1);

			size_t lastdot = matname.find_last_of('.');
			if (lastdot != 0 && lastdot != std::string::npos)
				matname = matname.substr(0, lastdot);

			for (auto &c : matname)
			{
				if (c >= 'A' && c <= 'Z') c = 'a' + (c - 'A');
			}

			matname = "materials/" + matname;

			materialRanges[matname].push_back(vertexidx);
		}
	}

	zmodel->Elements.reserve(MeshElements.Size());

	for (const auto &it : materialRanges)
	{
		uint32_t startElement = (uint32_t)zmodel->Elements.size();
		for (uint32_t vertexidx : it.second)
			zmodel->Elements.push_back(vertexidx);
		uint32_t vertexCount = (uint32_t)zmodel->Elements.size() - startElement;

		ZModelMaterial mat;
		mat.Name = it.first;
		mat.Flags = 0;
		mat.Renderstyle = 0;
		mat.StartElement = startElement;
		mat.VertexCount = vertexCount;
		zmodel->Materials.push_back(mat);
	}

	// Save mesh

	ZChunkStream zmdl, zdat;

	// zmdl
	{
		ZChunkStream &s = zmdl;
		s.Uint32(zmodel->Version);

		s.Uint32(zmodel->Materials.size());
		for (const ZModelMaterial &mat : zmodel->Materials)
		{
			s.String(mat.Name);
			s.Uint32(mat.Flags);
			s.Uint32(mat.Renderstyle);
			s.Uint32(mat.StartElement);
			s.Uint32(mat.VertexCount);
		}

		s.Uint32(zmodel->Bones.size());
		for (const ZModelBone &bone : zmodel->Bones)
		{
			s.String(bone.Name);
			s.Uint32((uint32_t)bone.Type);
			s.Uint32(bone.ParentBone);
			s.Vec3f(bone.Pivot);
		}

		s.Uint32(zmodel->Animations.size());
		for (const ZModelAnimation &anim : zmodel->Animations)
		{
			s.String(anim.Name);
			s.Float(anim.Duration);
			s.Vec3f(anim.AabbMin);
			s.Vec3f(anim.AabbMax);
			s.Uint32(anim.Bones.size());
			for (const ZModelBoneAnim &bone : anim.Bones)
			{
				s.FloatArray(bone.Translation.Timestamps);
				s.Vec3fArray(bone.Translation.Values);
				s.FloatArray(bone.Rotation.Timestamps);
				s.QuaternionfArray(bone.Rotation.Values);
				s.FloatArray(bone.Scale.Timestamps);
				s.Vec3fArray(bone.Scale.Values);
			}
			s.Uint32(anim.Materials.size());
			for (const ZModelMaterialAnim &mat : anim.Materials)
			{
				s.FloatArray(mat.Translation.Timestamps);
				s.Vec3fArray(mat.Translation.Values);
				s.FloatArray(mat.Rotation.Timestamps);
				s.QuaternionfArray(mat.Rotation.Values);
				s.FloatArray(mat.Scale.Timestamps);
				s.Vec3fArray(mat.Scale.Values);
			}
		}

		s.Uint32(zmodel->Attachments.size());
		for (const ZModelAttachment &attach : zmodel->Attachments)
		{
			s.String(attach.Name);
			s.Uint32(attach.Bone);
			s.Vec3f(attach.Position);
		}
	}

	// zdat
	{
		ZChunkStream &s = zdat;

		s.VertexArray(zmodel->Vertices);
		s.Uint32Array(zmodel->Elements);
	}

	FILE *file = fopen(filename.c_str(), "wb");
	if (file)
	{
		uint32_t chunkhdr[2];
		memcpy(chunkhdr, "ZMDL", 4);
		chunkhdr[1] = zmdl.ChunkLength();
		fwrite(chunkhdr, 8, 1, file);
		fwrite(zmdl.ChunkData(), zmdl.ChunkLength(), 1, file);

		memcpy(chunkhdr, "ZDAT", 4);
		chunkhdr[1] = zdat.ChunkLength();
		fwrite(chunkhdr, 8, 1, file);
		fwrite(zdat.ChunkData(), zdat.ChunkLength(), 1, file);

		fclose(file);
	}
#endif
}
