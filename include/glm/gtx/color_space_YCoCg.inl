/// @ref gtx_colour_space_YCoCg

namespace glm
{
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<3, T, Q> rgb2YCoCg
	(
		vec<3, T, Q> const& rgbColour
	)
	{
		vec<3, T, Q> result;
		result.x/*Y */ =   rgbColour.r / T(4) + rgbColour.g / T(2) + rgbColour.b / T(4);
		result.y/*Co*/ =   rgbColour.r / T(2) + rgbColour.g * T(0) - rgbColour.b / T(2);
		result.z/*Cg*/ = - rgbColour.r / T(4) + rgbColour.g / T(2) - rgbColour.b / T(4);
		return result;
	}

	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<3, T, Q> YCoCg2rgb
	(
		vec<3, T, Q> const& YCoCgColour
	)
	{
		vec<3, T, Q> result;
		result.r = YCoCgColour.x + YCoCgColour.y - YCoCgColour.z;
		result.g = YCoCgColour.x				   + YCoCgColour.z;
		result.b = YCoCgColour.x - YCoCgColour.y - YCoCgColour.z;
		return result;
	}

	template<typename T, qualifier Q, bool isInteger>
	class compute_YCoCgR {
	public:
		static GLM_FUNC_QUALIFIER vec<3, T, Q> rgb2YCoCgR
		(
			vec<3, T, Q> const& rgbColour
		)
		{
			vec<3, T, Q> result;
			result.x/*Y */ = rgbColour.g * static_cast<T>(0.5) + (rgbColour.r + rgbColour.b) * static_cast<T>(0.25);
			result.y/*Co*/ = rgbColour.r - rgbColour.b;
			result.z/*Cg*/ = rgbColour.g - (rgbColour.r + rgbColour.b) * static_cast<T>(0.5);
			return result;
		}

		static GLM_FUNC_QUALIFIER vec<3, T, Q> YCoCgR2rgb
		(
			vec<3, T, Q> const& YCoCgRColour
		)
		{
			vec<3, T, Q> result;
			T tmp = YCoCgRColour.x - (YCoCgRColour.z * static_cast<T>(0.5));
			result.g = YCoCgRColour.z + tmp;
			result.b = tmp - (YCoCgRColour.y * static_cast<T>(0.5));
			result.r = result.b + YCoCgRColour.y;
			return result;
		}
	};

	template<typename T, qualifier Q>
	class compute_YCoCgR<T, Q, true> {
	public:
		static GLM_FUNC_QUALIFIER vec<3, T, Q> rgb2YCoCgR
		(
			vec<3, T, Q> const& rgbColour
		)
		{
			vec<3, T, Q> result;
			result.y/*Co*/ = rgbColour.r - rgbColour.b;
			T tmp = rgbColour.b + (result.y >> 1);
			result.z/*Cg*/ = rgbColour.g - tmp;
			result.x/*Y */ = tmp + (result.z >> 1);
			return result;
		}

		static GLM_FUNC_QUALIFIER vec<3, T, Q> YCoCgR2rgb
		(
			vec<3, T, Q> const& YCoCgRColour
		)
		{
			vec<3, T, Q> result;
			T tmp = YCoCgRColour.x - (YCoCgRColour.z >> 1);
			result.g = YCoCgRColour.z + tmp;
			result.b = tmp - (YCoCgRColour.y >> 1);
			result.r = result.b + YCoCgRColour.y;
			return result;
		}
	};

	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<3, T, Q> rgb2YCoCgR
	(
		vec<3, T, Q> const& rgbColour
	)
	{
		return compute_YCoCgR<T, Q, std::numeric_limits<T>::is_integer>::rgb2YCoCgR(rgbColour);
	}

	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<3, T, Q> YCoCgR2rgb
	(
		vec<3, T, Q> const& YCoCgRColour
	)
	{
		return compute_YCoCgR<T, Q, std::numeric_limits<T>::is_integer>::YCoCgR2rgb(YCoCgRColour);
	}
}//namespace glm
