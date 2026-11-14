/** @file
 * @brief Implements thread-local error reporting for the library.
 */

#include "error.h"

#ifdef __ANDROID__
#include <SDL.h>
#endif

namespace sl
{
	namespace
	{
		thread_local std::string error;
	}

	const std::string &last_error()
	{
		return error;
	}

	void clear_error()
	{
		error.clear();
	}

	namespace detail
	{

		void set_error(const std::string &message)
		{
			error = message;
#ifdef __ANDROID__
			// stderr is not visible on Android, so surface engine errors in logcat.
			SDL_Log("simlib error: %s", message.c_str());
#endif
		}

	} // namespace detail
} // namespace sl