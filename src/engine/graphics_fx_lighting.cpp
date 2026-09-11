/** @file
 * @brief Implements the tiled LightingPass effect (2D lights, shadows, and shadow casters).
 */
#include "graphics_fx.h"
#include "graphics_fx_internal.h"

#include "display.h"
#include "draw.h"
#include "gl2d.h"
#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace sl
{
	using detail::load_glsl_shader;
	using detail::submit_fullscreen_quad;

	namespace
	{
		struct Point
		{
			float x;
			float y;
		};

		struct GpuLight
		{
			float positionRadius[4];
			float colourIntensity[4];
			float shadowSoftness[4];
		};

		Point project_from_light(Point point, const Light &light, float distance)
		{
			const float dx = point.x - light.x;
			const float dy = point.y - light.y;
			const float length = std::sqrt(dx * dx + dy * dy);
			if (length <= 0.0001f)
			{
				return point;
			}
			return {point.x + dx / length * distance, point.y + dy / length * distance};
		}

		void fill_shadow_triangle(Bitmap *mask, Point first, Point second, Point third)
		{
			const float area = (second.x - first.x) * (third.y - first.y) -
							   (second.y - first.y) * (third.x - first.x);
			if (std::abs(area) <= 0.0001f)
			{
				return;
			}

			const int minimumX = std::max(0, static_cast<int>(std::floor(std::min({first.x, second.x, third.x}))));
			const int maximumX = std::min(mask->width - 1, static_cast<int>(std::ceil(std::max({first.x, second.x, third.x}))));
			const int minimumY = std::max(0, static_cast<int>(std::floor(std::min({first.y, second.y, third.y}))));
			const int maximumY = std::min(mask->height - 1, static_cast<int>(std::ceil(std::max({first.y, second.y, third.y}))));
			if (minimumX > maximumX || minimumY > maximumY)
			{
				return;
			}

			for (int y = minimumY; y <= maximumY; ++y)
			{
				for (int x = minimumX; x <= maximumX; ++x)
				{
					const Point sample{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
					const float edgeA = (second.x - first.x) * (sample.y - first.y) -
										(second.y - first.y) * (sample.x - first.x);
					const float edgeB = (third.x - second.x) * (sample.y - second.y) -
										(third.y - second.y) * (sample.x - second.x);
					const float edgeC = (first.x - third.x) * (sample.y - third.y) -
										(first.y - third.y) * (sample.x - third.x);
					if ((edgeA >= 0.0f && edgeB >= 0.0f && edgeC >= 0.0f) ||
						(edgeA <= 0.0f && edgeB <= 0.0f && edgeC <= 0.0f))
					{
						const std::size_t offset = (static_cast<std::size_t>(y) * mask->width + x) * 4;
						mask->pixels[offset] = 0;
						mask->pixels[offset + 1] = 0;
						mask->pixels[offset + 2] = 0;
						mask->pixels[offset + 3] = 255;
					}
				}
			}
		}

		void draw_shadow_edge(Bitmap *mask, Point first, Point second, const Light &light, float projectionDistance)
		{
			const Point edge{second.x - first.x, second.y - first.y};
			const Point normal{edge.y, -edge.x};
			const Point midpoint{(first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f};
			const float facing = (light.x - midpoint.x) * normal.x + (light.y - midpoint.y) * normal.y;
			if (facing <= 0.0f)
			{
				return;
			}

			const Point firstFar = project_from_light(first, light, projectionDistance);
			const Point secondFar = project_from_light(second, light, projectionDistance);
			fill_shadow_triangle(mask, first, second, secondFar);
			fill_shadow_triangle(mask, first, secondFar, firstFar);
		}

		void draw_shadow_caster(Bitmap *mask, const ShadowCaster &caster, const Light &light)
		{
			if (caster.vertices.size() < 2)
			{
				return;
			}
			const float projectionDistance = static_cast<float>(std::max(mask->width, mask->height)) * 4.0f;
			for (std::size_t index = 0; index < caster.vertices.size(); ++index)
			{
				const ShadowPoint &first = caster.vertices[index];
				const ShadowPoint &second = caster.vertices[(index + 1) % caster.vertices.size()];
				draw_shadow_edge(mask, {first.x, first.y}, {second.x, second.y}, light, projectionDistance);
			}
		}

	} // namespace

	ShadowCaster make_rectangle_shadow_caster(float left, float top, float right, float bottom)
	{
		return ShadowCaster{{
			{left, top},
			{right, top},
			{right, bottom},
			{left, bottom},
		}};
	}

	LightingPass::~LightingPass()
	{
		shutdown();
	}

	LightingPass::LightingPass(LightingPass &&other) noexcept
		: shader_(std::move(other.shader_)), cullShader_(std::move(other.cullShader_)), ambient_(other.ambient_),
		  shadowMasks_(std::exchange(other.shadowMasks_, {})),
		  lightBuffer_(std::exchange(other.lightBuffer_, 0)),
		  tileCountsBuffer_(std::exchange(other.tileCountsBuffer_, 0)),
		  tileIndicesBuffer_(std::exchange(other.tileIndicesBuffer_, 0)),
		  tileCountX_(other.tileCountX_), tileCountY_(other.tileCountY_)
	{
	}

	LightingPass &LightingPass::operator=(LightingPass &&other) noexcept
	{
		if (this != &other)
		{
			shutdown();
			shader_ = std::move(other.shader_);
			cullShader_ = std::move(other.cullShader_);
			ambient_ = other.ambient_;
			shadowMasks_ = std::exchange(other.shadowMasks_, {});
			lightBuffer_ = std::exchange(other.lightBuffer_, 0);
			tileCountsBuffer_ = std::exchange(other.tileCountsBuffer_, 0);
			tileIndicesBuffer_ = std::exchange(other.tileIndicesBuffer_, 0);
			tileCountX_ = other.tileCountX_;
			tileCountY_ = other.tileCountY_;
		}
		return *this;
	}

	bool LightingPass::initialise()
	{
		if (is_valid())
		{
			return true;
		}
		shadowMasks_.resize(max_shadow_lights, nullptr);
		if (!shader_.load(load_glsl_shader("fullscreen.vert"), load_glsl_shader("lighting.frag"), "lighting") ||
			!cullShader_.load_compute(load_glsl_shader("light_cull.comp"), "light-cull"))
		{
			return false;
		}
		if (!detail::active_renderer()->create_storage_buffer(sizeof(GpuLight), lightBuffer_) ||
			!detail::active_renderer()->create_storage_buffer(sizeof(std::uint32_t), tileCountsBuffer_) ||
			!detail::active_renderer()->create_storage_buffer(sizeof(std::uint32_t), tileIndicesBuffer_))
			return false;
		return true;
	}

	void LightingPass::shutdown()
	{
		shader_.reset();
		for (Bitmap *&shadowMask : shadowMasks_)
		{
			destroy_bitmap(shadowMask);
			shadowMask = nullptr;
		}
		if (lightBuffer_ != 0)
		{
			detail::active_renderer()->destroy_storage_buffer(lightBuffer_);
			lightBuffer_ = 0;
		}
		if (tileCountsBuffer_ != 0)
		{
			detail::active_renderer()->destroy_storage_buffer(tileCountsBuffer_);
			tileCountsBuffer_ = 0;
		}
		if (tileIndicesBuffer_ != 0)
		{
			detail::active_renderer()->destroy_storage_buffer(tileIndicesBuffer_);
			tileIndicesBuffer_ = 0;
		}
	}

	bool LightingPass::is_valid() const
	{
		return shader_.is_valid() && cullShader_.is_valid() && lightBuffer_ != 0 &&
			   tileCountsBuffer_ != 0 && tileIndicesBuffer_ != 0;
	}

	const std::string &LightingPass::error() const
	{
		return shader_.error();
	}

	void LightingPass::set_ambient(float ambient)
	{
		ambient_ = std::clamp(ambient, 0.0f, 1.0f);
	}

	bool LightingPass::ensure_shadow_mask(std::size_t index, int width, int height) const
	{
		if (index >= shadowMasks_.size())
		{
			return false;
		}
		Bitmap *&shadowMask = shadowMasks_[index];
		if (shadowMask && shadowMask->width == width && shadowMask->height == height)
		{
			return true;
		}
		destroy_bitmap(shadowMask);
		shadowMask = create_bitmap(width, height);
		return shadowMask != nullptr;
	}

	void LightingPass::apply(Bitmap *source, const Light &light, int x, int y, int width, int height,
							 const std::vector<ShadowCaster> &casters) const
	{
		apply(source, std::vector<Light>{light}, x, y, width, height, casters);
	}

	void LightingPass::apply(Bitmap *source, const std::vector<Light> &lights, int x, int y, int width, int height,
							 const std::vector<ShadowCaster> &casters) const
	{
		const bool flipVertical = graphics_backend() == GraphicsBackend::opengl;
		if (!source || !is_valid() || !upload_bitmap(source))
		{
			return;
		}

		if (width <= 0)
		{
			width = screen_width();
		}
		if (height <= 0)
		{
			height = screen_height();
		}
		if (width <= 0 || height <= 0)
		{
			return;
		}
		const bool sourceFlipVertical = graphics_backend() == GraphicsBackend::vulkan ? false : flipVertical;
		const std::size_t lightCount = lights.size();
		const std::size_t shadowLightCount = casters.empty()
												 ? 0
												 : std::min<std::size_t>(lightCount, max_shadow_lights);
		for (std::size_t index = 0; index < shadowLightCount; ++index)
		{
			if (!ensure_shadow_mask(index, source->width, source->height))
			{
				return;
			}
			clear_to_colour(shadowMasks_[index], {255, 255, 255});
			for (const ShadowCaster &caster : casters)
			{
				draw_shadow_caster(shadowMasks_[index], caster, lights[index]);
			}
			if (!upload_bitmap(shadowMasks_[index]))
			{
				return;
			}
		}

		std::vector<GpuLight> gpuLights(lightCount);
		for (std::size_t index = 0; index < lightCount; ++index)
		{
			const Light &light = lights[index];
			GpuLight &gpuLight = gpuLights[index];
			gpuLight.positionRadius[0] = light.x;
			gpuLight.positionRadius[1] = light.y;
			gpuLight.positionRadius[2] = std::max(light.radius, 0.0f);
			gpuLight.positionRadius[3] = 0.0f;
			gpuLight.colourIntensity[0] = static_cast<float>(light.colour.red) / 255.0f;
			gpuLight.colourIntensity[1] = static_cast<float>(light.colour.green) / 255.0f;
			gpuLight.colourIntensity[2] = static_cast<float>(light.colour.blue) / 255.0f;
			gpuLight.colourIntensity[3] = std::max(light.intensity, 0.0f);
			gpuLight.shadowSoftness[0] = std::max(light.shadow_softness, 0.0f);
			gpuLight.shadowSoftness[1] = 0.0f;
			gpuLight.shadowSoftness[2] = 0.0f;
			gpuLight.shadowSoftness[3] = 0.0f;
		}
		detail::Renderer *renderer = detail::active_renderer();
		renderer->upload_storage_buffer(lightBuffer_, gpuLights.size() * sizeof(GpuLight), gpuLights.data(), false);
		renderer->bind_storage_buffer(2, lightBuffer_);

		const int tileCountX = (source->width + tile_size - 1) / tile_size;
		const int tileCountY = (source->height + tile_size - 1) / tile_size;
		const bool tileBuffersNeedResize = tileCountX != tileCountX_ || tileCountY != tileCountY_;
		tileCountX_ = tileCountX;
		tileCountY_ = tileCountY;
		const std::size_t tileCount = static_cast<std::size_t>(tileCountX_) * tileCountY_;
		if (tileBuffersNeedResize)
		{
			renderer->upload_storage_buffer(tileCountsBuffer_, tileCount * sizeof(std::uint32_t), nullptr, false);
		}
		renderer->bind_storage_buffer(3, tileCountsBuffer_);
		if (tileBuffersNeedResize)
		{
			renderer->upload_storage_buffer(tileIndicesBuffer_, tileCount * max_lights_per_tile * sizeof(std::uint32_t), nullptr, false);
		}
		renderer->bind_storage_buffer(4, tileIndicesBuffer_);
		cullShader_.set_uniform("screenSize", source->width, source->height);
		cullShader_.set_uniform("tileCount", tileCountX_, tileCountY_);
		cullShader_.set_uniform("lightCount", static_cast<int>(lightCount));
		cullShader_.dispatch_compute(static_cast<unsigned int>(tileCountX_), static_cast<unsigned int>(tileCountY_), 1);
		renderer->storage_barrier();

		float projection[16];
		shader_.set_uniform("source", 0);
		shader_.set_uniform("lightCount", static_cast<int>(lightCount));
		shader_.set_uniform("shadowLightCount", static_cast<int>(shadowLightCount));
		shader_.set_uniform("tileCount", tileCountX_, tileCountY_);
		for (std::size_t index = 0; index < shadowLightCount; ++index)
		{
			const std::string suffix = "[" + std::to_string(index) + "]";
			renderer->bind_texture_unit(static_cast<unsigned int>(index + 1), shadowMasks_[index]->gpu_texture);
			shader_.set_uniform(("shadowMasks" + suffix).c_str(), static_cast<int>(index + 1));
		}
		shader_.set_uniform("ambient", ambient_);
		shader_.set_uniform("flipVertical", sourceFlipVertical ? 1 : 0);
		detail::gl2d_ortho_matrix(screen_width(), screen_height(), projection);
		shader_.set_uniform_mat4("uProjection", projection);
		submit_fullscreen_quad(x, y, width, height, source->gpu_texture, sourceFlipVertical);
		Shader::stop();
	}

} // namespace sl
