#include "korl-math.h"
korl_internal inline u64 korl_math_kilobytes(u64 x)
{
	return 1024 * x;
}
korl_internal inline u64 korl_math_megabytes(u64 x)
{
	return 1024 * korl_math_kilobytes(x);
}