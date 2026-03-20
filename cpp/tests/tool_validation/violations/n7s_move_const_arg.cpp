// N7 Space coding standard: std::move shall not be applied to a const
// argument, as it has no effect.
// Expected diagnostic: performance-move-const-arg

#include <string>
#include <utility>

std::string tvs_move_const_arg(const std::string& s) {
  return std::move(s); // std::move on const argument — target violation
}
