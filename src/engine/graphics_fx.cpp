#define GL_GLEXT_PROTOTYPES

#include "graphics_fx.h"

#include "display.h"
#include "draw.h"
#include "gl2d.h"

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace simlib {
namespace {

constexpr const char* fullscreen_vertex_source = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 uProjection;

out vec2 uv;

void main() {
	gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
	uv = aTexCoord;
}
)";

constexpr const char* bright_pass_fragment_source = R"(
#version 330 core
uniform sampler2D source;
uniform float threshold;
in vec2 uv;
out vec4 fragColor;

void main() {
	vec3 colour = texture(source, uv).rgb;
	fragColor = vec4(max(colour - vec3(threshold), vec3(0.0)), 1.0);
}
)";

constexpr const char* blur_fragment_source = R"(
#version 330 core
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

constexpr const char* composite_fragment_source = R"(
#version 330 core
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

std::string shader_log(GLuint shader) {
	GLint length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
	if (length <= 1) {
		return "Shader compilation failed.";
	}
	std::vector<GLchar> log(static_cast<std::size_t>(length));
	glGetShaderInfoLog(shader, length, nullptr, log.data());
	return log.data();
}

std::string program_log(GLuint program) {
	GLint length = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
	if (length <= 1) {
		return "Shader linking failed.";
	}
	std::vector<GLchar> log(static_cast<std::size_t>(length));
	glGetProgramInfoLog(program, length, nullptr, log.data());
	return log.data();
}

GLuint compile_shader(GLenum type, const std::string& source, std::string& error) {
	const GLuint shader = glCreateShader(type);
	if (shader == 0) {
		error = "Unable to create shader.";
		return 0;
	}

	const char* shaderSource = source.c_str();
	glShaderSource(shader, 1, &shaderSource, nullptr);
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_TRUE) {
		return shader;
	}

	error = shader_log(shader);
	glDeleteShader(shader);
	return 0;
}

/** Draw a textured quad at (x, y, width, height), used by every bloom pass. */
void submit_fullscreen_quad(int x, int y, int width, int height, GLuint texture, bool flipVertical) {
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

} // namespace

Shader::Shader(const std::string& vertexSource, const std::string& fragmentSource) {
	load(vertexSource, fragmentSource);
}

Shader::~Shader() {
	reset();
}

Shader::Shader(Shader&& other) noexcept
	: program_(std::exchange(other.program_, 0)), error_(std::move(other.error_)) {
}

Shader& Shader::operator=(Shader&& other) noexcept {
	if (this != &other) {
		reset();
		program_ = std::exchange(other.program_, 0);
		error_ = std::move(other.error_);
	}
	return *this;
}

bool Shader::load(const std::string& vertexSource, const std::string& fragmentSource) {
	reset();

	const GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vertexSource, error_);
	if (vertexShader == 0) {
		return false;
	}

	const GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentSource, error_);
	if (fragmentShader == 0) {
		glDeleteShader(vertexShader);
		return false;
	}

	const GLuint program = glCreateProgram();
	if (program == 0) {
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
	if (linked != GL_TRUE) {
		error_ = program_log(program);
		glDeleteProgram(program);
		return false;
	}

	program_ = program;
	error_.clear();
	return true;
}

void Shader::reset() {
	if (program_ != 0) {
		glDeleteProgram(static_cast<GLuint>(program_));
		program_ = 0;
	}
	error_.clear();
}

bool Shader::is_valid() const {
	return program_ != 0;
}

const std::string& Shader::error() const {
	return error_;
}

bool Shader::use() const {
	if (!is_valid()) {
		return false;
	}
	glUseProgram(static_cast<GLuint>(program_));
	return true;
}

void Shader::stop() {
	glUseProgram(0);
}

bool Shader::set_uniform(const char* name, int value) const {
	if (!use()) {
		return false;
	}
	const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
	if (location < 0) {
		return false;
	}
	glUniform1i(location, value);
	return true;
}

bool Shader::set_uniform(const char* name, float value) const {
	if (!use()) {
		return false;
	}
	const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
	if (location < 0) {
		return false;
	}
	glUniform1f(location, value);
	return true;
}

bool Shader::set_uniform(const char* name, float x, float y) const {
	if (!use()) {
		return false;
	}
	const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
	if (location < 0) {
		return false;
	}
	glUniform2f(location, x, y);
	return true;
}

bool Shader::set_uniform_mat4(const char* name, const float* matrix4x4) const {
	if (!use() || !matrix4x4) {
		return false;
	}
	const GLint location = glGetUniformLocation(static_cast<GLuint>(program_), name);
	if (location < 0) {
		return false;
	}
	glUniformMatrix4fv(location, 1, GL_FALSE, matrix4x4);
	return true;
}

Bloom::~Bloom() {
	shutdown();
}

Bloom::Bloom(Bloom&& other) noexcept
	: brightShader_(std::move(other.brightShader_)),
	  blurShader_(std::move(other.blurShader_)),
	  compositeShader_(std::move(other.compositeShader_)),
	  threshold_(other.threshold_),
	  intensity_(other.intensity_),
	  radius_(other.radius_),
	  downsample_(other.downsample_),
	  blurTargetA_(std::exchange(other.blurTargetA_, nullptr)),
	  blurTargetB_(std::exchange(other.blurTargetB_, nullptr)) {
}

Bloom& Bloom::operator=(Bloom&& other) noexcept {
	if (this != &other) {
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

bool Bloom::initialise() {
	if (is_valid()) {
		return true;
	}
	return brightShader_.load(fullscreen_vertex_source, bright_pass_fragment_source) &&
		blurShader_.load(fullscreen_vertex_source, blur_fragment_source) &&
		compositeShader_.load(fullscreen_vertex_source, composite_fragment_source);
}

void Bloom::shutdown() {
	brightShader_.reset();
	blurShader_.reset();
	compositeShader_.reset();
	destroy_bitmap(blurTargetA_);
	destroy_bitmap(blurTargetB_);
	blurTargetA_ = nullptr;
	blurTargetB_ = nullptr;
}

bool Bloom::is_valid() const {
	return brightShader_.is_valid() && blurShader_.is_valid() && compositeShader_.is_valid();
}

const std::string& Bloom::error() const {
	if (!brightShader_.is_valid()) return brightShader_.error();
	if (!blurShader_.is_valid()) return blurShader_.error();
	return compositeShader_.error();
}

void Bloom::set_threshold(float threshold) {
	threshold_ = std::clamp(threshold, 0.0f, 1.0f);
}

void Bloom::set_intensity(float intensity) {
	intensity_ = std::max(intensity, 0.0f);
}

void Bloom::set_radius(float radius) {
	radius_ = std::max(radius, 0.0f);
}

void Bloom::set_downsample(int factor) {
	downsample_ = std::max(1, factor);
}

bool Bloom::ensure_targets(int width, int height) const {
	if (blurTargetA_ && blurTargetB_ && blurTargetA_->width == width && blurTargetA_->height == height) {
		return true;
	}
	destroy_bitmap(blurTargetA_);
	destroy_bitmap(blurTargetB_);
	blurTargetA_ = create_render_target(width, height);
	blurTargetB_ = create_render_target(width, height);
	return blurTargetA_ != nullptr && blurTargetB_ != nullptr;
}

void Bloom::apply(Bitmap* source, int x, int y, int width, int height, bool flipVertical) const {
	if (!source || !is_valid() || !upload_bitmap(source)) {
		return;
	}

	if (width <= 0) {
		width = screen_width();
	}
	if (height <= 0) {
		height = screen_height();
	}
	if (width <= 0 || height <= 0) {
		return;
	}

	const int smallWidth = std::max(1, width / downsample_);
	const int smallHeight = std::max(1, height / downsample_);
	if (!ensure_targets(smallWidth, smallHeight)) {
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
	Bitmap* blurSource = blurTargetA_;
	Bitmap* blurDestination = blurTargetB_;
	constexpr int blurIterations = 4;
	for (int iteration = 0; iteration < blurIterations; ++iteration) {
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

} // namespace simlib
