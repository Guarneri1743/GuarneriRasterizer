#ifndef _TRIANGLE_
#define _TRIANGLE_
#include <CPURasterizer.hpp>

namespace Guarneri
{
	struct Triangle
	{
	public:
		Vertex vertices[3];
		bool flip;
		bool culled;

	public:
		Triangle();
		Triangle(const Vertex verts[3]);
		Triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3);
		Triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3, const bool& flip);
		void interpolate(const float& screen_y, Vertex& lhs, Vertex& rhs) const;
		std::vector<Triangle> horizontally_split() const;
		float area() const;
		float area_double() const;
		static float area_double(const Vector2& v1, const Vector2& v2, const Vector2& v3);
		static float area_double(const Vector3& v1, const Vector3& v2, const Vector3& v3);
		static float area(const Vector3& v1, const Vector3& v2, const Vector3& v3);
		Vertex& operator[](const uint32_t i);
		const Vertex& operator[](const uint32_t i) const;
		std::string str() const;
	};


	Triangle::Triangle()
	{
		culled = false;
		flip = false;
	}

	Triangle::Triangle(const Vertex verts[3])
	{
		culled = false;
		flip = false;
		for (int i = 0; i < 3; i++)
		{
			vertices[i] = verts[i];
		}
	}

	Triangle::Triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3)
	{
		culled = false;
		flip = false;
		vertices[0] = v1;
		vertices[1] = v2;
		vertices[2] = v3;
	}

	Triangle::Triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3, const bool& flip)
	{
		vertices[0] = v1;
		vertices[1] = v2;
		vertices[2] = v3;
		this->flip = flip;
	}

	// scan top/bottom Triangle
	// top-left-right | bottom-left-right(flipped)
	//===================================================
	//       top[0]
	//        /\
		//	   	 /  \
		//----------------------------- scanline[screen_y]
		//	   /	  \
		//    /        \
		//    ----------
		// left[1]    right[2]
		//
		// left[1]    right[2]
		//    ----------
		//    \        /
		//	   \      /
		//----------------------------- scanline[screen_y]
		//	     \  /
		//        \/
		//		bottom[0]
		//====================================================
	void Triangle::interpolate(const float& screen_y, Vertex& lhs, Vertex& rhs) const
	{
		float len = this->vertices[0].position.y - this->vertices[2].position.y;
		len = flip ? len : -len;
		float dy = flip ? screen_y - this->vertices[2].position.y : screen_y - this->vertices[0].position.y;
		float t = dy / len;
		assert(t > 0 && dy > 0);
		int l0, l1, r0, r1;
		l0 = flip ? 1 : 0;
		r0 = flip ? 2 : 0;
		l1 = flip ? 0 : 1;
		r1 = flip ? 0 : 2;
		lhs = Vertex::interpolate(this->vertices[l0], this->vertices[l1], t);
		rhs = Vertex::interpolate(this->vertices[r0], this->vertices[r1], t);
	}

	// split a Triangle to 1-2 triangles
	//===================================================
	//       top[0]
	//        /\
		//	   	 /  \
		//	    /	 \
		//	   /      \
		//     --------
		// left[1]    right[2]
		//
		// left[1]    right[2]
		//     --------
		//	   \      /
		//	    \    /
		//	     \  /
		//        \/
		//	    bottom[0]
		//====================================================
	std::vector<Triangle> Triangle::horizontally_split() const
	{
		std::vector<Triangle> ret;

		std::vector<Vertex> sorted;
		sorted.emplace_back(vertices[0]);
		sorted.emplace_back(vertices[1]);
		sorted.emplace_back(vertices[2]);
		std::sort(sorted.begin(), sorted.end(), [](const Vertex& lhs, const Vertex& rhs)
		{
			return lhs.position.y < rhs.position.y;
		});

		assert(sorted[0].position.y <= sorted[1].position.y);
		assert(sorted[1].position.y <= sorted[2].position.y);

		// Line
		if (sorted[0].position.y == sorted[1].position.y && sorted[1].position.y == sorted[2].position.y)
		{
			return ret;
		}

		// top Triangle
		if (sorted[1].position.y == sorted[2].position.y)
		{
			if (sorted[1].position.x >= sorted[2].position.x)
			{
				ret.emplace_back(Triangle(sorted[0], sorted[2], sorted[1]));
			}
			else
			{
				ret.emplace_back(Triangle(sorted[0], sorted[1], sorted[2]));
			}
			return ret;
		}

		// bottom Triangle
		if (sorted[0].position.y == sorted[1].position.y)
		{
			if (sorted[0].position.x >= sorted[1].position.x)
			{
				ret.emplace_back(Triangle(sorted[2], sorted[1], sorted[0], true));
			}
			else
			{
				ret.emplace_back(Triangle(sorted[2], sorted[0], sorted[1], true));
			}
			return ret;
		}

		// split triangles
		float mid_y = sorted[1].position.y;

		float t = (mid_y - sorted[0].position.y) / (sorted[2].position.y - sorted[0].position.y);

		// interpolate new Vertex
		Vertex v = Vertex::interpolate(sorted[0], sorted[2], t);

		// top Triangle: top-left-right
		if (v.position.x >= sorted[1].position.x)
		{
			ret.emplace_back(Triangle(sorted[0], sorted[1], v));
		}
		else
		{
			ret.emplace_back(Triangle(sorted[0], v, sorted[1]));
		}

		// bottom Triangle: bottom-left-right
		if (v.position.x >= sorted[1].position.x)
		{
			ret.emplace_back(Triangle(sorted[2], sorted[1], v, true));
		}
		else
		{
			ret.emplace_back(Triangle(sorted[2], v, sorted[1], true));
		}

		return ret;
	}

	float Triangle::area() const
	{
		auto e1 = vertices[1].position - vertices[0].position;
		auto e2 = vertices[2].position - vertices[0].position;
		return Vector3::cross(e1.xyz(), e2.xyz()).magnitude() / 2.0f;
	}

	float Triangle::area_double() const
	{
		auto e1 = vertices[1].position - vertices[0].position;
		auto e2 = vertices[2].position - vertices[0].position;
		return Vector3::cross(e1.xyz(), e2.xyz()).magnitude();
	}

	float Triangle::area_double(const Vector2& v1, const Vector2& v2, const Vector2& v3)
	{
		return (v3.x - v1.x) * (v2.y - v1.y) - (v3.y - v1.y) * (v2.x - v1.x);
	}

	float Triangle::area_double(const Vector3& v1, const Vector3& v2, const Vector3& v3)
	{
		auto e1 = v2 - v1;
		auto e2 = v3 - v1;
		return Vector3::cross(e1, e2).magnitude();
	}

	float Triangle::area(const Vector3& v1, const Vector3& v2, const Vector3& v3)
	{
		auto e1 = v2 - v1;
		auto e2 = v3 - v1;
		return Vector3::cross(e1, e2).magnitude() / 2.0f;
	}

	Vertex& Triangle::operator[](const uint32_t i)
	{
		return vertices[i];
	}

	const Vertex& Triangle::operator[](const uint32_t i) const
	{
		return vertices[i];
	}

	std::string Triangle::str() const
	{
		std::stringstream ss;
		ss << "Triangle: [v0: " << this->vertices[0] << ", v1: " << this->vertices[1] << ", v2: " << this->vertices[2] << "]";
		return ss.str();
	}

	static std::ostream& operator << (std::ostream& stream, const Triangle& tri)
	{
		stream << tri.str();
		return stream;
	}

	static std::stringstream& operator << (std::stringstream& stream, const Triangle& tri)
	{
		stream << tri.str();
		return stream;
	}
}
#endif