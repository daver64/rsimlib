/** @file
 * @brief D3D12 Renderer boilerplate; only ever compiled on Windows.
 */
#ifdef _WIN32

#include "d3d12_renderer.h"
#include "d3d12_context.h"

namespace sl::detail
{
    namespace
    {
        class D3D12Renderer final : public Renderer
        {
        public:
            void configure_window() override {}
            std::uint32_t window_flags() const override { return SDL_WINDOW_RESIZABLE; }

            bool initialise(SDL_Window *window, std::string &error) override
            {
                window_ = window;
                return context_.initialise(window, error);
            }
            void shutdown() override
            {
                context_.shutdown();
                window_ = nullptr;
            }
            bool resize(int width, int height, std::string &error) override
            {
                return context_.resize(width, height, error);
            }
            bool set_vsync(bool enabled) override
            {
                vsync_enabled_ = enabled;
                return true;
            }
            void present() override { context_.present(vsync_enabled_); }
            bool begin_frame(std::string &) override
            {
                return context_.begin_frame();
            }
            bool end_frame(std::string &) override
            {
                context_.present(vsync_enabled_);
                return true;
            }
            SDL_GLContext native_context() const override { return nullptr; }

            bool create_texture(const TextureDesc &, std::uint32_t &) override { return unsupported(); }
            bool upload_texture(std::uint32_t, int, int, const std::uint8_t *) override { return unsupported(); }
            void destroy_texture(std::uint32_t) override {}
            bool create_render_target(int, int, std::uint32_t &, std::uint32_t &) override { return unsupported(); }
            void destroy_render_target(std::uint32_t, std::uint32_t) override {}
            bool begin_render_target(std::uint32_t, int, int, std::string &) override { return unsupported(); }
            bool end_render_target(std::string &) override { return unsupported(); }
            bool create_shader(const ShaderSource &, const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "D3D12 shader compilation is not implemented yet.";
                return false;
            }
            bool create_compute_shader(const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "D3D12 compute shaders are not implemented yet.";
                return false;
            }
            bool create_shader_module(ShaderStage, const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "D3D12 shader modules are not implemented yet.";
                return false;
            }
            void destroy_shader(std::uint32_t) override {}
            bool use_shader(std::uint32_t) override { return unsupported(); }
            void stop_shader() override {}
            bool dispatch_compute(std::uint32_t, unsigned int, unsigned int, unsigned int) override { return unsupported(); }
            bool set_shader_int(std::uint32_t, const char *, int) override { return unsupported(); }
            bool set_shader_float(std::uint32_t, const char *, float) override { return unsupported(); }
            bool set_shader_float2(std::uint32_t, const char *, float, float) override { return unsupported(); }
            bool set_shader_int2(std::uint32_t, const char *, int, int) override { return unsupported(); }
            bool set_shader_float3(std::uint32_t, const char *, float, float, float) override { return unsupported(); }
            bool set_shader_mat4(std::uint32_t, const char *, const float *) override { return unsupported(); }
            bool initialise_2d() override { return false; }
            void shutdown_2d() override {}
            bool begin_2d(int, int) override { return unsupported(); }
            bool clear_frame(float red, float green, float blue, float alpha) override
            {
                context_.clear(red, green, blue, alpha);
                return true;
            }
            void submit_2d(PrimitiveType, const Vertex2D *, int, std::uint32_t) override {}
            bool create_storage_buffer(std::size_t, std::uint32_t &) override { return unsupported(); }
            void destroy_storage_buffer(std::uint32_t) override {}
            bool upload_storage_buffer(std::uint32_t, std::size_t, const void *, bool) override { return unsupported(); }
            void bind_storage_buffer(unsigned int, std::uint32_t) override {}
            void bind_texture_unit(unsigned int, std::uint32_t) override {}
            void storage_barrier() override {}

        private:
            bool unsupported()
            {
                last_error_ = "D3D12 renderer resource is not implemented yet.";
                return false;
            }

            SDL_Window *window_ = nullptr;
            D3D12Context context_;
            std::string last_error_;
            bool vsync_enabled_ = true;
        };
    }

    std::unique_ptr<Renderer> create_d3d12_renderer()
    {
        return std::make_unique<D3D12Renderer>();
    }
}

#endif // _WIN32
