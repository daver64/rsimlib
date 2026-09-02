/// @ref gtc_colour_space
/// @file glm/gtc/colour_space.hpp
///
/// @see core (dependence)
/// @see gtc_colour_space (dependence)
///
/// @defgroup gtc_colour_space GLM_GTC_colour_space
/// @ingroup gtc
///
/// Include <glm/gtc/colour_space.hpp> to use the features of this extension.
///
/// Allow to perform bit operations on integer values

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../exponential.hpp"
#include "../vec3.hpp"
#include "../vec4.hpp"
#include <limits>

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_colour_space extension included")
#endif

namespace glm
{
	/// @addtogroup gtc_colour_space
	/// @{

	/// Convert a linear colour to sRGB colour using a standard gamma correction.
	/// IEC 61966-2-1:1999 / Rec. 709 specification https://www.w3.org/Graphics/Colour/srgb
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> convertLinearToSRGB(vec<L, T, Q> const& ColourLinear);

	/// Convert a linear colour to sRGB colour using a custom gamma correction.
	/// IEC 61966-2-1:1999 / Rec. 709 specification https://www.w3.org/Graphics/Colour/srgb
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> convertLinearToSRGB(vec<L, T, Q> const& ColourLinear, T Gamma);

	/// Convert a sRGB colour to linear colour using a standard gamma correction.
	/// IEC 61966-2-1:1999 / Rec. 709 specification https://www.w3.org/Graphics/Colour/srgb
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> convertSRGBToLinear(vec<L, T, Q> const& ColourSRGB);

	/// Convert a sRGB colour to linear colour using a custom gamma correction.
	// IEC 61966-2-1:1999 / Rec. 709 specification https://www.w3.org/Graphics/Colour/srgb
	template<length_t L, typename T, qualifier Q>
	GLM_FUNC_DECL vec<L, T, Q> convertSRGBToLinear(vec<L, T, Q> const& ColourSRGB, T Gamma);

	/// @}
} //namespace glm

#include "colour_space.inl"
