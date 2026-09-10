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

    /** Owns and applies an OpenGL shader program. */
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
        /** Compile and link a compute shader, replacing the current program. */
        bool load_compute(const std::string &computeSource);
        /** Delete the program and clear its error state. */
        void reset();

        /** Return whether a linked program is available. */
        bool is_valid() const;
        /** Return the most recent shader compilation or linking error. */
        const std::string &error() const;

        /** Bind this shader program for subsequent OpenGL calls. */
        bool use() const;
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
         * back over the original at full resolution. Set flipVertical for
         * render-target sources (see create_render_target()).
         */
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0, bool flipVertical = false) const;

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
         * Set flipVertical for render-target sources (see create_render_target()).
         */
        void apply(Bitmap *source, int x = 0, int y = 0, int width = 0, int height = 0, bool flipVertical = false) const;

    private:
        Shader shader_;
        float radius_ = 0.75f;
        float softness_ = 0.45f;
        float intensity_ = 0.8f;
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
                   int width = 0, int height = 0, bool flipVertical = false,
                   const std::vector<ShadowCaster> &casters = {}) const;
        /** Apply an arbitrary batch of lights; only the first max_shadow_lights receive shadows. */
        void apply(Bitmap *source, const std::vector<Light> &lights, int x = 0, int y = 0,
               int width = 0, int height = 0, bool flipVertical = false,
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
