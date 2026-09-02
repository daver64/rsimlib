/// @ref gtx_colour_encoding
/// @file glm/gtx/colour_encoding.hpp
///
/// @see core (dependence)
/// @see gtx_colour_encoding (dependence)
///
/// @defgroup gtx_colour_encoding GLM_GTX_colour_encoding
/// @ingroup gtx
///
/// Include <glm/gtx/colour_encoding.hpp> to use the features of this extension.
///
/// @brief Allow to perform bit operations on integer values

#pragma once

// Dependencies
#include "../detail/setup.hpp"
#include "../detail/qualifier.hpp"
#include "../vec3.hpp"
#include <limits>

#ifndef GLM_ENABLE_EXPERIMENTAL
#	error "GLM: GLM_GTC_colour_encoding is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it."
#elif GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTC_colour_encoding extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_colour_encoding
	/// @{

	/// Convert a linear sRGB colour to D65 YUV.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> convertLinearSRGBToD65XYZ(vec<3, T, Q> const& ColourLinearSRGB);

	/// Convert a linear sRGB colour to D50 YUV.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> convertLinearSRGBToD50XYZ(vec<3, T, Q> const& ColourLinearSRGB);

	/// Convert a D65 YUV colour to linear sRGB.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> convertD65XYZToLinearSRGB(vec<3, T, Q> const& ColourD65XYZ);

	/// Convert a D65 YUV colour to D50 YUV.
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> convertD65XYZToD50XYZ(vec<3, T, Q> const& ColourD65XYZ);

	/// @}
} //namespace glm

#include "colour_encoding.inl"
