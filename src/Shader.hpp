#ifndef _SHADER_
#define _SHADER_
#include <Guarneri.hpp>

namespace Guarneri
{
	struct a2v
	{
		Vector4 position;
		Vector2 uv;
		Vector4 color;
		Vector3 normal;
		Vector3 tangent;
	};

	struct v2f
	{
		Vector4 position;
		Vector3 world_pos;
		Vector2 uv;
		Vector4 color;
		Vector3 tangent;
		Vector3 bitangent;
		Vector3 normal;
		Vector4 shadow_coord;
	};

	struct LightingData
	{
		LightingData()
		{
			glossiness = 32.0f;
		}
		float glossiness;
	};

	class Shader : public Object
	{
	public:
		Matrix4x4 model, view, projection;
		std::unordered_map<property_name, float> name2float;
		std::unordered_map<property_name, Vector4> name2float4;
		std::unordered_map<property_name, int> name2int;
		std::unordered_map<property_name, std::shared_ptr<Texture>> name2tex;
		std::unordered_map<property_name, std::shared_ptr<CubeMap>> name2cubemap;
		std::unordered_map<property_name, std::string> keywords;
		RawBuffer<float>* shadowmap;
		ColorMask color_mask;
		CompareFunc stencil_func;
		StencilOp stencil_pass_op;
		StencilOp stencil_fail_op;
		StencilOp stencil_zfail_op;
		uint8_t stencil_ref_val;
		uint8_t stencil_write_mask;
		uint8_t stencil_read_mask;
		CompareFunc ztest_func;
		ZWrite zwrite_mode;
		BlendFactor src_factor;
		BlendFactor dst_factor;
		BlendOp blend_op;
		bool transparent;
		bool double_face;
		bool skybox;
		bool shadow;
		LightingData lighting_param;
		bool discarded = false;
		bool normal_map = false;

	public:
		Shader();
		virtual ~Shader();
		virtual v2f vertex_shader(const a2v& input) const;
		float get_shadow_atten(const Vector4& light_space_pos) const;
		Vector3 reflect(const Vector3& n, const Vector3& light_out_dir) const;
		Color calculate_main_light(const DirectionalLight& light, const LightingData& lighting_data, const Vector3& v, const Vector3& n, Color main_tex, Color spec_tex, Color ao, const Matrix3x3& tbn) const;
		Color calculate_point_light(const PointLight& light, const LightingData& lighting_data, const Vector3& wpos, const Vector3& v, const Vector3& n, Color main_tex, Color spec_tex, Color ao, const Matrix3x3& tbn) const;
		virtual Color fragment_shader(const v2f& input) const;
		std::string str() const;
	};


	Shader::Shader()
	{
		this->color_mask = (ColorMask::R | ColorMask::G | ColorMask::B | ColorMask::A);
		this->stencil_func = CompareFunc::ALWAYS;
		this->stencil_pass_op = StencilOp::KEEP;
		this->stencil_fail_op = StencilOp::KEEP;
		this->stencil_zfail_op = StencilOp::KEEP;
		this->stencil_read_mask = 0xFF;
		this->stencil_write_mask = 0xFF;
		this->stencil_ref_val = 0;
		this->ztest_func = CompareFunc::LEQUAL;
		this->zwrite_mode = ZWrite::ON;
		this->src_factor = BlendFactor::SRC_ALPHA;
		this->dst_factor = BlendFactor::ONE_MINUS_SRC_ALPHA;
		this->blend_op = BlendOp::ADD;
		this->lighting_param = LightingData();
		this->transparent = false;
		this->double_face = false;
		this->skybox = false;
		this->shadow = false;
		this->shadowmap = nullptr;
	}

	Shader::~Shader()
	{}

	v2f Shader::vertex_shader(const a2v& input) const
	{
		v2f o;
		auto opos = Vector4(input.position.xyz(), 1.0f);
		auto wpos = model * opos;
		auto cpos = projection * view * wpos;
		auto light_space_pos = misc_param.main_light.light_space() * Vector4(wpos.xyz(), 1.0f);
		o.position = cpos;
		o.world_pos = wpos.xyz();
		o.shadow_coord = light_space_pos;
		o.color = input.color;
		Matrix3x3 normal_matrix = Matrix3x3(model).inverse().transpose();
		if (normal_map)
		{
			Vector3 t = (normal_matrix * input.tangent).normalized();
			Vector3 n = (normal_matrix * input.normal).normalized();
			t = (t - Vector3::dot(t, n) * n).normalized();
			Vector3 b = Vector3::cross(n, t);
			o.tangent = t;
			o.bitangent = b;
			o.normal = n;
		}
		else
		{
			o.normal = normal_matrix * input.normal;
		}
		o.uv = input.uv;
		return o;
	}

	float Shader::get_shadow_atten(const Vector4& light_space_pos) const
	{
		if (shadowmap == nullptr)
		{
			return 0.0f;
		}

		Vector3 proj_shadow_coord = light_space_pos.xyz();
		proj_shadow_coord = proj_shadow_coord * 0.5f + 0.5f;

		if (proj_shadow_coord.z > 1.0f)
		{
			return 0.0f;
		}

		float shadow_atten = 0.0f;

		Vector2 texel_size = 1.0f / Vector2(shadowmap->height, shadowmap->width);

		if (misc_param.pcf_on)
		{
			// PCF
			for (int x = -1; x <= 1; x++)
			{
				for (int y = -1; y <= 1; y++)
				{
					float depth;
					if (shadowmap->read(proj_shadow_coord.x + (float)x * texel_size.x, proj_shadow_coord.y + (float)y * texel_size.y, depth))
					{
						//printf("shadowmap: %f depth: %f\n", depth, proj_shadow_coord.z);
						shadow_atten += (proj_shadow_coord.z - 0.005f) > depth ? 1.0f : 0.0f;
					}
				}
			}

			shadow_atten /= 9.0f;
		}
		else
		{
			float depth;
			if (shadowmap->read(proj_shadow_coord.x, proj_shadow_coord.y, depth))
			{
				//printf("shadowmap: %f depth: %f\n", depth, proj_shadow_coord.z);
				shadow_atten = (proj_shadow_coord.z - 0.0005f) > depth ? 1.0f : 0.0f;
			}
		}

		return shadow_atten * 0.5f;
	}

	Vector3 Shader::reflect(const Vector3& n, const Vector3& light_out_dir) const
	{
		auto ndl = std::max(Vector3::dot(n, light_out_dir), 0.0f);
		return 2.0f * n * ndl - light_out_dir;
	}

	Color Shader::calculate_main_light(const DirectionalLight& light, const LightingData& lighting_data, const Vector3& v, const Vector3& n, Color main_tex, Color spec_tex, Color ao, const Matrix3x3& tbn) const
	{
		auto light_ambient = light.ambient;
		auto light_spec = light.specular;
		auto light_diffuse = light.diffuse;
		auto intensity = light.intensity;
		REF(intensity);
		light_diffuse *= intensity;
		light_spec *= intensity;

		float glossiness = std::clamp(lighting_data.glossiness, 0.0f, 256.0f);

		auto light_dir = -light.forward.normalized();
		light_dir = tbn * light_dir;

		auto ndl = std::max(Vector3::dot(n, light_dir), 0.0f);
		auto half_dir = (light_dir + v).normalized();
		auto spec = std::pow(std::max(Vector3::dot(n, half_dir), 0.0f), glossiness);

		auto ambient = light_ambient * ao;
		auto diffuse = Color::saturate(light_diffuse * ndl * main_tex);
		auto specular = Color::saturate(light_spec * spec * spec_tex);


		if ((misc_param.render_flag & RenderFlag::SPECULAR) != RenderFlag::DISABLE)
		{
			return specular;
		}

		return ambient + diffuse + specular;
	}

	Color Shader::calculate_point_light(const PointLight& light, const LightingData& lighting_data, const Vector3& wpos, const Vector3& v, const Vector3& n, Color main_tex, Color spec_tex, Color ao, const Matrix3x3& tbn) const
	{
		auto light_ambient = light.ambient;
		auto light_spec = light.specular;
		auto light_diffuse = light.diffuse;
		auto intensity = light.intensity;
		REF(intensity);
		light_diffuse *= intensity;
		light_spec *= intensity;
		float glossiness = std::clamp(lighting_data.glossiness, 0.0f, 256.0f);
		auto light_dir = (light.position - wpos).normalized();
		light_dir = tbn * light_dir;

		auto ndl = std::max(Vector3::dot(n, light_dir), 0.0f);
		auto half_dir = (light_dir + v).normalized();
		auto spec = std::pow(std::max(Vector3::dot(n, half_dir), 0.0f), glossiness);
		float distance = Vector3::length(light.position, wpos);
		float atten = 1.0f / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

		auto ambient = light_ambient * ao;
		auto diffuse = Color::saturate(light_diffuse * ndl * main_tex);
		auto specular = Color::saturate(light_spec * spec * spec_tex);

		if ((misc_param.render_flag & RenderFlag::SPECULAR) != RenderFlag::DISABLE)
		{
			return specular * atten;
		}

		return (ambient + diffuse + specular) * atten;
	}

	Color Shader::fragment_shader(const v2f& input) const
	{
		auto main_light = misc_param.main_light;
		auto point_lights = misc_param.point_lights;

		Vector3 cam_pos = misc_param.camera_pos;
		Vector3 wpos = input.world_pos;
		Vector4 screen_pos = input.position;

		Vector3 normal = input.normal.normalized();
		Vector3 view_dir = (cam_pos - wpos).normalized();

		Color normal_tex;
		Matrix3x3 tbn;
		if (name2tex.count(normal_prop) > 0 && name2tex.at(normal_prop)->sample(input.uv.x, input.uv.y, normal_tex))
		{
			tbn = Matrix3x3(input.tangent, input.bitangent, input.normal);
			view_dir = tbn * view_dir;
			auto packed_normal = Vector3(normal_tex.r, normal_tex.g, normal_tex.b);
			normal = (packed_normal * 2.0f - 1.0f).normalized();
		}

		//shadow
		float shadow_atten = 1.0f - get_shadow_atten(input.shadow_coord);

		Color ret = Color::BLACK;
		Color main_tex = Color::WHITE;
		name2tex.count(albedo_prop) > 0 && name2tex.at(albedo_prop)->sample(input.uv.x, input.uv.y, main_tex);

		Color spec_tex = Color::WHITE;
		name2tex.count(specular_prop) > 0 && name2tex.at(specular_prop)->sample(input.uv.x, input.uv.y, spec_tex);

		Color ao = Color::WHITE;
		name2tex.count(ao_prop) > 0 && name2tex.at(ao_prop)->sample(input.uv.x, input.uv.y, ao);

		ret += calculate_main_light(main_light, lighting_param, view_dir, normal, main_tex, spec_tex, ao, tbn);

		for (auto& light : point_lights)
		{
			ret += calculate_point_light(light, lighting_param, wpos, view_dir, normal, main_tex, spec_tex, ao, tbn);
		}

		ret *= shadow_atten;

		// todo: ddx ddy
		/*if ((misc_param.render_flag & RenderFlag::MIPMAP) != RenderFlag::DISABLE) {
			Vector2 ddx_uv = ddx.uv;
			Vector2 ddy_uv = ddy.uv;
			float rho = std::max(std::sqrt(Vector2::dot(ddx_uv, ddx_uv)), std::sqrt(Vector2::dot(ddy_uv, ddy_uv)));
			float logrho = std::log2(rho);
			int mip = std::max(int(logrho + 0.5f), 0);
			if (mip == 0) {
				return Color(1.0f, 0.0f, 0.0f, 1.0f);
			}
			else if (mip == 1) {
				return Color(0.0f, 1.0f, 0.0f, 1.0f);
			}
			else if (mip == 2) {
				return Color(0.0f, 0.0f, 1.0f, 1.0f);
			}
			else if (mip == 3) {
				return Color(1.0f, 0.0f, 1.0f, 1.0f);
			}
			else if (mip == 4) {
				return Color(0.0f, 1.0f, 1.0f, 1.0f);
			}
			else if (mip == 5) {
				return Color(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else if (mip == 6) {
				return Color(0.5f, 0.0f, 0.5f, 1.0f);
			}
			else if (mip == 7) {
				return Color(0.0f, 0.5f, 0.5f, 1.0f);
			}
			else {
				return Color(0.5f, 0.5f, 0.5f, 1.0f);
			}
		}*/

		if ((misc_param.render_flag & RenderFlag::UV) != RenderFlag::DISABLE)
		{
			return input.uv;
		}

		if ((misc_param.render_flag & RenderFlag::VERTEX_COLOR) != RenderFlag::DISABLE)
		{
			return input.color;
		}

		if ((misc_param.render_flag & RenderFlag::NORMAL) != RenderFlag::DISABLE)
		{
			return  input.normal.normalized();
		}

		ret = Color::saturate(ret);
		return Color(ret.r, ret.g, ret.b, 1.0f);
	}

	std::string Shader::str() const
	{
		std::stringstream ss;
		ss << "Shader[" << this->id << "]" << std::endl;
		return ss.str();
	}
}
#endif