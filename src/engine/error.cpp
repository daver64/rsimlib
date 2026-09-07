#include "error.h"

namespace simlib {
namespace {
thread_local std::string error;
}

const std::string& last_error() {
	return error;
}

void clear_error() {
	error.clear();
}

namespace detail {

void set_error(const std::string& message) {
	error = message;
}

} // namespace detail
} // namespace simlib