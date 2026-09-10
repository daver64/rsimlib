#pragma once

#ifdef _WIN32

#include <SDL2/SDL_video.h>

#include <cstdint>
#include <string>

struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12Resource;
struct IDXGISwapChain3;

namespace sl::detail
{
    /** Owns the D3D12 device, command submission objects, swap chain, and back buffers. */
    class D3D12Context
    {
    public:
        D3D12Context() = default;
        ~D3D12Context();

        D3D12Context(const D3D12Context &) = delete;
        D3D12Context &operator=(const D3D12Context &) = delete;

        bool initialise(SDL_Window *window, std::string &error);
        void shutdown();
        bool resize(int width, int height, std::string &error);
        bool begin_frame();
        void present(bool vsync);
        void clear(float red, float green, float blue, float alpha);
        bool is_valid() const;

        ID3D12Device *device() const { return device_; }
        ID3D12GraphicsCommandList *command_list() const { return command_list_; }

    private:
        bool create_backbuffers(std::string &error);
        void release_backbuffers();
        bool wait_for_gpu();

        SDL_Window *window_ = nullptr;
        ID3D12Device *device_ = nullptr;
        ID3D12CommandQueue *command_queue_ = nullptr;
        ID3D12CommandAllocator *command_allocator_ = nullptr;
        ID3D12GraphicsCommandList *command_list_ = nullptr;
        IDXGISwapChain3 *swap_chain_ = nullptr;
        ID3D12DescriptorHeap *rtv_heap_ = nullptr;
        ID3D12Resource *back_buffers_[2] = {};
        ID3D12Fence *fence_ = nullptr;
        void *fence_event_ = nullptr;
        std::uint64_t fence_value_ = 0;
        std::uint32_t rtv_descriptor_size_ = 0;
        std::uint32_t frame_index_ = 0;
        int width_ = 0;
        int height_ = 0;
        bool frame_open_ = false;
    };
}

#endif // _WIN32
