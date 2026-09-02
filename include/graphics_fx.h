#pragma once

#include <cstdint>
#include <string>

namespace rvoid::draw {
struct Texture;
}

namespace rvoid::graphics_fx {

/** Owns and applies an OpenGL shader program. */
class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertexSource, const std::string& fragmentSource);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /** Compile and link shader source, replacing the current program. */
    bool load(const std::string& vertexSource, const std::string& fragmentSource);
    /** Delete the program and clear its error state. */
    void reset();

    /** Return whether a linked program is available. */
    bool is_valid() const;
    /** Return the most recent shader compilation or linking error. */
    const std::string& error() const;

    /** Bind this shader program for subsequent OpenGL calls. */
    bool use() const;
    /** Unbind the current shader program. */
    static void stop();
    /** Set an integer shader uniform. */
    bool set_uniform(const char* name, int value) const;
    /** Set a floating-point shader uniform. */
    bool set_uniform(const char* name, float value) const;
    /** Set a two-component floating-point shader uniform. */
    bool set_uniform(const char* name, float x, float y) const;

private:
    std::uint32_t program_ = 0;
    std::string error_;
};

class Bloom {
public:
    /** Compile the bloom shader and prepare the effect. */
    bool initialise();
    /** Release resources owned by the effect. */
    void shutdown();
    /** Return whether the effect is ready to apply. */
    bool is_valid() const;
    /** Return the most recent initialization error. */
    const std::string& error() const;

    void set_threshold(float threshold);
    void set_intensity(float intensity);
    void set_radius(float radius);
    void apply(draw::Texture* source, int x = 0, int y = 0, int width = 0, int height = 0) const;

private:
    Shader shader_;
    float threshold_ = 0.7f;
    float intensity_ = 0.9f;
    float radius_ = 1.5f;
};

} // namespace rvoid::graphics_fx
