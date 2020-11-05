#pragma once
#include <guarneri.hpp>
#include <singleton.hpp>

namespace guarneri {
	class camera : public object {
	public:
		~camera(){}

	public:
		float fov;
		float aspect;
		float near;
		float far;
		//transform transform;
		float3 forward;
		float3 right;
		float3 up;
		float3 position;

	private:
		mat4 p;
		mat4 v;
		float yaw;
		float pitch;
		projection proj_type;

	public:
		static std::unique_ptr<camera> create(const float3& position_t, const float& aspect_t, const float& fov_t, const float& near_t, const float& far_t, const projection& proj_type_t) {
			auto ret = std::make_unique<camera>();
			ret->initialize(position_t, aspect_t, fov_t, near_t, far_t, proj_type_t);
			return ret;
		}

		void initialize(const float3& position_t, const float& aspect_t, const float& fov_t, const float& near_t, const float& far_t, const projection& proj_type_t) {
			this->position = position_t;
			this->aspect = aspect_t;
			this->fov = fov_t;
			this->near = near_t;
			this->far = far_t;
			this->yaw = 0.0f;
			this->pitch = 0.0f;
			this->proj_type = proj_type_t;
			update_camera();
			update_proj_mode();
		}

		mat4 view_matrix() const {
			return  mat4::lookat(position, position + forward, float3::UP);
		}

		const mat4 projection_matrix() const{
			return p;
		}

		void move(const float3& t) {
			this->position += t;
		}

		void move_forward(const float& distance) {
			this->position += distance * this->forward;
		}

		void move_backward(const float& distance) {
			this->position -= distance * this->forward;
		}

		void move_left(const float& distance) {
			this->position -= distance * this->right;
		}

		void move_right(const float& distance) {
			this->position += distance * this->right;
		}

		void move_ascend(const float& distance) {
			this->position += distance * this->up;
		}

		void move_descend(const float& distance) {
			this->position -= distance * this->up;
		}

		void rotate(const float& yaw_offset, const float& pitch_offset) {
			this->yaw -= yaw_offset;
			this->pitch -= pitch_offset;
			if (this->pitch > 89.0f)
			{
				this->pitch = 89.0f;
			}
			if (this->pitch < -89.0f)
			{
				this->pitch = -89.0f;
			}
			update_camera();
		}

		void lookat(const float3& target) {
			this->forward = float3::normalize(target - this->position);
		}

		void update_camera() {
			forward.x = cos(DEGREE2RAD(this->yaw)) * cos(DEGREE2RAD(this->pitch));
			forward.y = sin(DEGREE2RAD(this->pitch));
			forward.z = sin(DEGREE2RAD(this->yaw)) * cos(DEGREE2RAD(this->pitch));
			forward = float3::normalize(forward);
			float3::calculate_right_up(forward, right, up);
		} 

		//todo: ortho
		void update_proj_mode(){
			switch (this->proj_type) {
			case projection::perspective:
				this->p = mat4::perspective(this->fov, this->aspect, this->near, this->far);
				break;
			case projection::orthographic:
				this->p = mat4::perspective(this->fov, this->aspect, this->near, this->far);
				break;
			default:
				this->p = mat4::perspective(this->fov, this->aspect, this->near, this->far);
			}
		}

		public:
			std::string str() const {
				std::stringstream ss;
				ss << "Camera[" << this->id << "  pos: "<<this->position << "]";
				return ss.str();
			}
	};
}