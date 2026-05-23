int try_float16_encode(unsigned long b64) {
  unsigned long s16 = b64 >> 48 & 0x8000UL;
  unsigned long mant = b64 & 0xfffffffffffffUL;
  unsigned long exp = b64 >> 52 & 0x7ffUL;
  if (exp == 0 && mant == 0)    /* f64 denorms are out of range */
    return s16;                 /* so handle 0.0 and -0.0 only */
  if (exp >= 999 && exp < 1009) { /* f16 denorm, exp16 = 0 */
    if (mant & ((1UL << (1051 - exp)) - 1))
      return -1;                /* bits lost in f16 denorm */
    return s16 + ((mant + 0x10000000000000UL) >> (1051 - exp));
  }
  if (mant & 0x3ffffffffffUL)   /* bits lost in f16 */
    return -1;
  if (exp >= 1009 && exp <= 1038) /* normalized f16 */
    return s16 + ((exp - 1008) << 10) + (mant >> 42);
  if (exp == 2047)              /* Inf, NaN */
    return s16 + 0x7c00 + (mant >> 42);
  return -1;
}
