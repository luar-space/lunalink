// N7 Space coding standard: Large parameters should be passed by const
// reference, not by value.
// Expected diagnostic: performance-unnecessary-value-param

#include <string>

int tvs_unnecessary_value_param(std::string s) {  // Large param by value — target violation
    return static_cast<int>(s.size());
}
