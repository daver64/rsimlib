/** @file
 * @brief D3D12 renderer; only compiled on Windows.
 */
#ifdef _WIN32

#include "d3d12_renderer.h"
#include "d3d12_context.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <windows.h>

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sl::detail
{
    namespace
    {
        constexpr std::size_t max_batch_vertices = 65536;
        // Each flush within a frame needs its own region; the GPU reads them after recording ends.
        constexpr std::size_t vertex_buffer_vertices = max_batch_vertices * 16;
        constexpr std::uint32_t descriptor_capacity = 4096;
        constexpr std::uint32_t render_target_capacity = 256;

        struct Texture
        {
            ID3D12Resource *resource = nullptr;
            std::uint32_t descriptor = 0;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
        };

        struct RenderTarget
        {
            std::uint32_t texture = 0;
            std::uint32_t descriptor = 0;
            std::uint32_t depth_descriptor = render_target_capacity;
            ID3D12Resource *depth = nullptr;
            int width = 0;
            int height = 0;
        };

        struct Mesh
        {
            ID3D12Resource *vertices = nullptr;
            ID3D12Resource *indices = nullptr;
            D3D12_VERTEX_BUFFER_VIEW vertex_view{};
            D3D12_INDEX_BUFFER_VIEW index_view{};
            UINT index_count = 0;
        };

        std::string load_shader(const char *name)
        {
            std::ifstream file(std::filesystem::path(SIMLIB_D3D_SHADER_DIR) / name);
            if (!file)
            {
                file.clear();
                file.open(std::filesystem::path(SIMLIB_BUILD_D3D_SHADER_DIR) / name);
            }
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
                error = messages ? static_cast<const char *>(messages->GetBufferPointer())
                                 : "Unable to compile the D3D12 built-in shader.";
                if (messages)
                    messages->Release();
                return false;
            }
            if (messages)
                messages->Release();
            return true;
        }

        D3D12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE type)
        {
            D3D12_HEAP_PROPERTIES properties{};
            properties.Type = type;
            properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            properties.CreationNodeMask = 1;
            properties.VisibleNodeMask = 1;
            return properties;
        }

        D3D12_RESOURCE_DESC buffer_description(std::uint64_t size)
        {
            D3D12_RESOURCE_DESC description{};
            description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            description.Width = size;
            description.Height = 1;
            description.DepthOrArraySize = 1;
            description.MipLevels = 1;
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return description;
        }

        void transition_resource(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource,
                                 D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commands->ResourceBarrier(1, &barrier);
        }

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

            bool gui_backend_init() override
            {
                if (imgui_ready_)
                    return true;
                D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
                heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                heap_desc.NumDescriptors = imgui_descriptor_count;
                heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                if (FAILED(context_.device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&imgui_heap_))))
                    return false;
                imgui_descriptor_size_ =
                    context_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                imgui_free_.clear();
                for (UINT index = imgui_descriptor_count; index > 0; --index)
                    imgui_free_.push_back(index - 1);

                ImGui_ImplDX12_InitInfo info;
                info.Device = context_.device();
                info.CommandQueue = context_.command_queue();
                info.NumFramesInFlight = 1;
                info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                info.DSVFormat = DXGI_FORMAT_UNKNOWN;
                info.UserData = this;
                info.SrvDescriptorHeap = imgui_heap_;
                info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo *init, D3D12_CPU_DESCRIPTOR_HANDLE *cpu,
                                               D3D12_GPU_DESCRIPTOR_HANDLE *gpu)
                {
                    auto *self = static_cast<D3D12Renderer *>(init->UserData);
                    const UINT index = self->imgui_free_.back();
                    self->imgui_free_.pop_back();
                    *cpu = self->imgui_heap_->GetCPUDescriptorHandleForHeapStart();
                    *gpu = self->imgui_heap_->GetGPUDescriptorHandleForHeapStart();
                    cpu->ptr += static_cast<SIZE_T>(index) * self->imgui_descriptor_size_;
                    gpu->ptr += static_cast<UINT64>(index) * self->imgui_descriptor_size_;
                };
                info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo *init, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                              D3D12_GPU_DESCRIPTOR_HANDLE)
                {
                    auto *self = static_cast<D3D12Renderer *>(init->UserData);
                    const SIZE_T start = self->imgui_heap_->GetCPUDescriptorHandleForHeapStart().ptr;
                    self->imgui_free_.push_back(static_cast<UINT>((cpu.ptr - start) / self->imgui_descriptor_size_));
                };
                if (!ImGui_ImplDX12_Init(&info))
                {
                    imgui_heap_->Release();
                    imgui_heap_ = nullptr;
                    return false;
                }
                imgui_ready_ = true;
                return true;
            }

            void gui_backend_new_frame() override
            {
                if (imgui_ready_)
                    ImGui_ImplDX12_NewFrame();
            }

            void gui_backend_render() override
            {
                if (!imgui_ready_ || !ensure_frame())
                    return;
                flush_2d();
                auto *commands = context_.command_list();
                ID3D12DescriptorHeap *heaps[] = {imgui_heap_};
                commands->SetDescriptorHeaps(1, heaps);
                ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commands);
            }

            void gui_backend_shutdown() override
            {
                if (!imgui_ready_)
                    return;
                context_.wait_idle();
                ImGui_ImplDX12_Shutdown();
                imgui_ready_ = false;
                if (imgui_heap_)
                    imgui_heap_->Release();
                imgui_heap_ = nullptr;
            }

            void shutdown() override
            {
                gui_backend_shutdown();
                if (context_.frame_open())
                {
                    flush_2d();
                    context_.present(vsync_enabled_);
                }
                else
                {
                    context_.wait_idle();
                }
                shutdown_2d();
                for (auto &[handle, mesh] : meshes_)
                    release_mesh(mesh);
                meshes_.clear();
                for (ID3D12PipelineState *&pipeline : mesh_pipelines_)
                {
                    if (pipeline)
                        pipeline->Release();
                    pipeline = nullptr;
                }
                if (mesh_root_signature_)
                    mesh_root_signature_->Release();
                mesh_root_signature_ = nullptr;
                for (auto &[handle, texture] : textures_)
                    release_texture(texture);
                textures_.clear();
                for (auto &[handle, target] : render_targets_)
                {
                    if (target.depth)
                        target.depth->Release();
                }
                for (auto &texture : retired_textures_)
                    release_texture(texture);
                retired_textures_.clear();
                if (sampler_heap_)
                    sampler_heap_->Release();
                if (srv_heap_)
                    srv_heap_->Release();
                if (render_target_heap_)
                    render_target_heap_->Release();
                if (depth_stencil_heap_)
                    depth_stencil_heap_->Release();
                sampler_heap_ = nullptr;
                srv_heap_ = nullptr;
                render_target_heap_ = nullptr;
                depth_stencil_heap_ = nullptr;
                render_targets_.clear();
                context_.shutdown();
                window_ = nullptr;
            }

            bool resize(int width, int height, std::string &error) override
            {
                flush_2d();
                return context_.resize(width, height, error);
            }

            bool set_vsync(bool enabled) override
            {
                vsync_enabled_ = enabled;
                return true;
            }
            bool vsync_active() const override { return vsync_enabled_; }
            void present() override
            {
                flush_2d();
                suspend_active_render_target();
                context_.present(vsync_enabled_);
                release_retired_textures();
                if (shutdown_2d_pending_)
                    release_2d_resources();
            }
            void wait_idle() override
            {
                if (context_.frame_open())
                    present();
                else
                    context_.wait_idle();
            }
            bool begin_frame(std::string &error) override
            {
                if (context_.begin_frame())
                    return true;
                error = "Unable to begin the D3D12 frame.";
                return false;
            }
            bool end_frame(std::string &error) override
            {
                if (!context_.frame_open())
                    return true;
                present();
                return true;
            }
            SDL_GLContext native_context() const override { return nullptr; }

            bool create_texture(const TextureDesc &description, std::uint32_t &texture) override
            {
                texture = 0;
                if (description.width <= 0 || description.height <= 0 ||
                    !context_.device() || !ensure_descriptor_heaps())
                    return false;

                const std::uint32_t descriptor = acquire_descriptor();
                if (descriptor == descriptor_capacity)
                    return false;

                D3D12_RESOURCE_DESC resource_desc{};
                resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                resource_desc.Width = static_cast<UINT64>(description.width);
                resource_desc.Height = static_cast<UINT>(description.height);
                resource_desc.DepthOrArraySize = 1;
                resource_desc.MipLevels = 1;
                resource_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                resource_desc.SampleDesc.Count = 1;
                resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                resource_desc.Flags = description.render_target ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                                                                 : D3D12_RESOURCE_FLAG_NONE;
                Texture resource;
                resource.descriptor = descriptor;
                const D3D12_HEAP_PROPERTIES default_heap = heap_properties(D3D12_HEAP_TYPE_DEFAULT);
                const HRESULT created = context_.device()->CreateCommittedResource(
                    &default_heap, D3D12_HEAP_FLAG_NONE,
                    &resource_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                    IID_PPV_ARGS(&resource.resource));
                if (FAILED(created))
                {
                    free_descriptors_.push_back(descriptor);
                    return false;
                }

                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MipLevels = 1;
                context_.device()->CreateShaderResourceView(
                    resource.resource, &srv_desc, cpu_descriptor(srv_heap_, srv_descriptor_size_, descriptor));

                D3D12_SAMPLER_DESC sampler_desc{};
                sampler_desc.Filter = description.filter == TextureFilter::linear
                                          ? D3D12_FILTER_MIN_MAG_MIP_LINEAR
                                          : D3D12_FILTER_MIN_MAG_MIP_POINT;
                sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
                context_.device()->CreateSampler(
                    &sampler_desc, cpu_descriptor(sampler_heap_, sampler_descriptor_size_, descriptor));

                texture = next_texture_++;
                if (texture == 0)
                    texture = next_texture_++;
                textures_.emplace(texture, resource);
                return true;
            }

            bool upload_texture(std::uint32_t texture, int width, int height,
                                const std::uint8_t *pixels) override
            {
                const auto found = textures_.find(texture);
                if (found == textures_.end() || !pixels || width <= 0 || height <= 0 ||
                    !context_.device() || !context_.command_queue())
                    return false;

                if (found->second.state == D3D12_RESOURCE_STATE_RENDER_TARGET)
                    return false;
                if (context_.frame_open())
                {
                    flush_2d();
                    context_.present(vsync_enabled_);
                    release_retired_textures();
                }
                if (!context_.wait_idle())
                    return false;

                D3D12_RESOURCE_DESC destination_desc = found->second.resource->GetDesc();
                if (destination_desc.Width != static_cast<UINT64>(width) ||
                    destination_desc.Height != static_cast<UINT>(height))
                    return false;

                D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
                UINT row_count = 0;
                UINT64 unpadded_row_size = 0;
                UINT64 upload_size = 0;
                context_.device()->GetCopyableFootprints(&destination_desc, 0, 1, 0,
                                                         &footprint, &row_count, &unpadded_row_size,
                                                         &upload_size);
                ID3D12Resource *upload = nullptr;
                const D3D12_RESOURCE_DESC upload_desc = buffer_description(upload_size);
                const D3D12_HEAP_PROPERTIES upload_heap = heap_properties(D3D12_HEAP_TYPE_UPLOAD);
                HRESULT result = context_.device()->CreateCommittedResource(
                    &upload_heap, D3D12_HEAP_FLAG_NONE,
                    &upload_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                    IID_PPV_ARGS(&upload));
                if (FAILED(result))
                    return false;

                std::uint8_t *mapped = nullptr;
                D3D12_RANGE read_range{0, 0};
                result = upload->Map(0, &read_range, reinterpret_cast<void **>(&mapped));
                if (FAILED(result))
                {
                    upload->Release();
                    return false;
                }
                const auto *source = pixels;
                for (UINT row = 0; row < row_count; ++row)
                {
                    std::memcpy(mapped + footprint.Offset + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                                source + static_cast<std::size_t>(row) * width * 4,
                                static_cast<std::size_t>(unpadded_row_size));
                }
                upload->Unmap(0, nullptr);

                ID3D12CommandAllocator *allocator = nullptr;
                ID3D12GraphicsCommandList *commands = nullptr;
                result = context_.device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                   IID_PPV_ARGS(&allocator));
                if (SUCCEEDED(result))
                    result = context_.device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                  allocator, nullptr, IID_PPV_ARGS(&commands));
                if (FAILED(result))
                {
                    if (commands)
                        commands->Release();
                    if (allocator)
                        allocator->Release();
                    upload->Release();
                    return false;
                }

                const D3D12_RESOURCE_STATES previous_state = found->second.state;
                if (previous_state != D3D12_RESOURCE_STATE_COPY_DEST)
                    transition_resource(commands, found->second.resource, previous_state,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
                D3D12_TEXTURE_COPY_LOCATION source_location{};
                source_location.pResource = upload;
                source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source_location.PlacedFootprint = footprint;
                D3D12_TEXTURE_COPY_LOCATION destination_location{};
                destination_location.pResource = found->second.resource;
                destination_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                destination_location.SubresourceIndex = 0;
                commands->CopyTextureRegion(&destination_location, 0, 0, 0, &source_location, nullptr);
                transition_resource(commands, found->second.resource,
                                    D3D12_RESOURCE_STATE_COPY_DEST,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                result = commands->Close();
                if (SUCCEEDED(result))
                {
                    ID3D12CommandList *lists[] = {commands};
                    context_.command_queue()->ExecuteCommandLists(1, lists);
                    if (!context_.wait_idle())
                        result = E_FAIL;
                }
                commands->Release();
                allocator->Release();
                upload->Release();
                if (FAILED(result))
                    return false;
                found->second.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                return true;
            }

            bool download_texture(std::uint32_t texture, int width, int height,
                                  std::uint8_t *pixels) override
            {
                const auto found = textures_.find(texture);
                return active_render_target_ == 0 && found != textures_.end() &&
                       readback_resource(found->second.resource, found->second.state, width, height, pixels);
            }

            void destroy_texture(std::uint32_t handle) override
            {
                const auto found = textures_.find(handle);
                if (found == textures_.end())
                    return;
                flush_2d();
                if (context_.frame_open())
                {
                    retired_textures_.push_back(found->second);
                }
                else
                {
                    context_.wait_idle();
                    release_texture(found->second);
                }
                textures_.erase(found);
            }

            bool create_render_target(int width, int height, std::uint32_t &texture,
                                      std::uint32_t &framebuffer) override
            {
                texture = 0;
                framebuffer = 0;
                if (width <= 0 || height <= 0 || !ensure_render_target_heap())
                    return false;
                const std::uint32_t descriptor = acquire_render_target_descriptor();
                if (descriptor == render_target_capacity)
                    return false;
                if (!create_texture({width, height, TextureFilter::linear, true}, texture))
                {
                    free_render_target_descriptors_.push_back(descriptor);
                    return false;
                }
                const auto resource = textures_.find(texture);
                if (resource == textures_.end())
                {
                    destroy_texture(texture);
                    free_render_target_descriptors_.push_back(descriptor);
                    texture = 0;
                    return false;
                }
                context_.device()->CreateRenderTargetView(
                    resource->second.resource, nullptr,
                    cpu_descriptor(render_target_heap_, render_target_descriptor_size_, descriptor));
                framebuffer = next_render_target_++;
                if (framebuffer == 0)
                    framebuffer = next_render_target_++;
                RenderTarget target;
                target.texture = texture;
                target.descriptor = descriptor;
                target.width = width;
                target.height = height;
                render_targets_.emplace(framebuffer, target);
                return true;
            }
            void destroy_render_target(std::uint32_t texture, std::uint32_t framebuffer) override
            {
                const auto target = render_targets_.find(framebuffer);
                if (target == render_targets_.end())
                    return;
                if (active_render_target_ == framebuffer)
                {
                    std::string error;
                    end_render_target(error);
                }
                flush_2d();
                if (context_.frame_open())
                {
                    context_.present(vsync_enabled_);
                    release_retired_textures();
                }
                if (!context_.wait_idle())
                    return;
                free_render_target_descriptors_.push_back(target->second.descriptor);
                if (target->second.depth)
                {
                    target->second.depth->Release();
                    free_depth_stencil_descriptors_.push_back(target->second.depth_descriptor);
                }
                render_targets_.erase(target);
                destroy_texture(texture);
            }
            bool begin_render_target(std::uint32_t framebuffer, int width, int height,
                                     std::string &error) override
            {
                const auto target = render_targets_.find(framebuffer);
                const auto texture = target == render_targets_.end() ? textures_.end()
                                                                      : textures_.find(target->second.texture);
                if (target == render_targets_.end() || texture == textures_.end() ||
                    target->second.width != width || target->second.height != height ||
                    active_render_target_ != 0 || !ensure_frame())
                {
                    error = "Invalid or already-active D3D12 render target.";
                    return false;
                }
                flush_2d();
                if (texture->second.state != D3D12_RESOURCE_STATE_RENDER_TARGET)
                {
                    transition_resource(context_.command_list(), texture->second.resource,
                                        texture->second.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
                    texture->second.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                }
                D3D12_CPU_DESCRIPTOR_HANDLE handle =
                    cpu_descriptor(render_target_heap_, render_target_descriptor_size_,
                                   target->second.descriptor);
                D3D12_CPU_DESCRIPTOR_HANDLE depth_handle{};
                const bool has_depth = target->second.depth != nullptr;
                if (has_depth)
                    depth_handle = cpu_descriptor(depth_stencil_heap_, depth_stencil_descriptor_size_,
                                                  target->second.depth_descriptor);
                context_.command_list()->OMSetRenderTargets(1, &handle, FALSE,
                                                            has_depth ? &depth_handle : nullptr);
                D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width),
                                        static_cast<float>(height), 0.0f, 1.0f};
                D3D12_RECT scissor{0, 0, width, height};
                context_.command_list()->RSSetViewports(1, &viewport);
                context_.command_list()->RSSetScissorRects(1, &scissor);
                active_render_target_ = framebuffer;
                return true;
            }
            bool end_render_target(std::string &error) override
            {
                if (active_render_target_ == 0)
                {
                    error = "No D3D12 render target is active.";
                    return false;
                }
                if (!ensure_frame())
                {
                    error = last_error_;
                    return false;
                }
                flush_2d();
                const auto target = render_targets_.find(active_render_target_);
                if (target == render_targets_.end())
                {
                    active_render_target_ = 0;
                    error = "The active D3D12 render target no longer exists.";
                    return false;
                }
                const auto texture = textures_.find(target->second.texture);
                if (texture == textures_.end())
                {
                    active_render_target_ = 0;
                    error = "The active D3D12 render-target texture no longer exists.";
                    return false;
                }
                if (texture->second.state == D3D12_RESOURCE_STATE_RENDER_TARGET)
                    transition_resource(context_.command_list(), texture->second.resource,
                                        D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                texture->second.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                active_render_target_ = 0;
                context_.bind_backbuffer();
                return true;
            }
            bool download_render_target(std::uint32_t framebuffer, int width, int height,
                                        std::uint8_t *pixels) override
            {
                ID3D12Resource *resource = nullptr;
                D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                if (framebuffer == 0)
                {
                    resource = context_.back_buffer();
                    state = D3D12_RESOURCE_STATE_PRESENT;
                    if (active_render_target_ != 0)
                        return false;
                }
                else
                {
                    const auto target = render_targets_.find(framebuffer);
                    if (target == render_targets_.end() || active_render_target_ == framebuffer)
                        return false;
                    const auto texture = textures_.find(target->second.texture);
                    if (texture == textures_.end() || target->second.width != width ||
                        target->second.height != height)
                        return false;
                    resource = texture->second.resource;
                    state = texture->second.state;
                }
                return readback_resource(resource, state, width, height, pixels);
            }
            bool create_render_target_3d(int width, int height, int &samples,
                                         std::uint32_t &texture, std::uint32_t &framebuffer) override
            {
                samples = 1;
                if (!create_render_target(width, height, texture, framebuffer))
                    return false;
                if (!ensure_depth_stencil_heap())
                {
                    destroy_render_target(texture, framebuffer);
                    texture = 0;
                    framebuffer = 0;
                    return false;
                }
                const std::uint32_t descriptor = acquire_depth_stencil_descriptor();
                if (descriptor == render_target_capacity)
                {
                    destroy_render_target(texture, framebuffer);
                    texture = 0;
                    framebuffer = 0;
                    return false;
                }
                D3D12_RESOURCE_DESC resource_desc{};
                resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                resource_desc.Width = static_cast<UINT64>(width);
                resource_desc.Height = static_cast<UINT>(height);
                resource_desc.DepthOrArraySize = 1;
                resource_desc.MipLevels = 1;
                resource_desc.Format = DXGI_FORMAT_D32_FLOAT;
                resource_desc.SampleDesc.Count = 1;
                resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
                D3D12_CLEAR_VALUE clear_value{};
                clear_value.Format = DXGI_FORMAT_D32_FLOAT;
                clear_value.DepthStencil.Depth = 1.0f;
                const D3D12_HEAP_PROPERTIES default_heap = heap_properties(D3D12_HEAP_TYPE_DEFAULT);
                ID3D12Resource *depth = nullptr;
                if (FAILED(context_.device()->CreateCommittedResource(
                        &default_heap, D3D12_HEAP_FLAG_NONE, &resource_desc,
                        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&depth))))
                {
                    free_depth_stencil_descriptors_.push_back(descriptor);
                    destroy_render_target(texture, framebuffer);
                    texture = 0;
                    framebuffer = 0;
                    return false;
                }
                D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
                dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                context_.device()->CreateDepthStencilView(
                    depth, &dsv_desc,
                    cpu_descriptor(depth_stencil_heap_, depth_stencil_descriptor_size_, descriptor));
                const auto target = render_targets_.find(framebuffer);
                if (target == render_targets_.end())
                {
                    depth->Release();
                    free_depth_stencil_descriptors_.push_back(descriptor);
                    destroy_render_target(texture, framebuffer);
                    texture = 0;
                    framebuffer = 0;
                    return false;
                }
                target->second.depth = depth;
                target->second.depth_descriptor = descriptor;
                return true;
            }
            bool create_mesh(const Vertex3D *vertices, std::size_t vertex_count,
                             const std::uint32_t *indices, std::size_t index_count,
                             std::uint32_t &handle) override
            {
                handle = 0;
                if (!vertices || !indices || vertex_count == 0 || index_count == 0 ||
                    index_count > UINT_MAX ||
                    vertex_count > UINT_MAX / sizeof(Vertex3D) ||
                    index_count > UINT_MAX / sizeof(std::uint32_t) ||
                    !context_.device())
                    return false;

                Mesh mesh;
                const D3D12_HEAP_PROPERTIES upload_heap = heap_properties(D3D12_HEAP_TYPE_UPLOAD);
                const D3D12_RESOURCE_DESC vertex_desc =
                    buffer_description(sizeof(Vertex3D) * vertex_count);
                HRESULT result = context_.device()->CreateCommittedResource(
                    &upload_heap, D3D12_HEAP_FLAG_NONE, &vertex_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mesh.vertices));
                if (FAILED(result))
                    return false;
                const D3D12_RESOURCE_DESC index_desc =
                    buffer_description(sizeof(std::uint32_t) * index_count);
                result = context_.device()->CreateCommittedResource(
                    &upload_heap, D3D12_HEAP_FLAG_NONE, &index_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mesh.indices));
                if (FAILED(result))
                {
                    release_mesh(mesh);
                    return false;
                }
                void *mapped = nullptr;
                const D3D12_RANGE read_range{0, 0};
                result = mesh.vertices->Map(0, &read_range, &mapped);
                if (FAILED(result))
                {
                    release_mesh(mesh);
                    return false;
                }
                std::memcpy(mapped, vertices, sizeof(Vertex3D) * vertex_count);
                const D3D12_RANGE written_range{0, 0};
                mesh.vertices->Unmap(0, &written_range);
                mapped = nullptr;
                result = mesh.indices->Map(0, &read_range, &mapped);
                if (FAILED(result))
                {
                    release_mesh(mesh);
                    return false;
                }
                std::memcpy(mapped, indices, sizeof(std::uint32_t) * index_count);
                mesh.indices->Unmap(0, &written_range);
                mesh.vertex_view.BufferLocation = mesh.vertices->GetGPUVirtualAddress();
                mesh.vertex_view.SizeInBytes = static_cast<UINT>(sizeof(Vertex3D) * vertex_count);
                mesh.vertex_view.StrideInBytes = sizeof(Vertex3D);
                mesh.index_view.BufferLocation = mesh.indices->GetGPUVirtualAddress();
                mesh.index_view.SizeInBytes = static_cast<UINT>(sizeof(std::uint32_t) * index_count);
                mesh.index_view.Format = DXGI_FORMAT_R32_UINT;
                mesh.index_count = static_cast<UINT>(index_count);
                handle = next_mesh_++;
                if (handle == 0)
                    handle = next_mesh_++;
                meshes_.emplace(handle, mesh);
                return true;
            }
            void destroy_mesh(std::uint32_t handle) override
            {
                const auto found = meshes_.find(handle);
                if (found == meshes_.end())
                    return;
                flush_2d();
                if (context_.frame_open())
                    present();
                if (!context_.wait_idle())
                    return;
                release_mesh(found->second);
                meshes_.erase(found);
            }
            bool draw_mesh(std::uint32_t handle, const MeshDrawParams &params) override
            {
                const auto mesh = meshes_.find(handle);
                if (mesh == meshes_.end() || active_render_target_ == 0 ||
                    !initialise_2d() || !initialise_mesh_pipeline() || !ensure_frame())
                    return false;
                const auto target = render_targets_.find(active_render_target_);
                if (target == render_targets_.end() || !target->second.depth)
                    return false;
                flush_2d();

                auto texture = textures_.find(params.texture != 0 ? params.texture : white_texture_);
                if (texture == textures_.end() ||
                    texture->second.state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
                    texture = textures_.find(white_texture_);
                if (texture == textures_.end())
                    return false;

                std::array<float, 32> constants{};
                std::copy(std::begin(params.mvp), std::end(params.mvp), constants.begin());
                std::copy(&params.normal_rows[0][0], &params.normal_rows[0][0] + 12,
                          constants.begin() + 16);
                std::copy(std::begin(params.light), std::end(params.light), constants.begin() + 28);

                ID3D12GraphicsCommandList *commands = context_.command_list();
                ID3D12DescriptorHeap *heaps[] = {srv_heap_, sampler_heap_};
                commands->SetDescriptorHeaps(2, heaps);
                commands->SetGraphicsRootSignature(mesh_root_signature_);
                commands->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(constants.size()),
                                                         constants.data(), 0);
                commands->SetGraphicsRootDescriptorTable(
                    1, gpu_descriptor(srv_heap_, srv_descriptor_size_, texture->second.descriptor));
                commands->SetGraphicsRootDescriptorTable(
                    2, gpu_descriptor(sampler_heap_, sampler_descriptor_size_, texture->second.descriptor));
                commands->SetPipelineState(mesh_pipelines_[static_cast<std::size_t>(params.cull)]);
                commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                commands->IASetVertexBuffers(0, 1, &mesh->second.vertex_view);
                commands->IASetIndexBuffer(&mesh->second.index_view);
                commands->DrawIndexedInstanced(mesh->second.index_count, 1, 0, 0, 0);
                return true;
            }
            bool clear_3d(float red, float green, float blue, float alpha) override
            {
                if (active_render_target_ == 0 || !ensure_frame())
                    return false;
                const auto target = render_targets_.find(active_render_target_);
                if (target == render_targets_.end() || !target->second.depth)
                    return false;
                flush_2d();
                D3D12_CPU_DESCRIPTOR_HANDLE color =
                    cpu_descriptor(render_target_heap_, render_target_descriptor_size_,
                                   target->second.descriptor);
                D3D12_CPU_DESCRIPTOR_HANDLE depth =
                    cpu_descriptor(depth_stencil_heap_, depth_stencil_descriptor_size_,
                                   target->second.depth_descriptor);
                const float colour[4] = {red, green, blue, alpha};
                context_.command_list()->ClearRenderTargetView(color, colour, 0, nullptr);
                context_.command_list()->ClearDepthStencilView(
                    depth, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                return true;
            }
            bool create_shader(const ShaderSource &, const ShaderSource &, std::uint32_t &,
                               std::string &error) override
            {
                error = "D3D12 user shader compilation is not implemented yet.";
                return false;
            }
            bool create_compute_shader(const ShaderSource &, std::uint32_t &, std::string &error) override
            {
                error = "D3D12 compute shaders are not implemented yet.";
                return false;
            }
            bool create_shader_module(ShaderStage, const ShaderSource &, std::uint32_t &,
                                      std::string &error) override
            {
                error = "D3D12 shader modules are not implemented yet.";
                return false;
            }
            void destroy_shader(std::uint32_t) override {}
            bool use_shader(std::uint32_t) override { return unsupported(); }
            void stop_shader() override {}
            bool dispatch_compute(std::uint32_t, unsigned int, unsigned int, unsigned int) override
            {
                return unsupported();
            }
            bool set_shader_int(std::uint32_t, const char *, int) override { return unsupported(); }
            bool set_shader_float(std::uint32_t, const char *, float) override { return unsupported(); }
            bool set_shader_float2(std::uint32_t, const char *, float, float) override { return unsupported(); }
            bool set_shader_int2(std::uint32_t, const char *, int, int) override { return unsupported(); }
            bool set_shader_float3(std::uint32_t, const char *, float, float, float) override { return unsupported(); }
            bool set_shader_mat4(std::uint32_t, const char *, const float *) override { return unsupported(); }

            bool initialise_2d() override
            {
                if (initialized_2d_)
                    return true;
                if (root_signature_)
                    return false;
                shutdown_2d_pending_ = false;
                if (!ensure_descriptor_heaps())
                    return false;

                const std::string vertex_source = load_shader("default_2d.vert.hlsl");
                const std::string pixel_source = load_shader("default_2d.frag.hlsl");
                if (vertex_source.empty() || pixel_source.empty())
                {
                    last_error_ = "Unable to load the D3D12 default 2D shader assets.";
                    return false;
                }
                ID3DBlob *vertex_blob = nullptr;
                ID3DBlob *pixel_blob = nullptr;
                if (!compile_shader(vertex_source, "main", "vs_5_0", &vertex_blob, last_error_) ||
                    !compile_shader(pixel_source, "main", "ps_5_0", &pixel_blob, last_error_))
                {
                    if (vertex_blob)
                        vertex_blob->Release();
                    if (pixel_blob)
                        pixel_blob->Release();
                    return false;
                }

                D3D12_DESCRIPTOR_RANGE ranges[2]{};
                ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                ranges[0].NumDescriptors = 1;
                ranges[0].BaseShaderRegister = 0;
                ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                ranges[1].NumDescriptors = 1;
                ranges[1].BaseShaderRegister = 0;
                ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                D3D12_ROOT_PARAMETER parameters[3]{};
                parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                parameters[0].Constants.ShaderRegister = 0;
                parameters[0].Constants.RegisterSpace = 0;
                parameters[0].Constants.Num32BitValues = 16;
                parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
                for (UINT index = 0; index < 2; ++index)
                {
                    parameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    parameters[index + 1].DescriptorTable.NumDescriptorRanges = 1;
                    parameters[index + 1].DescriptorTable.pDescriptorRanges = &ranges[index];
                    parameters[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
                }
                D3D12_ROOT_SIGNATURE_DESC signature_desc{};
                signature_desc.NumParameters = 3;
                signature_desc.pParameters = parameters;
                signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
                ID3DBlob *serialized = nullptr;
                ID3DBlob *messages = nullptr;
                HRESULT result = D3D12SerializeRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                             &serialized, &messages);
                if (FAILED(result))
                {
                    last_error_ = messages ? static_cast<const char *>(messages->GetBufferPointer())
                                           : "Unable to serialize the D3D12 2D root signature.";
                    if (messages)
                        messages->Release();
                    vertex_blob->Release();
                    pixel_blob->Release();
                    return false;
                }
                if (messages)
                    messages->Release();
                result = context_.device()->CreateRootSignature(0, serialized->GetBufferPointer(),
                                                                serialized->GetBufferSize(),
                                                                IID_PPV_ARGS(&root_signature_));
                serialized->Release();
                if (FAILED(result))
                {
                    vertex_blob->Release();
                    pixel_blob->Release();
                    last_error_ = "Unable to create the D3D12 2D root signature.";
                    return false;
                }

                const D3D12_INPUT_ELEMENT_DESC elements[] = {
                    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                     static_cast<UINT>(offsetof(Vertex2D, x)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                     static_cast<UINT>(offsetof(Vertex2D, u)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                     static_cast<UINT>(offsetof(Vertex2D, r)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
                D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc{};
                pipeline_desc.pRootSignature = root_signature_;
                pipeline_desc.VS = {vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize()};
                pipeline_desc.PS = {pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize()};
                pipeline_desc.BlendState.AlphaToCoverageEnable = FALSE;
                pipeline_desc.BlendState.IndependentBlendEnable = FALSE;
                auto &blend = pipeline_desc.BlendState.RenderTarget[0];
                blend.BlendEnable = TRUE;
                blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                blend.BlendOp = D3D12_BLEND_OP_ADD;
                blend.SrcBlendAlpha = D3D12_BLEND_ONE;
                blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                pipeline_desc.SampleMask = UINT_MAX;
                pipeline_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
                pipeline_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
                pipeline_desc.RasterizerState.DepthClipEnable = TRUE;
                pipeline_desc.DepthStencilState.DepthEnable = FALSE;
                pipeline_desc.DepthStencilState.StencilEnable = FALSE;
                pipeline_desc.InputLayout = {elements, static_cast<UINT>(std::size(elements))};
                pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                pipeline_desc.NumRenderTargets = 1;
                pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
                pipeline_desc.SampleDesc.Count = 1;
                for (std::size_t index = 0; index < pipelines_.size(); ++index)
                {
                    result = context_.device()->CreateGraphicsPipelineState(&pipeline_desc,
                                                                            IID_PPV_ARGS(&pipelines_[index]));
                    if (FAILED(result))
                        break;
                    if (index == 0)
                        pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
                    else if (index == 1)
                        pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
                }
                vertex_blob->Release();
                pixel_blob->Release();
                if (FAILED(result))
                {
                    last_error_ = "Unable to create the D3D12 2D pipeline states.";
                    shutdown_2d();
                    return false;
                }

                const D3D12_HEAP_PROPERTIES upload_heap = heap_properties(D3D12_HEAP_TYPE_UPLOAD);
                const D3D12_RESOURCE_DESC vertex_desc =
                    buffer_description(sizeof(Vertex2D) * vertex_buffer_vertices);
                result = context_.device()->CreateCommittedResource(
                    &upload_heap, D3D12_HEAP_FLAG_NONE, &vertex_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertex_buffer_));
                if (FAILED(result))
                {
                    last_error_ = "Unable to create the D3D12 2D vertex buffer.";
                    shutdown_2d();
                    return false;
                }
                D3D12_RANGE read_range{0, 0};
                result = vertex_buffer_->Map(0, &read_range, reinterpret_cast<void **>(&mapped_vertices_));
                if (FAILED(result))
                {
                    mapped_vertices_ = nullptr;
                    last_error_ = "Unable to map the D3D12 2D vertex buffer.";
                    shutdown_2d();
                    return false;
                }
                vertex_capacity_ = vertex_buffer_vertices;
                vertex_offset_ = 0;

                const std::uint8_t white_pixel[4] = {255, 255, 255, 255};
                if (!create_texture({1, 1, TextureFilter::nearest}, white_texture_) ||
                    !upload_texture(white_texture_, 1, 1, white_pixel))
                {
                    shutdown_2d();
                    return false;
                }
                initialized_2d_ = true;
                return true;
            }

            void shutdown_2d() override
            {
                if (context_.frame_open())
                {
                    flush_2d();
                    shutdown_2d_pending_ = true;
                    return;
                }
                context_.wait_idle();
                release_2d_resources();
            }

            void release_2d_resources()
            {
                batch_vertices_.clear();
                if (vertex_buffer_)
                {
                    if (mapped_vertices_)
                        vertex_buffer_->Unmap(0, nullptr);
                    vertex_buffer_->Release();
                }
                vertex_buffer_ = nullptr;
                mapped_vertices_ = nullptr;
                vertex_capacity_ = 0;
                for (auto *&pipeline : pipelines_)
                {
                    if (pipeline)
                        pipeline->Release();
                    pipeline = nullptr;
                }
                if (root_signature_)
                    root_signature_->Release();
                root_signature_ = nullptr;
                destroy_texture(white_texture_);
                white_texture_ = 0;
                initialized_2d_ = false;
                shutdown_2d_pending_ = false;
            }

            bool begin_2d(int width, int height) override
            {
                if (!initialise_2d() || !ensure_frame())
                    return false;
                flush_2d();
                projection_ = {width > 0 ? 2.0f / width : 0.0f, 0, 0, 0,
                               0, height > 0 ? -2.0f / height : 0.0f, 0, 0,
                               0, 0, -1, 0, -1, 1, 0, 1};
                D3D12_VIEWPORT viewport{0.0f, 0.0f,
                                        static_cast<float>((std::max)(width, 0)),
                                        static_cast<float>((std::max)(height, 0)), 0.0f, 1.0f};
                D3D12_RECT scissor{0, 0, (std::max)(width, 0), (std::max)(height, 0)};
                auto *commands = context_.command_list();
                commands->RSSetViewports(1, &viewport);
                commands->RSSetScissorRects(1, &scissor);
                commands->SetGraphicsRootSignature(root_signature_);
                ID3D12DescriptorHeap *heaps[] = {srv_heap_, sampler_heap_};
                commands->SetDescriptorHeaps(2, heaps);
                commands->SetGraphicsRoot32BitConstants(0, 16, projection_.data(), 0);
                return true;
            }

            bool begin_shader_2d(std::uint32_t, int, int) override { return unsupported(); }
            void set_premultiplied_alpha(bool) override {}
            void set_blend_mode(BlendMode) override {}
            bool clear_frame(float red, float green, float blue, float alpha) override
            {
                if (!ensure_frame())
                    return false;
                flush_2d();
                if (active_render_target_ == 0)
                {
                    context_.clear(red, green, blue, alpha);
                }
                else
                {
                    const auto target = render_targets_.find(active_render_target_);
                    if (target == render_targets_.end())
                        return false;
                    D3D12_CPU_DESCRIPTOR_HANDLE handle =
                        cpu_descriptor(render_target_heap_, render_target_descriptor_size_,
                                       target->second.descriptor);
                    const float colour[4] = {red, green, blue, alpha};
                    context_.command_list()->ClearRenderTargetView(handle, colour, 0, nullptr);
                }
                return true;
            }

            void flush_2d() override
            {
                if (batch_vertices_.empty() || !mapped_vertices_ || !context_.frame_open())
                    return;
                const std::size_t count = batch_vertices_.size();
                if (vertex_frame_serial_ != context_.frame_serial())
                {
                    vertex_frame_serial_ = context_.frame_serial();
                    vertex_offset_ = 0;
                }
                if (vertex_offset_ + count > vertex_capacity_)
                {
                    batch_vertices_.clear();
                    return;
                }
                const std::size_t first_vertex = vertex_offset_;
                std::memcpy(mapped_vertices_ + first_vertex, batch_vertices_.data(), sizeof(Vertex2D) * count);
                vertex_offset_ += count;

                auto texture = textures_.find(batch_texture_ != 0 ? batch_texture_ : white_texture_);
                if (texture == textures_.end() ||
                    texture->second.state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
                    texture = textures_.find(white_texture_);
                if (texture == textures_.end())
                {
                    batch_vertices_.clear();
                    return;
                }

                auto *commands = context_.command_list();
                std::size_t pipeline_index = 0;
                D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                if (batch_primitive_ == PrimitiveType::lines)
                {
                    pipeline_index = 1;
                    topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
                }
                else if (batch_primitive_ == PrimitiveType::points)
                {
                    pipeline_index = 2;
                    topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
                }
                commands->SetPipelineState(pipelines_[pipeline_index]);
                commands->SetGraphicsRootDescriptorTable(
                    1, gpu_descriptor(srv_heap_, srv_descriptor_size_, texture->second.descriptor));
                commands->SetGraphicsRootDescriptorTable(
                    2, gpu_descriptor(sampler_heap_, sampler_descriptor_size_, texture->second.descriptor));
                D3D12_VERTEX_BUFFER_VIEW vertex_view{};
                vertex_view.BufferLocation =
                    vertex_buffer_->GetGPUVirtualAddress() + sizeof(Vertex2D) * first_vertex;
                vertex_view.SizeInBytes = static_cast<UINT>(sizeof(Vertex2D) * count);
                vertex_view.StrideInBytes = sizeof(Vertex2D);
                commands->IASetVertexBuffers(0, 1, &vertex_view);
                commands->IASetPrimitiveTopology(topology);
                commands->DrawInstanced(static_cast<UINT>(count), 1, 0, 0);
                batch_vertices_.clear();
            }

            void submit_2d(PrimitiveType primitive_mode, const Vertex2D *vertices,
                           int count, std::uint32_t texture) override
            {
                if (!vertices || count <= 0 || !initialise_2d())
                    return;
                const std::uint32_t resolved_texture = texture != 0 ? texture : white_texture_;
                const PrimitiveType target_primitive = get_target_batch_primitive(primitive_mode);
                const int new_vertex_count = get_decomposed_vertex_count(primitive_mode, count);
                if (new_vertex_count <= 0)
                    return;
                if (!batch_vertices_.empty() &&
                    (batch_texture_ != resolved_texture ||
                     batch_primitive_ != target_primitive ||
                     batch_vertices_.size() + static_cast<std::size_t>(new_vertex_count) > max_batch_vertices))
                    flush_2d();
                if (batch_vertices_.empty())
                {
                    batch_texture_ = resolved_texture;
                    batch_primitive_ = target_primitive;
                }
                PrimitiveType actual_primitive = batch_primitive_;
                append_decomposed_vertices(primitive_mode, vertices, count,
                                           actual_primitive, batch_vertices_);
            }

            bool create_storage_buffer(std::size_t, std::uint32_t &) override { return unsupported(); }
            void destroy_storage_buffer(std::uint32_t) override {}
            bool upload_storage_buffer(std::uint32_t, std::size_t, const void *, bool) override
            {
                return unsupported();
            }
            bool readback_storage_buffer(std::uint32_t, std::size_t, std::size_t, void *) override
            {
                return unsupported();
            }
            void bind_storage_buffer(unsigned int, std::uint32_t) override {}
            void bind_texture_unit(unsigned int, std::uint32_t) override {}
            void bind_storage_texture(unsigned int, std::uint32_t) override {}
            void storage_barrier() override {}

        private:
            bool initialise_mesh_pipeline()
            {
                if (mesh_root_signature_)
                    return true;
                if (!ensure_descriptor_heaps())
                    return false;
                const std::string vertex_source = load_shader("mesh3d.vert.hlsl");
                const std::string pixel_source = load_shader("mesh3d.frag.hlsl");
                if (vertex_source.empty() || pixel_source.empty())
                {
                    last_error_ = "Unable to load the D3D12 3D mesh shaders.";
                    return false;
                }
                ID3DBlob *vertex_blob = nullptr;
                ID3DBlob *pixel_blob = nullptr;
                if (!compile_shader(vertex_source, "main", "vs_5_0", &vertex_blob, last_error_) ||
                    !compile_shader(pixel_source, "main", "ps_5_0", &pixel_blob, last_error_))
                {
                    if (vertex_blob)
                        vertex_blob->Release();
                    if (pixel_blob)
                        pixel_blob->Release();
                    return false;
                }

                D3D12_DESCRIPTOR_RANGE ranges[2]{};
                ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                ranges[0].NumDescriptors = 1;
                ranges[0].BaseShaderRegister = 0;
                ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                ranges[1].NumDescriptors = 1;
                ranges[1].BaseShaderRegister = 0;
                ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                D3D12_ROOT_PARAMETER parameters[3]{};
                parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                parameters[0].Constants.ShaderRegister = 0;
                parameters[0].Constants.Num32BitValues = 32;
                parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                for (UINT index = 0; index < 2; ++index)
                {
                    parameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    parameters[index + 1].DescriptorTable.NumDescriptorRanges = 1;
                    parameters[index + 1].DescriptorTable.pDescriptorRanges = &ranges[index];
                    parameters[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
                }
                D3D12_ROOT_SIGNATURE_DESC signature_desc{};
                signature_desc.NumParameters = static_cast<UINT>(std::size(parameters));
                signature_desc.pParameters = parameters;
                signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
                ID3DBlob *serialized = nullptr;
                ID3DBlob *messages = nullptr;
                HRESULT result = D3D12SerializeRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                             &serialized, &messages);
                if (FAILED(result))
                {
                    last_error_ = messages ? static_cast<const char *>(messages->GetBufferPointer())
                                           : "Unable to serialize the D3D12 mesh root signature.";
                    if (messages)
                        messages->Release();
                    vertex_blob->Release();
                    pixel_blob->Release();
                    return false;
                }
                if (messages)
                    messages->Release();
                result = context_.device()->CreateRootSignature(
                    0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                    IID_PPV_ARGS(&mesh_root_signature_));
                serialized->Release();
                if (FAILED(result))
                {
                    vertex_blob->Release();
                    pixel_blob->Release();
                    last_error_ = "Unable to create the D3D12 mesh root signature.";
                    return false;
                }

                const D3D12_INPUT_ELEMENT_DESC elements[] = {
                    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                     static_cast<UINT>(offsetof(Vertex3D, position)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                     static_cast<UINT>(offsetof(Vertex3D, normal)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                     static_cast<UINT>(offsetof(Vertex3D, colour)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                     static_cast<UINT>(offsetof(Vertex3D, uv)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
                D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc{};
                pipeline_desc.pRootSignature = mesh_root_signature_;
                pipeline_desc.VS = {vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize()};
                pipeline_desc.PS = {pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize()};
                pipeline_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                pipeline_desc.SampleMask = UINT_MAX;
                pipeline_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
                pipeline_desc.RasterizerState.FrontCounterClockwise = TRUE;
                pipeline_desc.RasterizerState.DepthClipEnable = TRUE;
                pipeline_desc.DepthStencilState.DepthEnable = TRUE;
                pipeline_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
                pipeline_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
                pipeline_desc.DepthStencilState.StencilEnable = FALSE;
                pipeline_desc.InputLayout = {elements, static_cast<UINT>(std::size(elements))};
                pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                pipeline_desc.NumRenderTargets = 1;
                pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
                pipeline_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
                pipeline_desc.SampleDesc.Count = 1;
                for (std::size_t index = 0; index < mesh_pipelines_.size(); ++index)
                {
                    pipeline_desc.RasterizerState.CullMode =
                        index == 0 ? D3D12_CULL_MODE_BACK :
                        index == 1 ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_NONE;
                    result = context_.device()->CreateGraphicsPipelineState(
                        &pipeline_desc, IID_PPV_ARGS(&mesh_pipelines_[index]));
                    if (FAILED(result))
                        break;
                }
                vertex_blob->Release();
                pixel_blob->Release();
                if (FAILED(result))
                {
                    last_error_ = "Unable to create the D3D12 mesh pipeline states.";
                    for (ID3D12PipelineState *&pipeline : mesh_pipelines_)
                    {
                        if (pipeline)
                            pipeline->Release();
                        pipeline = nullptr;
                    }
                    mesh_root_signature_->Release();
                    mesh_root_signature_ = nullptr;
                    return false;
                }
                return true;
            }

            static void release_mesh(Mesh &mesh)
            {
                if (mesh.vertices)
                    mesh.vertices->Release();
                if (mesh.indices)
                    mesh.indices->Release();
                mesh = {};
            }

            bool ensure_render_target_heap()
            {
                if (render_target_heap_)
                    return true;
                D3D12_DESCRIPTOR_HEAP_DESC desc{};
                desc.NumDescriptors = render_target_capacity;
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                if (FAILED(context_.device()->CreateDescriptorHeap(&desc,
                                                                   IID_PPV_ARGS(&render_target_heap_))))
                    return false;
                render_target_descriptor_size_ =
                    context_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                return true;
            }

            bool ensure_depth_stencil_heap()
            {
                if (depth_stencil_heap_)
                    return true;
                D3D12_DESCRIPTOR_HEAP_DESC desc{};
                desc.NumDescriptors = render_target_capacity;
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                if (FAILED(context_.device()->CreateDescriptorHeap(&desc,
                                                                   IID_PPV_ARGS(&depth_stencil_heap_))))
                    return false;
                depth_stencil_descriptor_size_ =
                    context_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
                return true;
            }

            std::uint32_t acquire_render_target_descriptor()
            {
                if (!free_render_target_descriptors_.empty())
                {
                    const std::uint32_t descriptor = free_render_target_descriptors_.back();
                    free_render_target_descriptors_.pop_back();
                    return descriptor;
                }
                if (next_render_target_descriptor_ >= render_target_capacity)
                    return render_target_capacity;
                return next_render_target_descriptor_++;
            }

            std::uint32_t acquire_depth_stencil_descriptor()
            {
                if (!free_depth_stencil_descriptors_.empty())
                {
                    const std::uint32_t descriptor = free_depth_stencil_descriptors_.back();
                    free_depth_stencil_descriptors_.pop_back();
                    return descriptor;
                }
                if (next_depth_stencil_descriptor_ >= render_target_capacity)
                    return render_target_capacity;
                return next_depth_stencil_descriptor_++;
            }

            bool readback_resource(ID3D12Resource *resource, D3D12_RESOURCE_STATES source_state,
                                   int width, int height, std::uint8_t *pixels)
            {
                if (!resource || !pixels || width <= 0 || height <= 0 ||
                    active_render_target_ != 0 || !context_.device() || !context_.command_queue())
                    return false;
                const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
                if (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
                    resource_desc.Width != static_cast<UINT64>(width) ||
                    resource_desc.Height != static_cast<UINT>(height) ||
                    resource_desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
                    return false;

                if (context_.frame_open())
                {
                    flush_2d();
                    context_.present(vsync_enabled_);
                    release_retired_textures();
                }
                if (!context_.wait_idle())
                    return false;

                D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
                UINT row_count = 0;
                UINT64 row_size = 0;
                UINT64 readback_size = 0;
                context_.device()->GetCopyableFootprints(&resource_desc, 0, 1, 0,
                                                         &footprint, &row_count, &row_size, &readback_size);
                ID3D12Resource *readback = nullptr;
                const D3D12_RESOURCE_DESC readback_desc = buffer_description(readback_size);
                const D3D12_HEAP_PROPERTIES readback_heap = heap_properties(D3D12_HEAP_TYPE_READBACK);
                HRESULT result = context_.device()->CreateCommittedResource(
                    &readback_heap, D3D12_HEAP_FLAG_NONE, &readback_desc,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
                if (FAILED(result))
                    return false;

                ID3D12CommandAllocator *allocator = nullptr;
                ID3D12GraphicsCommandList *commands = nullptr;
                result = context_.device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                   IID_PPV_ARGS(&allocator));
                if (SUCCEEDED(result))
                    result = context_.device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                  allocator, nullptr, IID_PPV_ARGS(&commands));
                if (FAILED(result))
                {
                    if (commands)
                        commands->Release();
                    if (allocator)
                        allocator->Release();
                    readback->Release();
                    return false;
                }

                transition_resource(commands, resource, source_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource = resource;
                source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                source.SubresourceIndex = 0;
                D3D12_TEXTURE_COPY_LOCATION destination{};
                destination.pResource = readback;
                destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                destination.PlacedFootprint = footprint;
                commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
                transition_resource(commands, resource, D3D12_RESOURCE_STATE_COPY_SOURCE, source_state);
                result = commands->Close();
                if (SUCCEEDED(result))
                {
                    ID3D12CommandList *lists[] = {commands};
                    context_.command_queue()->ExecuteCommandLists(1, lists);
                    if (!context_.wait_idle())
                        result = E_FAIL;
                }
                commands->Release();
                allocator->Release();
                if (FAILED(result))
                {
                    readback->Release();
                    return false;
                }

                const D3D12_RANGE read_range{0, static_cast<SIZE_T>(readback_size)};
                std::uint8_t *mapped = nullptr;
                result = readback->Map(0, &read_range, reinterpret_cast<void **>(&mapped));
                if (FAILED(result))
                {
                    readback->Release();
                    return false;
                }
                for (UINT row = 0; row < row_count; ++row)
                {
                    std::memcpy(pixels + static_cast<std::size_t>(row) * width * 4,
                                mapped + footprint.Offset +
                                    static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                                static_cast<std::size_t>(row_size));
                }
                const D3D12_RANGE written_range{0, 0};
                readback->Unmap(0, &written_range);
                readback->Release();
                return true;
            }

            bool ensure_frame()
            {
                if (!context_.frame_open() && !context_.begin_frame())
                {
                    last_error_ = "Unable to begin the D3D12 frame.";
                    return false;
                }
                if (active_render_target_ != 0)
                    return bind_active_render_target();
                return true;
            }

            bool bind_active_render_target()
            {
                const auto target = render_targets_.find(active_render_target_);
                if (target == render_targets_.end())
                    return false;
                const auto texture = textures_.find(target->second.texture);
                if (texture == textures_.end())
                    return false;
                if (texture->second.state != D3D12_RESOURCE_STATE_RENDER_TARGET)
                {
                    transition_resource(context_.command_list(), texture->second.resource,
                                        texture->second.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
                    texture->second.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                }
                D3D12_CPU_DESCRIPTOR_HANDLE handle =
                    cpu_descriptor(render_target_heap_, render_target_descriptor_size_,
                                   target->second.descriptor);
                D3D12_CPU_DESCRIPTOR_HANDLE depth_handle{};
                const bool has_depth = target->second.depth != nullptr;
                if (has_depth)
                    depth_handle = cpu_descriptor(depth_stencil_heap_, depth_stencil_descriptor_size_,
                                                  target->second.depth_descriptor);
                context_.command_list()->OMSetRenderTargets(1, &handle, FALSE,
                                                            has_depth ? &depth_handle : nullptr);
                D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(target->second.width),
                                        static_cast<float>(target->second.height), 0.0f, 1.0f};
                D3D12_RECT scissor{0, 0, target->second.width, target->second.height};
                context_.command_list()->RSSetViewports(1, &viewport);
                context_.command_list()->RSSetScissorRects(1, &scissor);
                return true;
            }

            void suspend_active_render_target()
            {
                if (active_render_target_ == 0)
                    return;
                const auto target = render_targets_.find(active_render_target_);
                if (target == render_targets_.end())
                    return;
                const auto texture = textures_.find(target->second.texture);
                if (texture != textures_.end() &&
                    texture->second.state == D3D12_RESOURCE_STATE_RENDER_TARGET)
                {
                    transition_resource(context_.command_list(), texture->second.resource,
                                        D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    texture->second.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                }
            }

            bool ensure_descriptor_heaps()
            {
                if (srv_heap_ && sampler_heap_)
                    return true;
                D3D12_DESCRIPTOR_HEAP_DESC desc{};
                desc.NumDescriptors = descriptor_capacity;
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                HRESULT result = context_.device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srv_heap_));
                if (FAILED(result))
                    return false;
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                result = context_.device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&sampler_heap_));
                if (FAILED(result))
                {
                    srv_heap_->Release();
                    srv_heap_ = nullptr;
                    return false;
                }
                srv_descriptor_size_ =
                    context_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                sampler_descriptor_size_ =
                    context_.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                return true;
            }

            std::uint32_t acquire_descriptor()
            {
                if (!free_descriptors_.empty())
                {
                    const std::uint32_t descriptor = free_descriptors_.back();
                    free_descriptors_.pop_back();
                    return descriptor;
                }
                if (next_descriptor_ >= descriptor_capacity)
                    return descriptor_capacity;
                return next_descriptor_++;
            }

            void release_texture(Texture &texture)
            {
                if (texture.resource)
                    texture.resource->Release();
                free_descriptors_.push_back(texture.descriptor);
                texture = {};
            }

            void release_retired_textures()
            {
                for (auto &texture : retired_textures_)
                    release_texture(texture);
                retired_textures_.clear();
            }

            static D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor(ID3D12DescriptorHeap *heap,
                                                               std::uint32_t increment,
                                                               std::uint32_t index)
            {
                D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
                handle.ptr += static_cast<SIZE_T>(increment) * index;
                return handle;
            }

            static D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor(ID3D12DescriptorHeap *heap,
                                                               std::uint32_t increment,
                                                               std::uint32_t index)
            {
                D3D12_GPU_DESCRIPTOR_HANDLE handle = heap->GetGPUDescriptorHandleForHeapStart();
                handle.ptr += static_cast<UINT64>(increment) * index;
                return handle;
            }

            bool unsupported()
            {
                last_error_ = "This D3D12 renderer feature is not implemented yet.";
                return false;
            }

            SDL_Window *window_ = nullptr;
            D3D12Context context_;
            static constexpr UINT imgui_descriptor_count = 64;
            ID3D12DescriptorHeap *imgui_heap_ = nullptr;
            UINT imgui_descriptor_size_ = 0;
            std::vector<UINT> imgui_free_;
            bool imgui_ready_ = false;
            std::string last_error_;
            bool vsync_enabled_ = true;
            bool initialized_2d_ = false;
            bool shutdown_2d_pending_ = false;
            ID3D12DescriptorHeap *srv_heap_ = nullptr;
            ID3D12DescriptorHeap *sampler_heap_ = nullptr;
            ID3D12DescriptorHeap *render_target_heap_ = nullptr;
            ID3D12DescriptorHeap *depth_stencil_heap_ = nullptr;
            ID3D12RootSignature *root_signature_ = nullptr;
            std::array<ID3D12PipelineState *, 3> pipelines_{};
            ID3D12RootSignature *mesh_root_signature_ = nullptr;
            std::array<ID3D12PipelineState *, 3> mesh_pipelines_{};
            ID3D12Resource *vertex_buffer_ = nullptr;
            Vertex2D *mapped_vertices_ = nullptr;
            std::size_t vertex_capacity_ = 0;
            std::size_t vertex_offset_ = 0;
            std::uint64_t vertex_frame_serial_ = 0;
            std::uint32_t srv_descriptor_size_ = 0;
            std::uint32_t sampler_descriptor_size_ = 0;
            std::uint32_t render_target_descriptor_size_ = 0;
            std::uint32_t depth_stencil_descriptor_size_ = 0;
            std::uint32_t next_texture_ = 1;
            std::uint32_t next_descriptor_ = 0;
            std::uint32_t next_render_target_ = 1;
            std::uint32_t next_render_target_descriptor_ = 0;
            std::uint32_t next_depth_stencil_descriptor_ = 0;
            std::uint32_t next_mesh_ = 1;
            std::uint32_t white_texture_ = 0;
            std::uint32_t active_render_target_ = 0;
            std::array<float, 16> projection_{};
            std::vector<std::uint32_t> free_descriptors_;
            std::vector<std::uint32_t> free_render_target_descriptors_;
            std::vector<std::uint32_t> free_depth_stencil_descriptors_;
            std::unordered_map<std::uint32_t, Texture> textures_;
            std::unordered_map<std::uint32_t, RenderTarget> render_targets_;
            std::unordered_map<std::uint32_t, Mesh> meshes_;
            std::vector<Texture> retired_textures_;
            std::vector<Vertex2D> batch_vertices_;
            PrimitiveType batch_primitive_ = PrimitiveType::triangles;
            std::uint32_t batch_texture_ = 0;
        };
    }

    std::unique_ptr<Renderer> create_d3d12_renderer()
    {
        return std::make_unique<D3D12Renderer>();
    }
}

#endif // _WIN32
