#define GL_GLEXT_PROTOTYPES

#include "graphics_fx.h"

#include "display.h"
#include "draw.h"

#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace simlib::graphics_fx {
namespace {

constexpr const char* fullscreen_vertex_source = R"(
#version 120
varying vec2 uv;

void main() {
	gl_Position = ftransform();
	uv = gl_MultiTexCoord0.xy;
}
)";

constexpr const char* bloom_fragment_source = R"(
#version 120
uniform sampler2D source;
uniform vec2 texel;
uniform float threshold;
uniform float intensity;
uniform float radius;
varying vec2 uv;

vec3 bright(vec3 colour) {
	return max(colour - vec3(threshold), vec3(0.0));
}

void main() {
	vec4 base = texture2D(source, uv);
	vec3 bloom = vec3(0.0);
	bloom += bright(texture2D(source, uv + texel * vec2(-radius, -radius)).rgb);
	bloom += bright(texture2D(source, uv + texel * vec2(0.0, -radius)).rgb);
	bloom += bright(texture2D(source, uv + texel * vec2(radius, -radius)).rgb);
	bloom += bright(texture2D(source, uv + texel * vec2(-radius, 0.0)).rgb);
	bloom += bright(base.rgb);
	bloom += bright(texture2D(source, uv + texel * vec2(radius, 0.0)).rgb);
	bloom += bright(texture2D(source, uv + texel * vec2(-radius, radius)).rgb);
	bloom += bright(texture2D(source, uv + texel * vec2(0.0, radius)).rgb);
	bloom += bright(texture2D(source, uv + texel * vec2(radius, radius)).rgb);
	gl_FragColor = vec4(base.rgb + bloom * (intensity / 9.0), base.a);
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

void set_screen_projection() {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(
		0.0,
		static_cast<double>(display::screen_width()),
		static_cast<double>(display::screen_height()),
		0.0,
		-1.0,
		1.0
	);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}

void restore_projection() {
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
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

bool Bloom::initialise() {
	return shader_.is_valid() || shader_.load(fullscreen_vertex_source, bloom_fragment_source);
}

void Bloom::shutdown() {
	shader_.reset();
}

bool Bloom::is_valid() const {
	return shader_.is_valid();
}

const std::string& Bloom::error() const {
	return shader_.error();
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

void Bloom::apply(draw::Bitmap* source, int x, int y, int width, int height) const {
	if (!source || !shader_.is_valid() || !draw::upload_bitmap(source)) {
		return;
	}

	if (width <= 0) {
		width = display::screen_width();
	}
	if (height <= 0) {
		height = display::screen_height();
	}
	if (width <= 0 || height <= 0) {
		return;
	}

	shader_.use();
	shader_.set_uniform("source", 0);
	shader_.set_uniform("texel", 1.0f / source->width, 1.0f / source->height);
	shader_.set_uniform("threshold", threshold_);
	shader_.set_uniform("intensity", intensity_);
	shader_.set_uniform("radius", radius_);

	set_screen_projection();
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, source->gpu_texture);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
	glTexCoord2f(1.0f, 0.0f); glVertex2i(x + width, y);
	glTexCoord2f(1.0f, 1.0f); glVertex2i(x + width, y + height);
	glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + height);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	restore_projection();
	Shader::stop();
}

} // namespace simlib::graphics_fx
