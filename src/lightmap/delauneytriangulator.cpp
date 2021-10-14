
#include "delauneytriangulator.h"

void DelauneyTriangulator::triangulate()
{
	std::vector<Vertex*> ordered_vertices = remove_duplicates(create_ordered_vertex_list());

	Vertex super_A, super_B, super_C;
	Triangle super_triangle;
	super_triangle.A = &super_A;
	super_triangle.B = &super_B;
	super_triangle.C = &super_C;
	calculate_supertriangle(ordered_vertices, super_triangle);

	triangles = perform_delauney_triangulation(ordered_vertices, super_triangle);
}

std::vector<DelauneyTriangulator::Vertex*> DelauneyTriangulator::create_ordered_vertex_list()
{
	if (vertices.empty())
		return {};

	std::vector<Vertex*> ordered_vertices;

	size_t num_vertices = vertices.size();
	for (size_t index_vertices = 0; index_vertices < num_vertices; index_vertices++)
	{
		ordered_vertices.push_back(&vertices[index_vertices]);
	}

	std::sort(ordered_vertices.begin(), ordered_vertices.end(), [](Vertex* a, Vertex* b) { return a->x == b->x ? a->y < b->y : a->x < b->x; });

	return ordered_vertices;
}

std::vector<DelauneyTriangulator::Vertex*> DelauneyTriangulator::remove_duplicates(const std::vector<Vertex*>& ordered_vertices)
{
	// Link duplicates and remove them from the ordered list:

	std::vector<Vertex*> filtered_vertices;
	Vertex* prev = nullptr;
	for (Vertex* v : ordered_vertices)
	{
		v->next = nullptr; // clear for all vertices just in case triangulate is ever called twice

		if (prev && prev->x == v->x && prev->y == v->y)
		{
			prev->next = v;
		}
		else
		{
			filtered_vertices.push_back(v);
		}

		prev = v;
	}
}

void DelauneyTriangulator::calculate_supertriangle(std::vector<Vertex*>& vertices, Triangle& super_triangle)
{
	// Find min and max values:

	size_t num_vertices = vertices.size();

	float min_x = 0.0f;
	float max_x = 0.0f;
	float min_y = 0.0f;
	float max_y = 0.0f;
	if (num_vertices > 0)
	{
		min_x = vertices[0]->x;
		max_x = vertices[0]->x;
		min_y = vertices[0]->y;
		max_y = vertices[0]->y;
	}

	for (size_t index_vertices = 1; index_vertices < num_vertices; index_vertices++)
	{
		Vertex* cur_vertex = vertices[index_vertices];

		min_x = std::min(min_x, cur_vertex->x);
		max_x = std::max(max_x, cur_vertex->x);
		min_y = std::min(min_y, cur_vertex->y);
		max_y = std::max(max_y, cur_vertex->y);
	}

	// Setup super triangle based on min/max values:

	float dx = max_x - min_x;
	float dy = max_y - min_y;
	float dmax = (dx > dy) ? dx : dy;
	float xmid = (max_x + min_x) * 0.5f;
	float ymid = (max_y + min_y) * 0.5f;

	super_triangle.A->x = xmid - 20.0f * dmax;
	super_triangle.A->y = ymid - dmax;
	super_triangle.A->data = nullptr;

	super_triangle.B->x = xmid;
	super_triangle.B->y = ymid + 20.0f * dmax;
	super_triangle.B->data = nullptr;

	super_triangle.C->x = xmid + 20.0f * dmax;
	super_triangle.C->y = ymid - dmax;
	super_triangle.C->data = nullptr;

	calc_cirumcenter(super_triangle);
}

std::vector<DelauneyTriangulator::Triangle> DelauneyTriangulator::perform_delauney_triangulation(const std::vector<Vertex*>& vertices, const Triangle& super_triangle)
{
	std::vector<Triangle> triangles;

	// add supertriangle vertices to the end of the vertex list
	triangles.push_back(super_triangle);

	std::vector<Triangle_Edge> edges;

	// for each sample point in the vertex list:
	size_t num_vertices = vertices.size();
	for (size_t index_vertices = 0; index_vertices < num_vertices; index_vertices++)
	{
		Vertex* insertion_point = vertices[index_vertices];

		edges.clear();

		// For each triangle currently in the triangle list
		std::vector<Triangle>::size_type index_triangles, num_triangles;
		num_triangles = triangles.size();
		for (index_triangles = 0; index_triangles < num_triangles; index_triangles++)
		{
			Triangle& cur_triangle = triangles[index_triangles];

			// Check if the point lies in the triangle circumcircle:
			float dist_x = insertion_point->x - cur_triangle.circumcenter_x;
			float dist_y = insertion_point->y - cur_triangle.circumcenter_y;
			float dist2 = dist_x * dist_x + dist_y * dist_y;
			if (dist2 < cur_triangle.radius2)
			{
				// Add triangle edges to edge buffer:
				edges.push_back(Triangle_Edge(cur_triangle.A, cur_triangle.B));
				edges.push_back(Triangle_Edge(cur_triangle.B, cur_triangle.C));
				edges.push_back(Triangle_Edge(cur_triangle.C, cur_triangle.A));

				// Remove triange from triangle list:
				triangles.erase(triangles.begin() + index_triangles);
				index_triangles--;
				num_triangles--;
			}
		}

		// Delete all doubly specified edges from the edge buffer. This leaves the edges of the enclosing polygon only
		int64_t index_edges1, index_edges2, num_edges; // intentionally integer to allow index to be negative when deleting index.
		num_edges = (int64_t)edges.size();
		for (index_edges1 = 0; index_edges1 < num_edges; index_edges1++)
		{
			Triangle_Edge& edge1 = edges[index_edges1];
			for (index_edges2 = 0/*index_edges1+1*/; index_edges2 < num_edges; index_edges2++)
			{
				if (index_edges1 == index_edges2) continue;
				Triangle_Edge& edge2 = edges[index_edges2];
				if ((edge1.first == edge2.first && edge1.second == edge2.second) ||
					(edge1.second == edge2.first && edge1.first == edge2.second))
				{
					// Same edges, delete both:
					if (index_edges1 < index_edges2)
					{
						edges.erase(edges.begin() + index_edges2);
						edges.erase(edges.begin() + index_edges1);
					}
					else
					{
						edges.erase(edges.begin() + index_edges1);
						edges.erase(edges.begin() + index_edges2);
					}
					num_edges -= 2;
					index_edges1--;
					break;
				}
			}
		}

		// add to the triangle list all triangles formed between the point and the edges of the enclosing polygon
		for (index_edges1 = 0; index_edges1 < num_edges; index_edges1++)
		{
			Triangle triangle;
			triangle.A = edges[index_edges1].first;
			triangle.B = edges[index_edges1].second;
			triangle.C = insertion_point;
			calc_cirumcenter(triangle);
			triangles.push_back(triangle);
		}
	}

	// remove any triangles from the triangle list that use the supertriangle vertices
	size_t num_triangles = triangles.size();
	for (size_t index_triangles = 0; index_triangles < num_triangles; index_triangles++)
	{
		Triangle& cur_triangle = triangles[index_triangles];

		if (
			cur_triangle.A == super_triangle.A ||
			cur_triangle.A == super_triangle.B ||
			cur_triangle.A == super_triangle.C ||
			cur_triangle.B == super_triangle.A ||
			cur_triangle.B == super_triangle.B ||
			cur_triangle.B == super_triangle.C ||
			cur_triangle.C == super_triangle.A ||
			cur_triangle.C == super_triangle.B ||
			cur_triangle.C == super_triangle.C)
		{
			// triangle shares one or more points with supertriangle, remove it:
			triangles.erase(triangles.begin() + index_triangles);
			index_triangles--;
			num_triangles--;
		}
	}

	return triangles;
}

void DelauneyTriangulator::calc_cirumcenter(Triangle& triangle)
{
	float a_0 = triangle.A->x;
	float a_1 = triangle.A->y;
	float b_0 = triangle.B->x;
	float b_1 = triangle.B->y;
	float c_0 = triangle.C->x;
	float c_1 = triangle.C->y;

	float A = b_0 - a_0;
	float B = b_1 - a_1;
	float C = c_0 - a_0;
	float D = c_1 - a_1;

	float E = A * (a_0 + b_0) + B * (a_1 + b_1);
	float F = C * (a_0 + c_0) + D * (a_1 + c_1);

	float G = 2.0f * (A * (c_1 - b_1) - B * (c_0 - b_0));

	float p_0 = (D * E - B * F) / G;
	float p_1 = (A * F - C * E) / G;

	triangle.circumcenter_x = p_0;
	triangle.circumcenter_y = p_1;
	float radius_x = triangle.A->x - triangle.circumcenter_x;
	float radius_y = triangle.A->y - triangle.circumcenter_y;
	triangle.radius2 = radius_x * radius_x + radius_y * radius_y;
}
