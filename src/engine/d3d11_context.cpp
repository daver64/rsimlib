/** @file
 * @brief D3D11 device/swap-chain boilerplate for the Windows-only backend.
 *
 * This is intentionally minimal: it establishes the device, swap chain, and
 * back-buffer render target view so the window can be cleared and presented.
 * Texture, shader, and 2D-submission support are not implemented yet; see
 * D3D11Renderer in renderer.cpp for the stubbed Renderer interface methods.
 */
#ifdef _WIN32

#include "d3d11_context.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <d3d11.h>
#include <dxgi.h>

namespace sl::detail
{
    D3D11Context::~D3D11Context()
    {
        shutdown();
    }

    bool D3D11Context::initialise(SDL_Window *window, std::string &error)
    {
        if (!window)
        {
            error = "D3D11 context requires a valid window.";
            return false;
        }
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(window, &info))
        {
            error = SDL_GetError();
            return false;
        }
        const HWND hwnd = info.info.win.window;

        int windowWidth = 0, windowHeight = 0;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        width_ = windowWidth;
        height_ = windowHeight;

        DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
        swap_chain_desc.BufferDesc.Width = static_cast<UINT>(windowWidth);
        swap_chain_desc.BufferDesc.Height = static_cast<UINT>(windowHeight);
        swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
        swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.OutputWindow = hwnd;
        swap_chain_desc.Windowed = TRUE;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT device_flags = 0;
#ifndef NDEBUG
        device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        const D3D_FEATURE_LEVEL requested_levels[] = {D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL obtained_level{};
        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags,
            requested_levels, 1u, D3D11_SDK_VERSION,
            &swap_chain_desc, &swap_chain_, &device_, &obtained_level, &context_);
        if (FAILED(hr))
        {
            error = "Unable to create the D3D11 device and swap chain.";
            return false;
        }
        if (create_backbuffer_view(error))
        {
            return true;
        }
        shutdown();
        return false;
    }

    bool D3D11Context::create_backbuffer_view(std::string &error)
    {
        ID3D11Texture2D *back_buffer = nullptr;
        HRESULT hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back_buffer));
        if (FAILED(hr) || !back_buffer)
        {
            error = "Unable to retrieve the D3D11 swap-chain back buffer.";
            return false;
        }
        hr = device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_view_);
        back_buffer->Release();
        if (FAILED(hr))
        {
            error = "Unable to create the D3D11 back-buffer render target view.";
            return false;
        }
        context_->OMSetRenderTargets(1, &render_target_view_, nullptr);
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<FLOAT>(width_);
        viewport.Height = static_cast<FLOAT>(height_);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);
        return true;
    }

    void D3D11Context::release_backbuffer_view()
    {
        if (render_target_view_)
        {
            render_target_view_->Release();
            render_target_view_ = nullptr;
        }
    }

    bool D3D11Context::resize(int width, int height, std::string &error)
    {
        if (!swap_chain_ || width <= 0 || height <= 0)
        {
            return false;
        }
        width_ = width;
        height_ = height;
        release_backbuffer_view();
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        const HRESULT hr = swap_chain_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                                      DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            error = "Unable to resize the D3D11 swap chain.";
            return false;
        }
        return create_backbuffer_view(error);
    }

    void D3D11Context::present(bool vsync)
    {
        if (swap_chain_)
        {
            swap_chain_->Present(vsync ? 1 : 0, 0);
        }
    }

    void D3D11Context::clear(float red, float green, float blue, float alpha)
    {
        if (!context_ || !render_target_view_)
        {
            return;
        }
        const float color[4] = {red, green, blue, alpha};
        context_->ClearRenderTargetView(render_target_view_, color);
    }

    void D3D11Context::set_render_target(ID3D11RenderTargetView *render_target, int width, int height)
    {
        if (!context_ || !render_target || width <= 0 || height <= 0)
        {
            return;
        }
        context_->OMSetRenderTargets(1, &render_target, nullptr);
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<FLOAT>(width);
        viewport.Height = static_cast<FLOAT>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);
    }

    void D3D11Context::bind_backbuffer()
    {
        if (render_target_view_)
        {
            set_render_target(render_target_view_, width_, height_);
        }
    }

    bool D3D11Context::is_valid() const
    {
        return device_ != nullptr && context_ != nullptr && swap_chain_ != nullptr;
    }

    void D3D11Context::shutdown()
    {
        release_backbuffer_view();
        if (context_) { context_->ClearState(); context_->Release(); context_ = nullptr; }
        if (swap_chain_) { swap_chain_->Release(); swap_chain_ = nullptr; }
        if (device_) { device_->Release(); device_ = nullptr; }
        width_ = 0;
        height_ = 0;
    }
}

#endif // _WIN32
