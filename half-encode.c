long try_float16_encode(unsigned long long b64) {
  /* use 64-bit unsigned for internal work */
  /* return 32-bit signed to allow for -1 for error and 16-bits unsigned for success */
  unsigned long long s16  = b64 >> 48 & 0x8000ULL;
  unsigned long long mant = b64 & 0xfffffffffffffULL;
  unsigned long long exp  = b64 >> 52 & 0x7ffULL;
  if (exp == 0 && mant == 0)    /* f64 denorms are out of range */
    return (long)s16;           /* so handle 0.0 and -0.0 only */
  if (exp >= 999 && exp < 1009) { /* f16 denorm, exp16 = 0 */
    if (mant & ((1UL << (1051 - exp)) - 1))
      return -1;                /* bits lost in f16 denorm */
    return (long)(s16 + ((mant + 0x10000000000000ULL) >> (1051 - exp)));
  }
  if (mant & 0x3ffffffffffULL)   /* bits lost in f16 */
    return -1;
  if (exp >= 1009 && exp <= 1038) /* normalized f16 */
    return (long)(s16 + ((exp - 1008) << 10) + (mant >> 42));
  if (exp == 2047)              /* Inf, NaN */
    return (long)(s16 + 0x7c00 + (mant >> 42));
  return -1;
}
