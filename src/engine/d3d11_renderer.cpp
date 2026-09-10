/** @file
 * @brief D3D11 Renderer boilerplate; only ever compiled on Windows.
 *
 * Establishes the device/swap chain and implements the resource-free 2D path.
 */
#ifdef _WIN32

#include "d3d11_renderer.h"
#include "d3d11_context.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace sl::detail
{
    namespace
    {
        struct Texture
        {
            ID3D11Texture2D *resource = nullptr;
            ID3D11ShaderResourceView *view = nullptr;
            ID3D11SamplerState *sampler = nullptr;
        };

        struct RenderTarget
        {
            ID3D11RenderTargetView *view = nullptr;
            std::uint32_t texture = 0;
        };

        void release_texture(Texture &texture)
        {
            if (texture.sampler) texture.sampler->Release();
            if (texture.view) texture.view->Release();
            if (texture.resource) texture.resource->Release();
            texture = {};
        }

        std::string load_shader(const char *name)
        {
            std::ifstream file(std::filesystem::path(SIMLIB_D3D_SHADER_DIR) / name);
            return file ? std::string(std::istreambuf_iterator<char>(file), {}) : std::string{};
        }

        bool compile_shader(const std::string &source, const char *entry, const char *target,
                            ID3DBlob **blob, std::string &error)
        {
            ID3DBlob *messages = nullptr;
            const HRESULT result = D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr,
                entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, blob, &messages);
            if (FAILED(result))
            {
                error = messages ? static_cast<const char *>(messages->GetBufferPointer()) :
                    "Unable to compile the D3D11 built-in shader.";
                if (messages) messages->Release();
                return false;
            }
            if (messages) messages->Release();
            return true;
        }

        class D3D11Renderer final : public Renderer
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
                shutdown_2d();
                for (auto &[handle, target] : render_targets_)
                {
                    if (target.view) target.view->Release();
                }
                render_targets_.clear();
                for (auto &[handle, texture] : textures_) release_texture(texture);
                textures_.clear();
                context_.shutdown();
                window_ = nullptr;
            }
            bool resize(int width, int height, std::string &error) override
            {
                return context_.resize(width, height, error);
            }
            bool set_vsync(bool enabled) override { vsync_enabled_ = enabled; return true; }
            void present() override { context_.present(vsync_enabled_); }
            bool begin_frame(std::string &) override { return context_.is_valid(); }
            bool end_frame(std::string &) override { present(); return true; }
            SDL_GLContext native_context() const override { return nullptr; }

            bool create_texture(const TextureDesc &description, std::uint32_t &texture) override
            {
                texture = 0;
                if (description.width <= 0 || description.height <= 0 || !context_.device()) return false;
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = static_cast<UINT>(description.width);
                desc.Height = static_cast<UINT>(description.height);
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                Texture resource;
                if (FAILED(context_.device()->CreateTexture2D(&desc, nullptr, &resource.resource)) ||
                    FAILED(context_.device()->CreateShaderResourceView(resource.resource, nullptr, &resource.view)))
                {
                    release_texture(resource);
                    return false;
                }
                D3D11_SAMPLER_DESC sampler_desc{};
                sampler_desc.Filter = description.filter == TextureFilter::linear ?
                    D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
                sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
                if (FAILED(context_.device()->CreateSamplerState(&sampler_desc, &resource.sampler)))
                {
                    release_texture(resource);
                    return false;
                }
                texture = next_texture_++;
                textures_.emplace(texture, resource);
                return true;
            }
            bool upload_texture(std::uint32_t texture, int width, int height, const std::uint8_t *pixels) override
            {
                const auto iterator = textures_.find(texture);
                if (iterator == textures_.end() || !pixels || width <= 0 || height <= 0) return false;
                D3D11_BOX box{0, 0, 0, static_cast<UINT>(width), static_cast<UINT>(height), 1};
                context_.context()->UpdateSubresource(iterator->second.resource, 0, &box, pixels,
                    static_cast<UINT>(width * 4), 0);
                return true;
            }
            void destroy_texture(std::uint32_t texture) override
            {
                const auto iterator = textures_.find(texture);
                if (iterator != textures_.end())
                {
                    release_texture(iterator->second);
                    textures_.erase(iterator);
                }
            }
            bool create_render_target(int width, int height, std::uint32_t &texture, std::uint32_t &framebuffer) override
            {
                texture = 0; framebuffer = 0;
                if (!create_texture({width, height, TextureFilter::linear}, texture)) return false;
                const auto iterator = textures_.find(texture);
                ID3D11RenderTargetView *view = nullptr;
                if (iterator == textures_.end() || FAILED(context_.device()->CreateRenderTargetView(
                    iterator->second.resource, nullptr, &view)))
                {
                    destroy_texture(texture);
                    return false;
                }
                framebuffer = next_render_target_++;
                render_targets_.emplace(framebuffer, RenderTarget{view, texture});
                return true;
            }
            void destroy_render_target(std::uint32_t texture, std::uint32_t framebuffer) override
            {
                const auto iterator = render_targets_.find(framebuffer);
                if (iterator != render_targets_.end())
                {
                    if (iterator->second.view) iterator->second.view->Release();
                    render_targets_.erase(iterator);
                }
                destroy_texture(texture);
            }
            bool begin_render_target(std::uint32_t framebuffer, int width, int height, std::string &error) override
            {
                const auto iterator = render_targets_.find(framebuffer);
                if (iterator == render_targets_.end() || width <= 0 || height <= 0)
                {
                    error = "Invalid D3D11 render target.";
                    return false;
                }
                context_.set_render_target(iterator->second.view, width, height);
                return true;
            }
            bool end_render_target(std::string &) override
            {
                context_.bind_backbuffer();
                return true;
            }
            bool create_shader(const ShaderSource &, const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "D3D11 shader compilation is not implemented yet.";
                return false;
            }
            bool create_compute_shader(const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "D3D11 compute shaders are not implemented yet.";
                return false;
            }
            bool create_shader_module(ShaderStage, const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "D3D11 shader modules are not implemented yet.";
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
            bool initialise_2d() override
            {
                if (vertex_shader_) return true;
                const std::string vertex_source = load_shader("default_2d.vert.hlsl");
                const std::string pixel_source = load_shader("default_2d.frag.hlsl");
                if (vertex_source.empty() || pixel_source.empty())
                {
                    last_error_ = "Unable to load the D3D11 default 2D shader assets.";
                    return false;
                }
                ID3DBlob *vertex_blob = nullptr;
                ID3DBlob *pixel_blob = nullptr;
                if (!compile_shader(vertex_source, "main", "vs_4_0", &vertex_blob, last_error_) ||
                    !compile_shader(pixel_source, "main", "ps_4_0", &pixel_blob, last_error_))
                {
                    if (vertex_blob) vertex_blob->Release();
                    if (pixel_blob) pixel_blob->Release();
                    return false;
                }
                const HRESULT created = context_.device()->CreateVertexShader(vertex_blob->GetBufferPointer(),
                    vertex_blob->GetBufferSize(), nullptr, &vertex_shader_);
                const HRESULT pixel_created = context_.device()->CreatePixelShader(pixel_blob->GetBufferPointer(),
                    pixel_blob->GetBufferSize(), nullptr, &pixel_shader_);
                D3D11_INPUT_ELEMENT_DESC elements[] = {
                    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, x)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, u)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, r)), D3D11_INPUT_PER_VERTEX_DATA, 0}};
                const HRESULT layout_created = context_.device()->CreateInputLayout(elements, 3,
                    vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &input_layout_);
                vertex_blob->Release(); pixel_blob->Release();
                D3D11_BUFFER_DESC constant_desc{sizeof(float) * 16, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER,
                    D3D11_CPU_ACCESS_WRITE, 0, 0};
                const HRESULT buffer_created = context_.device()->CreateBuffer(&constant_desc, nullptr, &projection_buffer_);
                if (FAILED(created) || FAILED(pixel_created) || FAILED(layout_created) || FAILED(buffer_created))
                {
                    return false;
                }
                const std::uint8_t white_pixel[4] = {255, 255, 255, 255};
                return create_texture({1, 1, TextureFilter::nearest}, white_texture_) &&
                    upload_texture(white_texture_, 1, 1, white_pixel);
            }
            void shutdown_2d() override
            {
                if (projection_buffer_) projection_buffer_->Release();
                if (input_layout_) input_layout_->Release();
                if (pixel_shader_) pixel_shader_->Release();
                if (vertex_shader_) vertex_shader_->Release();
                if (vertex_buffer_) vertex_buffer_->Release();
                projection_buffer_ = nullptr; input_layout_ = nullptr; pixel_shader_ = nullptr;
                vertex_shader_ = nullptr; vertex_buffer_ = nullptr;
                destroy_texture(white_texture_);
                white_texture_ = 0;
            }
            bool begin_2d(int width, int height) override
            {
                if (!initialise_2d()) return false;
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (FAILED(context_.context()->Map(projection_buffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
                float projection[16] = {width > 0 ? 2.0f / width : 0.0f, 0, 0, 0, 0,
                    height > 0 ? -2.0f / height : 0.0f, 0, 0, 0, 0, -1, 0, -1, 1, 0, 1};
                std::memcpy(mapped.pData, projection, sizeof(projection));
                context_.context()->Unmap(projection_buffer_, 0);
                context_.context()->VSSetConstantBuffers(0, 1, &projection_buffer_);
                context_.context()->VSSetShader(vertex_shader_, nullptr, 0);
                context_.context()->PSSetShader(pixel_shader_, nullptr, 0);
                context_.context()->IASetInputLayout(input_layout_);
                return true;
            }
            bool clear_frame(float red, float green, float blue, float alpha) override
            {
                context_.clear(red, green, blue, alpha);
                return true;
            }
            void submit_2d(PrimitiveType primitive_mode, const Vertex2D *vertices, int count, std::uint32_t texture) override
            {
                if (!vertices || count <= 0 || !initialise_2d()) return;
                if (!vertex_buffer_ || vertex_capacity_ < static_cast<std::size_t>(count))
                {
                    if (vertex_buffer_) vertex_buffer_->Release();
                    D3D11_BUFFER_DESC desc{static_cast<UINT>(sizeof(Vertex2D) * count), D3D11_USAGE_DYNAMIC,
                        D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
                    if (FAILED(context_.device()->CreateBuffer(&desc, nullptr, &vertex_buffer_))) return;
                    vertex_capacity_ = static_cast<std::size_t>(count);
                }
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (FAILED(context_.context()->Map(vertex_buffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
                std::memcpy(mapped.pData, vertices, sizeof(Vertex2D) * count);
                context_.context()->Unmap(vertex_buffer_, 0);
                const auto iterator = textures_.find(texture != 0 ? texture : white_texture_);
                ID3D11ShaderResourceView *view = iterator == textures_.end() ? nullptr : iterator->second.view;
                ID3D11SamplerState *sampler = iterator == textures_.end() ? nullptr : iterator->second.sampler;
                UINT stride = sizeof(Vertex2D), offset = 0;
                context_.context()->IASetVertexBuffers(0, 1, &vertex_buffer_, &stride, &offset);
                context_.context()->PSSetShaderResources(0, 1, &view);
                context_.context()->PSSetSamplers(0, 1, &sampler);
                D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
                if (primitive_mode == PrimitiveType::points) topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
                else if (primitive_mode == PrimitiveType::lines) topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
                else if (primitive_mode == PrimitiveType::triangles) topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                context_.context()->IASetPrimitiveTopology(topology);
                context_.context()->Draw(static_cast<UINT>(count), 0);
            }
            bool create_storage_buffer(std::size_t, std::uint32_t &) override { return unsupported(); }
            void destroy_storage_buffer(std::uint32_t) override {}
            bool upload_storage_buffer(std::uint32_t, std::size_t, const void *, bool) override { return unsupported(); }
            void bind_storage_buffer(unsigned int, std::uint32_t) override {}
            void bind_texture_unit(unsigned int, std::uint32_t) override {}
            void storage_barrier() override {}

        private:
            bool unsupported()
            {
                last_error_ = "D3D11 renderer resource is not implemented yet.";
                return false;
            }

            SDL_Window *window_ = nullptr;
            D3D11Context context_;
            std::string last_error_;
            bool vsync_enabled_ = true;
            std::uint32_t next_texture_ = 1;
            std::uint32_t next_render_target_ = 1;
            std::unordered_map<std::uint32_t, Texture> textures_;
            std::unordered_map<std::uint32_t, RenderTarget> render_targets_;
            ID3D11VertexShader *vertex_shader_ = nullptr;
            ID3D11PixelShader *pixel_shader_ = nullptr;
            ID3D11InputLayout *input_layout_ = nullptr;
            ID3D11Buffer *projection_buffer_ = nullptr;
            ID3D11Buffer *vertex_buffer_ = nullptr;
            std::size_t vertex_capacity_ = 0;
            std::uint32_t white_texture_ = 0;
        };
    }

    std::unique_ptr<Renderer> create_d3d11_renderer()
    {
        return std::make_unique<D3D11Renderer>();
    }
}

#endif // _WIN32
