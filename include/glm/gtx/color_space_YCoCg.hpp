/// @ref gtx_colour_space_YCoCg
/// @file glm/gtx/colour_space_YCoCg.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_colour_space_YCoCg GLM_GTX_colour_space_YCoCg
/// @ingroup gtx
///
/// Include <glm/gtx/colour_space_YCoCg.hpp> to use the features of this extension.
///
/// RGB to YCoCg conversions and operations

#pragma once

// Dependency:
#include "../glm.hpp"

#ifndef GLM_ENABLE_EXPERIMENTAL
#	error "GLM: GLM_GTX_colour_space_YCoCg is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it."
#elif GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_colour_space_YCoCg extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_colour_space_YCoCg
	/// @{

	/// Convert a colour from RGB colour space to YCoCg colour space.
	/// @see gtx_colour_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> rgb2YCoCg(
		vec<3, T, Q> const& rgbColour);

	/// Convert a colour from YCoCg colour space to RGB colour space.
	/// @see gtx_colour_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> YCoCg2rgb(
		vec<3, T, Q> const& YCoCgColour);

	/// Convert a colour from RGB colour space to YCoCgR colour space.
	/// @see "YCoCg-R: A Colour Space with RGB Reversibility and Low Dynamic Range"
	/// @see gtx_colour_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> rgb2YCoCgR(
		vec<3, T, Q> const& rgbColour);

	/// Convert a colour from YCoCgR colour space to RGB colour space.
	/// @see "YCoCg-R: A Colour Space with RGB Reversibility and Low Dynamic Range"
	/// @see gtx_colour_space_YCoCg
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> YCoCgR2rgb(
		vec<3, T, Q> const& YCoCgColour);

	/// @}
}//namespace glm

#include "colour_space_YCoCg.inl"
