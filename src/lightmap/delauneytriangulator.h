
#pragma once

#include <vector>
#include <algorithm>

// Bowyer-Watson delauney triangulator (http://paulbourke.net/papers/triangulate/)
class DelauneyTriangulator
{
public:
	struct Vertex
	{
		Vertex() = default;
		Vertex(float x, float y, void* data = nullptr) : x(x), y(y), data(data) { }

		float x = 0.0f;
		float y = 0.0f;
		void* data = nullptr;
		Vertex* next = nullptr;
	};

	struct Triangle
	{
		Vertex* A;
		Vertex* B;
		Vertex* C;
		float circumcenter_x;
		float circumcenter_y;
		float radius2;
	};

	typedef std::pair<Vertex*, Vertex*> Triangle_Edge;

	std::vector<Vertex> vertices;
	std::vector<Triangle> triangles;

	void triangulate();

private:
	std::vector<Vertex*> create_ordered_vertex_list();
	static std::vector<Vertex*> remove_duplicates(const std::vector<Vertex*>& ordered_vertices);
	static void calculate_supertriangle(std::vector<Vertex*>& vertices, Triangle& super_triangle);
	static void calc_cirumcenter(Triangle& triangle);
	static std::vector<Triangle> perform_delauney_triangulation(const std::vector<Vertex*>& vertices, const Triangle& super_triangle);
};
