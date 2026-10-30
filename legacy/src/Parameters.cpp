#include "Parameters.h"
#include <algorithm>
#include <cmath>
namespace liminal
{
const std::array<Spec, count> specs{{
#define SPEC(id, name, min, max, initial, discrete, group) {#id, name, min, max, initial, discrete, group},
    LIMINAL_PARAMETERS(SPEC)
#undef SPEC
}};
Settings defaults()
{
    Settings p{};
    for (std::size_t i = 0; i < count; ++i)
        p[i] = specs[i].initial;
    return p;
}
Settings sanitise(Settings p)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        auto &s = specs[i];
        p[i] = std::isfinite(p[i]) ? std::clamp(p[i], s.min, s.max) : s.initial;
        if (s.discrete)
            p[i] = std::round(p[i]);
    }
    return p;
}
} // namespace liminal
