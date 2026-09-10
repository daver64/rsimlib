/** @file
 * @brief Implements shader-based bloom, vignette, and screen-fade effects.
 */

#define GL_GLEXT_PROTOTYPES

#include "graphics_fx.h"

#include "display.h"
#include "draw.h"
#include "gl2d.h"

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace sl
{
	namespace
	{

		constexpr const char *fullscreen_vertex_source = R"(
#version 430 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 uProjection;

out vec2 uv;

void main() {
	gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
	uv = aTexCoord;
}
)";

		constexpr const char *bright_pass_fragment_source = R"(
#version 430 core
uniform sampler2D source;
uniform float threshold;
in vec2 uv;
out vec4 fragColor;

void main() {
	vec3 colour = texture(source, uv).rgb;
	fragColor = vec4(max(colour - vec3(threshold), vec3(0.0)), 1.0);
}
)";

		constexpr const char *blur_fragment_source = R"(
#version 430 core
uniform sampler2D source;
uniform vec2 texel;
uniform vec2 direction;
uniform float radius;
in vec2 uv;
out vec4 fragColor;

void main() {
	// 9-tap separable Gaussian
	float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
	vec3 result = texture(source, uv).rgb * weights[0];
	for (int i = 1; i < 5; ++i) {
		vec2 offset = direction * texel * radius * float(i);
		result += texture(source, uv + offset).rgb * weights[i];
		result += texture(source, uv - offset).rgb * weights[i];
	}
	fragColor = vec4(result, 1.0);
}
)";

		constexpr const char *composite_fragment_source = R"(
#version 430 core
uniform sampler2D source;
uniform sampler2D bloomTex;
uniform float intensity;
in vec2 uv;
out vec4 fragColor;

void main() {
	vec4 base = texture(source, uv);
	vec3 bloom = texture(bloomTex, uv).rgb;
	fragColor = vec4(base.rgb + bloom * intensity, base.a);
}
)";

		constexpr const char *vignette_fragment_source = R"(
#version 430 core
uniform sampler2D source;
uniform float radius;
uniform float softness;
uniform float intensity;
in vec2 uv;
out vec4 fragColor;

void main() {
	vec4 base = texture(source, uv);
	vec2 centred = uv - vec2(0.5);
	centred.x *= float(textureSize(source, 0).x) / float(textureSize(source, 0).y);
	float dist = length(centred);
	float inner = max(radius - softness, 0.0);
	float t = clamp((dist - inner) / max(softness, 0.0001), 0.0, 1.0);
	fragColor = vec4(base.rgb * (1.0 - t * intensity), base.a);
}
)";

		constexpr const char *lighting_fragment_source = R"(
#version 430 core
uniform sampler2D source;
uniform int lightCount;
uniform int shadowLightCount;
uniform float ambient;
uniform int flipVertical;
uniform sampler2D shadowMasks[8];
uniform ivec2 tileCount;

struct GpuLight {
	vec4 positionRadius;
	vec4 colourIntensity;
	vec4 shadowSoftness;
};

layout(std430, binding = 2) readonly buffer LightBuffer {
	GpuLight lights[];
};
layout(std430, binding = 3) readonly buffer TileCounts {
	uint tileCounts[];
};
layout(std430, binding = 4) readonly buffer TileIndices {
	uint tileIndices[];
};
in vec2 uv;
out vec4 fragColor;

void main() {
	vec2 lightUv = uv;
	if (flipVertical != 0) {
		lightUv.y = 1.0 - lightUv.y;
	}
	vec4 base = texture(source, uv);
	vec2 pixelPosition = lightUv * vec2(textureSize(source, 0));
	ivec2 tile = ivec2(pixelPosition / 16.0);
	int tileIndex = tile.y * tileCount.x + tile.x;
	uint tileLightCount = tileCounts[tileIndex];
	vec3 illumination = vec3(ambient);
	for (uint tileLight = 0u; tileLight < tileLightCount; ++tileLight) {
		int index = int(tileIndices[tileIndex * 128 + tileLight]);
		GpuLight light = lights[index];
		float distanceToLight = distance(pixelPosition, light.positionRadius.xy);
		float falloff = 1.0 - smoothstep(0.0, max(light.positionRadius.z, 0.0001), distanceToLight);
		float shadow = 1.0;
		if (index < shadowLightCount) {
			vec2 shadowTexel = 1.0 / vec2(textureSize(shadowMasks[index], 0));
			shadow = 0.0;
			for (int offsetY = -1; offsetY <= 1; ++offsetY) {
				for (int offsetX = -1; offsetX <= 1; ++offsetX) {
					vec2 offset = vec2(offsetX, offsetY) * shadowTexel * max(light.shadowSoftness.x, 0.0);
					shadow += texture(shadowMasks[index], lightUv + offset).r;
				}
			}
			shadow /= 9.0;
		}
		illumination += light.colourIntensity.rgb * falloff * light.colourIntensity.a * shadow;
	}
	fragColor = vec4(base.rgb * illumination, base.a);
}
)";

		constexpr const char *light_cull_compute_source = R"(
#version 430 core
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

uniform ivec2 screenSize;
uniform ivec2 tileCount;
uniform int lightCount;

struct GpuLight {
	vec4 positionRadius;
	vec4 colourIntensity;
	vec4 shadowSoftness;
};

layout(std430, binding = 2) readonly buffer LightBuffer {
	GpuLight lights[];
};
layout(std430, binding = 3) writeonly buffer TileCounts {
	uint tileCounts[];
};
layout(std430, binding = 4) writeonly buffer TileIndices {
	uint tileIndices[];
};

void main() {
	ivec2 tile = ivec2(gl_GlobalInvocationID.xy);
	if (tile.x >= tileCount.x || tile.y >= tileCount.y) return;
	int tileIndex = tile.y * tileCount.x + tile.x;
	vec2 minimum = vec2(tile * 16);
	vec2 maximum = min(minimum + vec2(16), vec2(screenSize));
	uint count = 0u;
	for (int index = 0; index < lightCount; ++index) {
		vec2 closest = clamp(lights[index].positionRadius.xy, minimum, maximum);
		vec2 delta = lights[index].positionRadius.xy - closest;
		float radius = lights[index].positionRadius.z;
		if (dot(delta, delta) <= radius * radius && count < 128u) {
			tileIndices[tileIndex * 128 + count] = uint(index);
			count++;
		}
	}
	tileCounts[tileIndex] = count;
}
)";

		std::string shader_log(GLuint shader)
		{
			GLint length = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			if (length <= 1)
			{
				return "Shader compilation failed.";
			}
			std::vector<GLchar> log(static_cast<std::size_t>(length));
			glGetShaderInfoLog(shader, length, nullptr, log.data());
			return log.data();
		}

		std::string program_log(GLuint program)
		{
			GLint length = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			if (length <= 1)
			{
				return "Shader linking failed.";
			}
			std::vector<GLchar> log(static_cast<std::size_t>(length));
			glGetProgramInfoLog(program, length, nullptr, log.data());
			return log.data();
		}

		GLuint compile_shader(GLenum type, const std::string &source, std::string &error)
		{
			const GLuint shader = glCreateShader(type);
			if (shader == 0)
			{
				error = "Unable to create shader.";
				return 0;
			}

			const char *shaderSource = source.c_str();
			glShaderSource(shader, 1, &shaderSource, nullptr);
			glCompileShader(shader);

			GLint compiled = GL_FALSE;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
			if (compiled == GL_TRUE)
			{
				return shader;
			}

			error = shader_log(shader);
			glDeleteShader(shader);
			return 0;
		}

		/** Draw a textured quad at (x, y, width, height), used by every bloom pass. */
		void submit_fullscreen_quad(int x, int y, int width, int height, GLuint texture, bool flipVertical)
		{
			const float left = static_cast<float>(x);
			const float top = static_cast<float>(y);
			const float right = static_cast<float>(x + width);
			const float bottom = static_cast<float>(y + height);
			const float topV = flipVertical ? 1.0f : 0.0f;
			const float bottomV = flipVertical ? 0.0f : 1.0f;
			const detail::GLVertex vertices[4] = {
				{left, top, 0.0f, topV, 1.0f, 1.0f, 1.0f, 1.0f},
				{right, top, 1.0f, topV, 1.0f, 1.0f, 1.0f, 1.0f},
				{right, bottom, 1.0f, bottomV, 1.0f, 1.0f, 1.0f, 1.0f},
				{left, bottom, 0.0f, bottomV, 1.0f, 1.0f, 1.0f, 1.0f},
			};
			detail::gl2d_submit(GL_TRIANGLE_FAN, vertices, 4, texture);
		}

		struct Point
		{
			float x;
			float y;
		};

		struct GpuLight
		{
			float positionRadius[4];
			float colourIntensity[4];
			float shadowSoftness[4];
		};

		Point project_from_light(Point point, const Light &light, float distance)
		{
			const float dx = point.x - light.x;
			const float dy = point.y - light.y;
			const float length = std::sqrt(dx * dx + dy * dy);
			if (length <= 0.0001f)
			{
				return point;
			}
			return {point.x + dx / length * distance, point.y + dy / length * distance};
		}

		void fill_shadow_triangle(Bitmap *mask, Point first, Point second, Point third)
		{
			const float area = (second.x - first.x) * (third.y - first.y) -
				(second.y - first.y) * (third.x - first.x);
			if (std::abs(area) <= 0.0001f)
			{
				return;
			}

			const int minimumX = std::max(0, static_cast<int>(std::floor(std::min({first.x, second.x, third.x}))));
			const int maximumX = std::min(mask->width - 1, static_cast<int>(std::ceil(std::max({first.x, second.x, third.x}))));
			const int minimumY = std::max(0, static_cast<int>(std::floor(std::min({first.y, second.y, third.y}))));
			const int maximumY = std::min(mask->height - 1, static_cast<int>(std::ceil(std::max({first.y, second.y, third.y}))));
			if (minimumX > maximumX || minimumY > maximumY)
			{
				return;
			}

			for (int y = minimumY; y <= maximumY; ++y)
			{
				for (int x = minimumX; x <= maximumX; ++x)
				{
					const Point sample{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
					const float edgeA = (second.x - first.x) * (sample.y - first.y) -
						(second.y - first.y) * (sample.x - first.x);
					const float edgeB = (third.x - second.x) * (sample.y - second.y) -
						(third.y - second.y) * (sample.x - second.x);
					const float edgeC = (first.x - third.x) * (sample.y - third.y) -
						(first.y - third.y) * (sample.x - third.x);
					if ((edgeA >= 0.0f && edgeB >= 0.0f && edgeC >= 0.0f) ||
						(edgeA <= 0.0f && edgeB <= 0.0f && edgeC <= 0.0f))
					{
						const std::size_t offset = (static_cast<std::size_t>(y) * mask->width + x) * 4;
						mask->pixels[offset] = 0;
						mask->pixels[offset + 1] = 0;
						mask->pixels[offset + 2] = 0;
						mask->pixels[offset + 3] = 255;
					}
				}
			}
		}

		void draw_shadow_edge(Bitmap *mask, Point first, Point second, const Light &light, float projectionDistance)
		{
			const Point edge{second.x - first.x, second.y - first.y};
			const Point normal{edge.y, -edge.x};
			const Point midpoint{(first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f};
			const float facing = (light.x - midpoint.x) * normal.x + (light.y - midpoint.y) * normal.y;
			if (facing <= 0.0f)
			{
				return;
			}

			const Point firstFar = project_from_light(first, light, projectionDistance);
			const Point secondFar = project_from_light(second, light, projectionDistance);
			fill_shadow_triangle(mask, first, second, secondFar);
			fill_shadow_triangle(mask, first, secondFar, firstFar);
		}

		void draw_shadow_caster(Bitmap *mask, const ShadowCaster &caster, const Light &light)
		{
			if (caster.vertices.size() < 2)
			{
				return;
			}
			const float projectionDistance = static_cast<float>(std::max(mask->width, mask->height)) * 4.0f;
			for (std::size_t index = 0; index < caster.vertices.size(); ++index)
			{
				const ShadowPoint &first = caster.vertices[index];
				const ShadowPoint &second = caster.vertices[(index + 1) % caster.vertices.size()];
				draw_shadow_edge(mask, {first.x, first.y}, {second.x, second.y}, light, projectionDistance);
			}
		}

	} // namespace

	ShadowCaster make_rectangle_shadow_caster(float left, float top, float right, float bottom)
	{
		return ShadowCaster{{
			{left, top},
			{right, top},
			{right, bottom},
			{left, bottom},
		}};
	}

	Shader::Shader(const std::string &vertexSource, const std::string &fragmentSource)
	{
		load(vertexSource, fragmentSource);
	}

	Shader::~Shader()
	{
		reset();
	}

	Shader::Shader(Shader &&other) noexcept
		: program_(std::exchange(other.program_, 0)), error_(std::move(other.error_))
	{
	}

	Shader &Shader::operator=(Shader &&other) noexcept
	{
		if (this != &other)
		{
			reset();
			program_ = std::exchange(other.program_, 0);
			error_ = std::move(other.error_);
		}
		return *this;
	}

	bool Shader::load(const std::string &vertexSource, const std::string &fragmentSource)
	{
		reset();

		const GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vertexSource, error_);
		if (vertexShader == 0)
		{
			return false;
		}

		const GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentSource, error_);
		if (fragmentShader == 0)
		{
			glDeleteShader(vertexShader);
			return false;
		}

		const GLuint program = glCreateProgram();
		if (program == 0)
		{
			error_ = "Unable to create shader program.";
			glDeleteShader(fragmentShader);
			glDeleteShader(vertexShader);
			return false;
		}

		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);
		glDeleteShader(fragmentShader);
		glDeleteShader(vertexShader);

		GLint linked = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked != GL_TRUE)
		{
			error_ = program_log(program);
			glDeleteProgram(program);
			return false;
		}

		program_ = program;
		error_.clear();
		return true;
	}

	bool Shader::load_compute(const std::string &computeSource)
	{
		reset();
		const GLuint computeShader = compile_shader(GL_COMPUTE_SHADER, computeSource, error_);
		if (computeShader == 0)
		{
			return false;
		}
		const GLuint program = glCreateProgram();
		if (program == 0)
		{
			error_ = "Unable to create compute shader program.";
			glDeleteShader(computeShader);
			return false;
		}
		glAttachShader(program, computeShader);
		glLinkProgram(program);
		glDeleteShader(computeShader);
		GLint linked = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked != GL_TRUE)
		{
			error_ = program_log(program);
			glDeleteProgram(program);
			return false;
		}
		program_ = program;
		error_.clear();
		return true;
	}

	void Shader::reset()
	{
		if (program_ != 0)
		{
			glDeleteProgram(static_cast<GLuint>(program_));
			program_ = 0;
		}
		error_.clear();
	}

	bool Shader::is_valid() const
	{
		return program_ != 0;
	}

	const std::string &Shader::error() const
	{
		return error_;
	}

	bool Shader::use() const
	{
		if (!is_valid())
		{
			return false;
		}
		glUseProgram(static_cast<GLuint>(program_));
		return true;
	}

	void Shader::stop()
	{
		glUseProgram(0);
	}

	bool Shader::dispatch_compute(unsigned int groupsX, unsigned int groupsY, unsigned int groupsZ) const
	{
		if (!use())
		{
			return false;
		}
		glDispatchCompute(groupsX, groupsY, groupsZ);
		return true;
	}

	bool Shader::set_uniform(const char *name, int value) const
	{
		if (!use())
		{
			return false;
		}
		const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
		if (location < 0)
		{
			return false;
		}
		glUniform1i(location, value);
		return true;
	}

	bool Shader::set_uniform(const char *name, float value) const
	{
		if (!use())
		{
			return false;
		}
		const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
		if (location < 0)
		{
			return false;
		}
		glUniform1f(location, value);
		return true;
	}

	bool Shader::set_uniform(const char *name, float x, float y) const
	{
		if (!use())
		{
			return false;
		}
		const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
		if (location < 0)
		{
			return false;
		}
		glUniform2f(location, x, y);
		return true;
	}

	bool Shader::set_uniform(const char *name, int x, int y) const
	{
		if (!use())
		{
			return false;
		}
		const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
		if (location < 0)
		{
			return false;
		}
		glUniform2i(location, x, y);
		return true;
	}

	bool Shader::set_uniform(const char *name, float x, float y, float z) const
	{
		if (!use())
		{
			return false;
		}
		const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
		if (location < 0)
		{
			return false;
		}
		glUniform3f(location, x, y, z);
		return true;
	}

	bool Shader::set_uniform_mat4(const char *name, const float *matrix4x4) const
	{
		if (!use() || !matrix4x4)
		{
			return false;
		}
		const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
		if (location < 0)
		{
			return false;
		}
		glUniformMatrix4fv(location, 1, GL_FALSE, matrix4x4);
		return true;
	}

	Bloom::~Bloom()
	{
		shutdown();
	}

	Bloom::Bloom(Bloom &&other) noexcept
		: brightShader_(std::move(other.brightShader_)),
		  blurShader_(std::move(other.blurShader_)),
		  compositeShader_(std::move(other.compositeShader_)),
		  threshold_(other.threshold_),
		  intensity_(other.intensity_),
		  radius_(other.radius_),
		  downsample_(other.downsample_),
		  blurTargetA_(std::exchange(other.blurTargetA_, nullptr)),
		  blurTargetB_(std::exchange(other.blurTargetB_, nullptr))
	{
	}

	Bloom &Bloom::operator=(Bloom &&other) noexcept
	{
		if (this != &other)
		{
			shutdown();
			brightShader_ = std::move(other.brightShader_);
			blurShader_ = std::move(other.blurShader_);
			compositeShader_ = std::move(other.compositeShader_);
			threshold_ = other.threshold_;
			intensity_ = other.intensity_;
			radius_ = other.radius_;
			downsample_ = other.downsample_;
			blurTargetA_ = std::exchange(other.blurTargetA_, nullptr);
			blurTargetB_ = std::exchange(other.blurTargetB_, nullptr);
		}
		return *this;
	}

	bool Bloom::initialise()
	{
		if (is_valid())
		{
			return true;
		}
		return brightShader_.load(fullscreen_vertex_source, bright_pass_fragment_source) &&
			   blurShader_.load(fullscreen_vertex_source, blur_fragment_source) &&
			   compositeShader_.load(fullscreen_vertex_source, composite_fragment_source);
	}

	void Bloom::shutdown()
	{
		brightShader_.reset();
		blurShader_.reset();
		compositeShader_.reset();
		destroy_bitmap(blurTargetA_);
		destroy_bitmap(blurTargetB_);
		blurTargetA_ = nullptr;
		blurTargetB_ = nullptr;
	}

	bool Bloom::is_valid() const
	{
		return brightShader_.is_valid() && blurShader_.is_valid() && compositeShader_.is_valid();
	}

	const std::string &Bloom::error() const
	{
		if (!brightShader_.is_valid())
			return brightShader_.error();
		if (!blurShader_.is_valid())
			return blurShader_.error();
		return compositeShader_.error();
	}

	void Bloom::set_threshold(float threshold)
	{
		threshold_ = std::clamp(threshold, 0.0f, 1.0f);
	}

	void Bloom::set_intensity(float intensity)
	{
		intensity_ = std::max(intensity, 0.0f);
	}

	void Bloom::set_radius(float radius)
	{
		radius_ = std::max(radius, 0.0f);
	}

	void Bloom::set_downsample(int factor)
	{
		downsample_ = std::max(1, factor);
	}

	bool Bloom::ensure_targets(int width, int height) const
	{
		if (blurTargetA_ && blurTargetB_ && blurTargetA_->width == width && blurTargetA_->height == height)
		{
			return true;
		}
		destroy_bitmap(blurTargetA_);
		destroy_bitmap(blurTargetB_);
		blurTargetA_ = create_render_target(width, height);
		blurTargetB_ = create_render_target(width, height);
		return blurTargetA_ != nullptr && blurTargetB_ != nullptr;
	}

	void Bloom::apply(Bitmap *source, int x, int y, int width, int height, bool flipVertical) const
	{
		if (!source || !is_valid() || !upload_bitmap(source))
		{
			return;
		}

		if (width <= 0)
		{
			width = screen_width();
		}
		if (height <= 0)
		{
			height = screen_height();
		}
		if (width <= 0 || height <= 0)
		{
			return;
		}

		const int smallWidth = std::max(1, width / downsample_);
		const int smallHeight = std::max(1, height / downsample_);
		if (!ensure_targets(smallWidth, smallHeight))
		{
			return;
		}

		float projection[16];

		// pass 1: threshold + downsample source into blurTargetA_
		begin_render_target(blurTargetA_);
		clear_render_target(Colour{0, 0, 0, 0});
		brightShader_.set_uniform("source", 0);
		brightShader_.set_uniform("threshold", threshold_);
		detail::gl2d_ortho_matrix(smallWidth, smallHeight, projection);
		brightShader_.set_uniform_mat4("uProjection", projection);
		submit_fullscreen_quad(0, 0, smallWidth, smallHeight, source->gpu_texture, flipVertical);
		end_render_target();

		// repeat the separable blur several times: a small single pass can only spread a
		// handful of texels, so iterating approximates a much wider Gaussian on the cheap
		Bitmap *blurSource = blurTargetA_;
		Bitmap *blurDestination = blurTargetB_;
		constexpr int blurIterations = 4;
		for (int iteration = 0; iteration < blurIterations; ++iteration)
		{
			// horizontal
			begin_render_target(blurDestination);
			clear_render_target(Colour{0, 0, 0, 0});
			blurShader_.set_uniform("source", 0);
			blurShader_.set_uniform("texel", 1.0f / smallWidth, 1.0f / smallHeight);
			blurShader_.set_uniform("radius", radius_);
			blurShader_.set_uniform("direction", 1.0f, 0.0f);
			detail::gl2d_ortho_matrix(smallWidth, smallHeight, projection);
			blurShader_.set_uniform_mat4("uProjection", projection);
			submit_fullscreen_quad(0, 0, smallWidth, smallHeight, blurSource->gpu_texture, false);
			end_render_target();
			std::swap(blurSource, blurDestination);

			// vertical
			begin_render_target(blurDestination);
			clear_render_target(Colour{0, 0, 0, 0});
			blurShader_.set_uniform("direction", 0.0f, 1.0f);
			submit_fullscreen_quad(0, 0, smallWidth, smallHeight, blurSource->gpu_texture, false);
			end_render_target();
			std::swap(blurSource, blurDestination);
		}

		// pass 4: composite the blurred glow back over the full-resolution source
		compositeShader_.set_uniform("intensity", intensity_);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, blurSource->gpu_texture);
		compositeShader_.set_uniform("bloomTex", 1);
		compositeShader_.set_uniform("source", 0);
		detail::gl2d_ortho_matrix(screen_width(), screen_height(), projection);
		compositeShader_.set_uniform_mat4("uProjection", projection);
		submit_fullscreen_quad(x, y, width, height, source->gpu_texture, flipVertical);
		Shader::stop();
	}

	Vignette::~Vignette()
	{
		shutdown();
	}

	Vignette::Vignette(Vignette &&other) noexcept
		: shader_(std::move(other.shader_)),
		  radius_(other.radius_),
		  softness_(other.softness_),
		  intensity_(other.intensity_)
	{
	}

	Vignette &Vignette::operator=(Vignette &&other) noexcept
	{
		if (this != &other)
		{
			shader_ = std::move(other.shader_);
			radius_ = other.radius_;
			softness_ = other.softness_;
			intensity_ = other.intensity_;
		}
		return *this;
	}

	bool Vignette::initialise()
	{
		if (is_valid())
		{
			return true;
		}
		return shader_.load(fullscreen_vertex_source, vignette_fragment_source);
	}

	void Vignette::shutdown()
	{
		shader_.reset();
	}

	bool Vignette::is_valid() const
	{
		return shader_.is_valid();
	}

	const std::string &Vignette::error() const
	{
		return shader_.error();
	}

	void Vignette::set_radius(float radius)
	{
		radius_ = std::clamp(radius, 0.0f, 1.0f);
	}

	void Vignette::set_softness(float softness)
	{
		softness_ = std::clamp(softness, 0.0001f, 1.0f);
	}

	void Vignette::set_intensity(float intensity)
	{
		intensity_ = std::clamp(intensity, 0.0f, 1.0f);
	}

	void Vignette::apply(Bitmap *source, int x, int y, int width, int height, bool flipVertical) const
	{
		if (!source || !is_valid() || !upload_bitmap(source))
		{
			return;
		}

		if (width <= 0)
		{
			width = screen_width();
		}
		if (height <= 0)
		{
			height = screen_height();
		}
		if (width <= 0 || height <= 0)
		{
			return;
		}

		float projection[16];
		shader_.set_uniform("source", 0);
		shader_.set_uniform("radius", radius_);
		shader_.set_uniform("softness", softness_);
		shader_.set_uniform("intensity", intensity_);
		detail::gl2d_ortho_matrix(screen_width(), screen_height(), projection);
		shader_.set_uniform_mat4("uProjection", projection);
		submit_fullscreen_quad(x, y, width, height, source->gpu_texture, flipVertical);
		Shader::stop();
	}

	LightingPass::~LightingPass()
	{
		shutdown();
	}

	LightingPass::LightingPass(LightingPass &&other) noexcept
		: shader_(std::move(other.shader_)), cullShader_(std::move(other.cullShader_)), ambient_(other.ambient_),
		  shadowMasks_(std::exchange(other.shadowMasks_, {})),
		  lightBuffer_(std::exchange(other.lightBuffer_, 0)),
		  tileCountsBuffer_(std::exchange(other.tileCountsBuffer_, 0)),
		  tileIndicesBuffer_(std::exchange(other.tileIndicesBuffer_, 0)),
		  tileCountX_(other.tileCountX_), tileCountY_(other.tileCountY_)
	{
	}

	LightingPass &LightingPass::operator=(LightingPass &&other) noexcept
	{
		if (this != &other)
		{
			shutdown();
			shader_ = std::move(other.shader_);
			cullShader_ = std::move(other.cullShader_);
			ambient_ = other.ambient_;
			shadowMasks_ = std::exchange(other.shadowMasks_, {});
			lightBuffer_ = std::exchange(other.lightBuffer_, 0);
			tileCountsBuffer_ = std::exchange(other.tileCountsBuffer_, 0);
			tileIndicesBuffer_ = std::exchange(other.tileIndicesBuffer_, 0);
			tileCountX_ = other.tileCountX_;
			tileCountY_ = other.tileCountY_;
		}
		return *this;
	}

	bool LightingPass::initialise()
	{
		if (is_valid())
		{
			return true;
		}
		shadowMasks_.resize(max_shadow_lights, nullptr);
		if (!shader_.load(fullscreen_vertex_source, lighting_fragment_source) ||
			!cullShader_.load_compute(light_cull_compute_source))
		{
			return false;
		}
		GLuint buffers[3] = {};
		glGenBuffers(3, buffers);
		lightBuffer_ = buffers[0];
		tileCountsBuffer_ = buffers[1];
		tileIndicesBuffer_ = buffers[2];
		return lightBuffer_ != 0 && tileCountsBuffer_ != 0 && tileIndicesBuffer_ != 0;
	}

	void LightingPass::shutdown()
	{
		shader_.reset();
		for (Bitmap *&shadowMask : shadowMasks_)
		{
			destroy_bitmap(shadowMask);
			shadowMask = nullptr;
		}
		if (lightBuffer_ != 0)
		{
			const GLuint lightBuffer = static_cast<GLuint>(lightBuffer_);
			glDeleteBuffers(1, &lightBuffer);
			lightBuffer_ = 0;
		}
		if (tileCountsBuffer_ != 0)
		{
			const GLuint buffer = static_cast<GLuint>(tileCountsBuffer_);
			glDeleteBuffers(1, &buffer);
			tileCountsBuffer_ = 0;
		}
		if (tileIndicesBuffer_ != 0)
		{
			const GLuint buffer = static_cast<GLuint>(tileIndicesBuffer_);
			glDeleteBuffers(1, &buffer);
			tileIndicesBuffer_ = 0;
		}
	}

	bool LightingPass::is_valid() const
	{
		return shader_.is_valid() && cullShader_.is_valid() && lightBuffer_ != 0 &&
			tileCountsBuffer_ != 0 && tileIndicesBuffer_ != 0;
	}

	const std::string &LightingPass::error() const
	{
		return shader_.error();
	}

	void LightingPass::set_ambient(float ambient)
	{
		ambient_ = std::clamp(ambient, 0.0f, 1.0f);
	}

	bool LightingPass::ensure_shadow_mask(std::size_t index, int width, int height) const
	{
		if (index >= shadowMasks_.size())
		{
			return false;
		}
		Bitmap *&shadowMask = shadowMasks_[index];
		if (shadowMask && shadowMask->width == width && shadowMask->height == height)
		{
			return true;
		}
		destroy_bitmap(shadowMask);
		shadowMask = create_bitmap(width, height);
		return shadowMask != nullptr;
	}

	void LightingPass::apply(Bitmap *source, const Light &light, int x, int y, int width, int height,
		bool flipVertical, const std::vector<ShadowCaster> &casters) const
	{
		apply(source, std::vector<Light>{light}, x, y, width, height, flipVertical, casters);
	}

	void LightingPass::apply(Bitmap *source, const std::vector<Light> &lights, int x, int y, int width, int height,
		bool flipVertical, const std::vector<ShadowCaster> &casters) const
	{
		if (!source || !is_valid() || !upload_bitmap(source))
		{
			return;
		}

		if (width <= 0)
		{
			width = screen_width();
		}
		if (height <= 0)
		{
			height = screen_height();
		}
		if (width <= 0 || height <= 0)
		{
			return;
		}
		const std::size_t lightCount = lights.size();
		const std::size_t shadowLightCount = std::min<std::size_t>(lightCount, max_shadow_lights);
		for (std::size_t index = 0; index < shadowLightCount; ++index)
		{
			if (!ensure_shadow_mask(index, source->width, source->height))
			{
				return;
			}
			clear_to_colour(shadowMasks_[index], {255, 255, 255});
			for (const ShadowCaster &caster : casters)
			{
				draw_shadow_caster(shadowMasks_[index], caster, lights[index]);
			}
			if (!upload_bitmap(shadowMasks_[index]))
			{
				return;
			}
		}

		std::vector<GpuLight> gpuLights(lightCount);
		for (std::size_t index = 0; index < lightCount; ++index)
		{
			const Light &light = lights[index];
			GpuLight &gpuLight = gpuLights[index];
			gpuLight.positionRadius[0] = light.x;
			gpuLight.positionRadius[1] = light.y;
			gpuLight.positionRadius[2] = std::max(light.radius, 0.0f);
			gpuLight.positionRadius[3] = 0.0f;
			gpuLight.colourIntensity[0] = static_cast<float>(light.colour.red) / 255.0f;
			gpuLight.colourIntensity[1] = static_cast<float>(light.colour.green) / 255.0f;
			gpuLight.colourIntensity[2] = static_cast<float>(light.colour.blue) / 255.0f;
			gpuLight.colourIntensity[3] = std::max(light.intensity, 0.0f);
			gpuLight.shadowSoftness[0] = std::max(light.shadow_softness, 0.0f);
			gpuLight.shadowSoftness[1] = 0.0f;
			gpuLight.shadowSoftness[2] = 0.0f;
			gpuLight.shadowSoftness[3] = 0.0f;
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(lightBuffer_));
		glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(gpuLights.size() * sizeof(GpuLight)),
			gpuLights.data(), GL_STREAM_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, static_cast<GLuint>(lightBuffer_));

		tileCountX_ = (source->width + tile_size - 1) / tile_size;
		tileCountY_ = (source->height + tile_size - 1) / tile_size;
		const std::size_t tileCount = static_cast<std::size_t>(tileCountX_) * tileCountY_;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(tileCountsBuffer_));
		glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(tileCount * sizeof(std::uint32_t)),
			nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, static_cast<GLuint>(tileCountsBuffer_));
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(tileIndicesBuffer_));
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			static_cast<GLsizeiptr>(tileCount * max_lights_per_tile * sizeof(std::uint32_t)),
			nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, static_cast<GLuint>(tileIndicesBuffer_));
		cullShader_.set_uniform("screenSize", source->width, source->height);
		cullShader_.set_uniform("tileCount", tileCountX_, tileCountY_);
		cullShader_.set_uniform("lightCount", static_cast<int>(lightCount));
		cullShader_.dispatch_compute(static_cast<unsigned int>(tileCountX_), static_cast<unsigned int>(tileCountY_), 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		float projection[16];
		shader_.set_uniform("source", 0);
		shader_.set_uniform("lightCount", static_cast<int>(lightCount));
		shader_.set_uniform("shadowLightCount", static_cast<int>(shadowLightCount));
		shader_.set_uniform("tileCount", tileCountX_, tileCountY_);
		for (std::size_t index = 0; index < shadowLightCount; ++index)
		{
			const std::string suffix = "[" + std::to_string(index) + "]";
			glActiveTexture(GL_TEXTURE1 + static_cast<GLenum>(index));
			glBindTexture(GL_TEXTURE_2D, shadowMasks_[index]->gpu_texture);
			shader_.set_uniform(("shadowMasks" + suffix).c_str(), static_cast<int>(index + 1));
		}
		shader_.set_uniform("ambient", ambient_);
		shader_.set_uniform("flipVertical", flipVertical ? 1 : 0);
		detail::gl2d_ortho_matrix(screen_width(), screen_height(), projection);
		shader_.set_uniform_mat4("uProjection", projection);
		submit_fullscreen_quad(x, y, width, height, source->gpu_texture, flipVertical);
		Shader::stop();
	}

	void ScreenFade::set_colour(Colour colour)
	{
		colour_ = colour;
	}

	Colour ScreenFade::colour() const
	{
		return colour_;
	}

	void ScreenFade::apply() const
	{
		if (colour_.alpha == 0)
		{
			return;
		}
		rectfill(screen, 0.0f, 0.0f,
				 static_cast<float>(screen_width()), static_cast<float>(screen_height()), colour_);
	}

} // namespace sl
