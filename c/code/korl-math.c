#include "korl-math.h"
korl_internal inline u64 korl_math_kilobytes(u64 x)
{
    return 1024 * x;
}
korl_internal inline u64 korl_math_megabytes(u64 x)
{
    return 1024 * korl_math_kilobytes(x);
}
korl_internal inline uintptr_t korl_math_roundUpPowerOf2(
    uintptr_t value, uintptr_t powerOf2Multiple)
{
    /* derived from this source: https://stackoverflow.com/a/9194117 */
    // ensure multiple is non-zero
    korl_assert(powerOf2Multiple);
    // ensure multiple is a power of two
    korl_assert((powerOf2Multiple & (powerOf2Multiple - 1)) == 0);
    return (value + (powerOf2Multiple - 1)) & ~(powerOf2Multiple - 1);
}
