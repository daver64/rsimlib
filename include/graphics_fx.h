#pragma once

#include "draw.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sl
{
    struct Bitmap;
}

namespace sl
{

    /**
     * Describes one named field inside a custom Vulkan shader's fragment
     * push-constant block, relative to the block's own start (0-based).
     * Sizes: float/int = 4, vec2 = 8, vec3 = 12, vec4/mat4's row = varies;
     * use 64 for a full mat4.
     */
    struct ShaderUniform
    {
        std::string name;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
    };

    /** Owns and applies a shader program for the active graphics backend. */
    class Shader
    {
    public:
        Shader() = default;
        Shader(const std::string &vertexSource, const std::string &fragmentSource);
        ~Shader();

        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;
        Shader(Shader &&other) noexcept;
        Shader &operator=(Shader &&other) noexcept;

        /** Compile and link shader source, replacing the current program. */
        bool load(const std::string &vertexSource, const std::string &fragmentSource);
        /** Compile and link a named built-in shader asset, replacing the current program. */
        bool load(const std::string &vertexSource, const std::string &fragmentSource, const std::string &assetId);
        /**
         * Compile and link an arbitrary custom shader, replacing the current program.
         * On Vulkan this compiles the supplied GLSL to SPIR-V at runtime (requires
         * glslangValidator to be installed) and builds a pipeline whose fragment
         * stage samples `vulkanSamplerNames` (in binding order 0..N-1) and exposes
         * `vulkanUniforms` as named push-constant fields settable via set_uniform.
         * The special uniform name "uProjection" is always available for a 4x4
         * matrix and does not need to be listed in `vulkanUniforms`. On OpenGL,
         * `vulkanSamplerNames` and `vulkanUniforms` are ignored.
         */
        bool load(const std::string &vertexSource, const std::string &fragmentSource,
                  const std::vector<std::string> &vulkanSamplerNames,
                  const std::vector<ShaderUniform> &vulkanUniforms);
        /** Load vertex and fragment GLSL source files and compile them. */
        bool load_files(const std::string &vertexPath, const std::string &fragmentPath,
                const std::vector<std::string> &vulkanSamplerNames = {},
                const std::vector<ShaderUniform> &vulkanUniforms = {},
                const std::string &assetId = {});
        /** Compile and link a compute shader, replacing the current program. */
        bool load_compute(const std::string &computeSource);
        /** Compile and link a named built-in compute shader asset, replacing the current program. */
        bool load_compute(const std::string &computeSource, const std::string &assetId);
        /** Load and compile a compute GLSL source file. */
        bool load_compute_file(const std::string &computePath, const std::string &assetId = {});
        /** Delete the program and clear its error state. */
        void reset();

        /** Return whether a linked program is available. */
        bool is_valid() const;
        /** Return the most recent shader compilation or linking error. */
        const std::string &error() const;

        /** Bind this shader program for subsequent OpenGL calls. */
        bool use() const;
        /** Draw a textured quad with this shader using the active renderer. */
        bool draw_textured_quad(Bitmap *texture, float x, float y, float width, float height) const;
        /** Unbind the current shader program. */
        static void stop();
        /** Dispatch this program as a compute shader. */
        bool dispatch_compute(unsigned int groupsX, unsigned int groupsY, unsigned int groupsZ) const;
        /** Set an integer shader uniform. */
        bool set_uniform(const char *name, int value) const;
        /** Set a floating-point shader uniform. */
        bool set_uniform(const char *name, float value) const;
        /** Set a two-component floating-point shader uniform. */
        bool set_uniform(const char *name, float x, float y) const;
        /** Set a two-component integer shader uniform. */
        bool set_uniform(const char *name, int x, int y) const;
        /** Set a three-component floating-point shader uniform. */
        bool set_uniform(const char *name, float x, float y, float z) const;
        /** Set a 4x4 matrix shader uniform (column-major). */
        bool set_uniform_mat4(const char *name, const float *matrix4x4) const;

    private:
        std::uint32_t program_ = 0;
        std::string error_;
    };

    /** Internal helper for chaining fullscreen post-process passes without managing one bitmap per effect. */
    class PingPongBuffer
    {
    public:
        PingPongBuffer() = default;
        ~PingPongBuffer();

        PingPongBuffer(const PingPongBuffer &) = delete;
        PingPongBuffer &operator=(const PingPongBuffer &) = delete;

        void initialise(int width, int height);
        void shutdown();
        bool valid() const;

        Bitmap *begin(Bitmap *source);
        Bitmap *source() const;
        Bitmap *target() const;
        Bitmap *advance();

    private:
        Bitmap *buffers_[2] = {nullptr, nullptr};
        Bitmap *source_ = nullptr;
        Bitmap *current_target_ = nullptr;
        int width_ = 0;
        int height_ = 0;
        int next_index_ = 0;
    };

    class Bloom
    {
    public:
        Bloom() = default;
        ~Bloom();

        Bloom(const Bloom &) = delete;
        Bloom &operator=(const Bloom &) = delete;
        Bloom(Bloom &&other) noexcept;
        Bloom &operator=(Bloom &&other) noexcept;

        /** Compile the bloom shaders and prepare the effect. */
        bool initialise();
        /** Release resources owned by the effect. */
        void shutdown();
        /** Return whether the effect is ready to apply. */
        bool is_valid() const;
        /** Return the most recent initialization error. */
        const std::string &error() const;

        /** Set the luminance threshold that contributes to bloom. */
        void set_threshold(float threshold);
        /** Set the bloom brightness multiplier. */
        void set_intensity(float intensity);
        /** Set the blur sample spacing, in downsampled-buffer texels. */
        void set_radius(float radius);
        /**
         * Set how many times smaller the internal blur buffers are than the source
         * (e.g. 4 blurs at quarter resolution). Bigger values give a wider, softer,
         * cheaper glow; smaller values look tighter and costlier. Takes effect on
         * the next apply() call.
         */
        void set_downsample(int factor);

        /**
         * Apply a wide-area glow to a bitmap region: thresholds bright pixels,
         * downsamples, blurs with a separable Gaussian, then composites the result
         * back over the original at full resolution. Texture orientation is
         * resolved by the active backend.
         */
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader brightShader_;
        Shader blurShader_;
        Shader compositeShader_;
        float threshold_ = 0.7f;
        float intensity_ = 0.9f;
        float radius_ = 1.5f;
        int downsample_ = 2;
        mutable Bitmap *blurTargetA_ = nullptr;
        mutable Bitmap *blurTargetB_ = nullptr;

        bool ensure_targets(int width, int height) const;
    };

    /** Darkens the screen edges around a soft-radius circle to draw focus toward the centre. */
    class Vignette
    {
    public:
        Vignette() = default;
        ~Vignette();

        Vignette(const Vignette &) = delete;
        Vignette &operator=(const Vignette &) = delete;
        Vignette(Vignette &&other) noexcept;
        Vignette &operator=(Vignette &&other) noexcept;

        /** Compile the vignette shader and prepare the effect. */
        bool initialise();
        /** Release resources owned by the effect. */
        void shutdown();
        /** Return whether the effect is ready to apply. */
        bool is_valid() const;
        /** Return the most recent initialization error. */
        const std::string &error() const;

        /** Set where the darkening starts, as a fraction of the screen's half-diagonal (0-1). */
        void set_radius(float radius);
        /** Set how wide the fade from clear to fully darkened is (0-1). */
        void set_softness(float softness);
        /** Set how dark the vignette gets at its outer edge (0 = no effect, 1 = black). */
        void set_intensity(float intensity);

        /**
         * Darken a bitmap region's edges and draw the result to the current render target.
         * Texture orientation is resolved by the active backend.
         */
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float radius_ = 0.75f;
        float softness_ = 0.45f;
        float intensity_ = 0.8f;
    };

    /** Applies brightness, contrast, saturation, and exposure to a bitmap. */
    class ColourAdjust
    {
    public:
        ColourAdjust() = default;
        ~ColourAdjust() = default;

        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_brightness(float value);
        void set_contrast(float value);
        void set_saturation(float value);
        void set_exposure(float value);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float brightness_ = 0.0f;
        float contrast_ = 1.0f;
        float saturation_ = 1.0f;
        float exposure_ = 0.0f;
    };

    /** Applies a separable Gaussian blur to a bitmap using two temporary targets. */
    class Blur
    {
    public:
        Blur() = default;
        ~Blur();
        Blur(const Blur &) = delete;
        Blur &operator=(const Blur &) = delete;
        Blur(Blur &&other) noexcept;
        Blur &operator=(Blur &&other) noexcept;

        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_radius(float radius);
        void set_iterations(int iterations);
        void set_opacity(float opacity);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float radius_ = 1.5f;
        int iterations_ = 2;
        float opacity_ = 1.0f;
        mutable Bitmap *target_a_ = nullptr;
        mutable Bitmap *target_b_ = nullptr;

        bool ensure_targets(int width, int height) const;
    };

    /** Separates RGB samples toward the screen edges for a lens/damage effect. */
    class ChromaticAberration
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_strength(float strength);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float strength_ = 0.004f;
    };

    /** Reduces a bitmap to configurable screen-space colour blocks. */
    class Pixelate
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_pixel_size(float size);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float pixel_size_ = 8.0f;
    };

    /** Blurs pixels along rays from a configurable focal point. */
    class RadialBlur
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_centre(float x, float y);
        void set_strength(float strength);
        void set_samples(int samples);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float centre_x_ = 0.5f;
        float centre_y_ = 0.5f;
        float strength_ = 0.25f;
        int samples_ = 8;
    };

    /** Applies animated sinusoidal UV distortion for heat, water, or portal effects. */
    class HeatHaze
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_strength(float strength);
        void set_frequency(float frequency);
        void set_time(float time);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float strength_ = 0.008f;
        float frequency_ = 24.0f;
        float time_ = 0.0f;
    };

    /** Pushes pixels outward along an expanding ring centered on a focal point. */
    class Shockwave
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        /** Set the normalized focal point; (0, 0) is top-left and (1, 1) is bottom-right. */
        void set_centre(float x, float y);
        /** Set the normalized distance of the expanding ring from its focal point. */
        void set_radius(float radius);
        /** Set the normalized thickness of the distortion ring. */
        void set_width(float width);
        /** Set the normalized outward displacement at the ring peak. */
        void set_strength(float strength);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float centre_x_ = 0.5f;
        float centre_y_ = 0.5f;
        float radius_ = 0.25f;
        float width_ = 0.08f;
        float strength_ = 0.025f;
    };

    /** A retro CRT composite combining scanlines, light curvature, and blocky pixelation. */
    class CRTFilter
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_pixel_size(float size);
        void set_scanline_strength(float strength);
        void set_curvature(float curvature);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float pixel_size_ = 4.0f;
        float scanline_strength_ = 0.35f;
        float curvature_ = 0.18f;
    };

    /** Applies ordered (Bayer) dithering to reduce colour depth for a retro/print look. */
    class DitherFilter
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        void set_pixel_size(float size);
        void set_levels(float levels);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float pixel_size_ = 1.0f;
        float levels_ = 4.0f;
    };

    /** Adds animated monochrome film grain over a bitmap. */
    class FilmGrain
    {
    public:
        bool initialise();
        void shutdown();
        bool is_valid() const;
        const std::string &error() const;
        /** Set the grain intensity from 0 (off) upward. */
        void set_strength(float strength);
        /** Set the animation time in seconds. */
        void set_time(float time);
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0) const;

    private:
        Shader shader_;
        float strength_ = 0.08f;
        float time_ = 0.0f;
    };

    /** Applies a decaying camera offset to 2D projections for impact and motion effects. */
    class ScreenShake
    {
    public:
        ScreenShake() = default;
        ~ScreenShake();
        ScreenShake(const ScreenShake &) = delete;
        ScreenShake &operator=(const ScreenShake &) = delete;

        void trigger(float amplitude, float duration);
        void update(float delta_seconds);
        void clear();
        bool active() const;

    private:
        float amplitude_ = 0.0f;
        float duration_ = 0.0f;
        float remaining_ = 0.0f;
        float offset_x_ = 0.0f;
        float offset_y_ = 0.0f;
    };

    /** A radial 2D light expressed in screen pixels. */
    struct Light
    {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 256.0f;
        float intensity = 1.0f;
        float shadow_softness = 0.0f;
        Colour colour{255, 255, 255};
    };

    /** A point in a 2D shadow-casting polygon. */
    struct ShadowPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    /** A polygon whose edges can block a radial light. Vertices should be ordered around its perimeter. */
    struct ShadowCaster
    {
        std::vector<ShadowPoint> vertices;
    };

    /** Create an axis-aligned rectangular shadow caster. */
    ShadowCaster make_rectangle_shadow_caster(float left, float top, float right, float bottom);

    /** Modulates a rendered scene with an arbitrary number of radial lights. */
    class LightingPass
    {
    public:
        static constexpr std::size_t max_shadow_lights = 8;
        static constexpr int tile_size = 16;
        static constexpr int max_lights_per_tile = 128;

        LightingPass() = default;
        ~LightingPass();

        LightingPass(const LightingPass &) = delete;
        LightingPass &operator=(const LightingPass &) = delete;
        LightingPass(LightingPass &&other) noexcept;
        LightingPass &operator=(LightingPass &&other) noexcept;

        /** Compile the lighting shader and prepare the effect. */
        bool initialise();
        /** Release resources owned by the effect. */
        void shutdown();
        /** Return whether the effect is ready to apply. */
        bool is_valid() const;
        /** Return the most recent initialization error. */
        const std::string &error() const;

        /** Set the minimum scene illumination, from 0 (black) to 1 (full brightness). */
        void set_ambient(float ambient);

        /** Apply ambient plus radial lighting and polygon shadows to a bitmap. */
        void apply(Bitmap *source, const Light &light, int x = 0, int y = 0,
                   int width = 0, int height = 0,
                   const std::vector<ShadowCaster> &casters = {}) const;
        /** Apply an arbitrary batch of lights; only the first max_shadow_lights receive shadows. */
        void apply(Bitmap *source, const std::vector<Light> &lights, int x = 0, int y = 0,
               int width = 0, int height = 0,
               const std::vector<ShadowCaster> &casters = {}) const;

    private:
        Shader shader_;
        Shader cullShader_;
        float ambient_ = 0.2f;
        mutable std::vector<Bitmap *> shadowMasks_;
        std::uint32_t lightBuffer_ = 0;
        mutable std::uint32_t tileCountsBuffer_ = 0;
        mutable std::uint32_t tileIndicesBuffer_ = 0;
        mutable int tileCountX_ = 0;
        mutable int tileCountY_ = 0;

        bool ensure_shadow_mask(std::size_t index, int width, int height) const;
    };

    /** Draws a solid-colour overlay over the whole screen, useful for fades and hit-flash feedback. */
    class ScreenFade
    {
    public:
        /** Set the overlay colour, including its alpha (0 = invisible, 255 = fully opaque). */
        void set_colour(Colour colour);
        /** Return the current overlay colour. */
        Colour colour() const;

        /** Draw the overlay colour over the entire screen. No-op when the colour's alpha is zero. */
        void apply() const;

    private:
        Colour colour_{0, 0, 0, 0};
    };

} // namespace sl
