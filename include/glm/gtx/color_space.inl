/// @ref gtx_colour_space

#include <glm/ext/scalar_relational.hpp>
#include <glm/ext/scalar_constants.hpp>

namespace glm
{
	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<3, T, Q> rgbColour(const vec<3, T, Q>& hsvColour)
	{
		vec<3, T, Q> hsv = hsvColour;
		vec<3, T, Q> rgbColour;

		if(equal(hsv.y, static_cast<T>(0), epsilon<T>()))
			// achromatic (grey)
			rgbColour = vec<3, T, Q>(hsv.z);
		else
		{
			T sector = floor(hsv.x * (T(1) / T(60)));
			T frac = (hsv.x * (T(1) / T(60))) - sector;
			// factorial part of h
			T o = hsv.z * (T(1) - hsv.y);
			T p = hsv.z * (T(1) - hsv.y * frac);
			T q = hsv.z * (T(1) - hsv.y * (T(1) - frac));

			switch(int(sector))
			{
			default:
			case 0:
				rgbColour.r = hsv.z;
				rgbColour.g = q;
				rgbColour.b = o;
				break;
			case 1:
				rgbColour.r = p;
				rgbColour.g = hsv.z;
				rgbColour.b = o;
				break;
			case 2:
				rgbColour.r = o;
				rgbColour.g = hsv.z;
				rgbColour.b = q;
				break;
			case 3:
				rgbColour.r = o;
				rgbColour.g = p;
				rgbColour.b = hsv.z;
				break;
			case 4:
				rgbColour.r = q;
				rgbColour.g = o;
				rgbColour.b = hsv.z;
				break;
			case 5:
				rgbColour.r = hsv.z;
				rgbColour.g = o;
				rgbColour.b = p;
				break;
			}
		}

		return rgbColour;
	}

	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<3, T, Q> hsvColour(const vec<3, T, Q>& rgbColour)
	{
		vec<3, T, Q> hsv = rgbColour;
		T Min   = min(min(rgbColour.r, rgbColour.g), rgbColour.b);
		T Max   = max(max(rgbColour.r, rgbColour.g), rgbColour.b);
		T Delta = Max - Min;

		hsv.z = Max;

		if(!equal(Max, static_cast<T>(0), epsilon<T>()))
		{
			hsv.y = Delta / hsv.z;
			T h = static_cast<T>(0);

			if(equal(rgbColour.r, Max, epsilon<T>()))
				// between yellow & magenta
				h = static_cast<T>(0) + T(60) * (rgbColour.g - rgbColour.b) / Delta;
			else if(equal(rgbColour.g, Max, epsilon<T>()))
				// between cyan & yellow
				h = static_cast<T>(120) + T(60) * (rgbColour.b - rgbColour.r) / Delta;
			else
				// between magenta & cyan
				h = static_cast<T>(240) + T(60) * (rgbColour.r - rgbColour.g) / Delta;

			if(h < T(0))
				hsv.x = h + T(360);
			else
				hsv.x = h;
		}
		else
		{
			// If r = g = b = 0 then s = 0, h is undefined
			hsv.y = static_cast<T>(0);
			hsv.x = static_cast<T>(0);
		}

		return hsv;
	}

	template<typename T>
	GLM_FUNC_QUALIFIER mat<4, 4, T, defaultp> saturation(T const s)
	{
		vec<3, T, defaultp> rgbw = vec<3, T, defaultp>(T(0.2126), T(0.7152), T(0.0722));

		vec<3, T, defaultp> const col((T(1) - s) * rgbw);

		mat<4, 4, T, defaultp> result(T(1));
		result[0][0] = col.x + s;
		result[0][1] = col.x;
		result[0][2] = col.x;
		result[1][0] = col.y;
		result[1][1] = col.y + s;
		result[1][2] = col.y;
		result[2][0] = col.z;
		result[2][1] = col.z;
		result[2][2] = col.z + s;

		return result;
	}

	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<3, T, Q> saturation(const T s, const vec<3, T, Q>& colour)
	{
		return vec<3, T, Q>(saturation(s) * vec<4, T, Q>(colour, T(0)));
	}

	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER vec<4, T, Q> saturation(const T s, const vec<4, T, Q>& colour)
	{
		return saturation(s) * colour;
	}

	template<typename T, qualifier Q>
	GLM_FUNC_QUALIFIER T luminosity(const vec<3, T, Q>& colour)
	{
		const vec<3, T, Q> tmp = vec<3, T, Q>(0.33, 0.59, 0.11);
		return dot(colour, tmp);
	}
}//namespace glm
