/// @ref gtx_colour_space
/// @file glm/gtx/colour_space.hpp
///
/// @see core (dependence)
///
/// @defgroup gtx_colour_space GLM_GTX_colour_space
/// @ingroup gtx
///
/// Include <glm/gtx/colour_space.hpp> to use the features of this extension.
///
/// Related to RGB to HSV conversions and operations.

#pragma once

// Dependency:
#include "../glm.hpp"

#ifndef GLM_ENABLE_EXPERIMENTAL
#	error "GLM: GLM_GTX_colour_space is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it."
#elif GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#	pragma message("GLM: GLM_GTX_colour_space extension included")
#endif

namespace glm
{
	/// @addtogroup gtx_colour_space
	/// @{

	/// Converts a colour from HSV colour space to its colour in RGB colour space.
	/// @see gtx_colour_space
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> rgbColour(
		vec<3, T, Q> const& hsvValue);

	/// Converts a colour from RGB colour space to its colour in HSV colour space.
	/// @see gtx_colour_space
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> hsvColour(
		vec<3, T, Q> const& rgbValue);

	/// Build a saturation matrix.
	/// @see gtx_colour_space
	template<typename T>
	GLM_FUNC_DECL mat<4, 4, T, defaultp> saturation(
		T const s);

	/// Modify the saturation of a colour.
	/// @see gtx_colour_space
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<3, T, Q> saturation(
		T const s,
		vec<3, T, Q> const& colour);

	/// Modify the saturation of a colour.
	/// @see gtx_colour_space
	template<typename T, qualifier Q>
	GLM_FUNC_DECL vec<4, T, Q> saturation(
		T const s,
		vec<4, T, Q> const& colour);

	/// Compute colour luminosity associating ratios (0.33, 0.59, 0.11) to RGB canals.
	/// @see gtx_colour_space
	template<typename T, qualifier Q>
	GLM_FUNC_DECL T luminosity(
		vec<3, T, Q> const& colour);

	/// @}
}//namespace glm

#include "colour_space.inl"
