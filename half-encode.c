/* Constants based on IEEE 754; _LEN is in bits */

/* Half-precision */
#define HLF_MANT_LEN          10
#define HLF_EXP_LEN           5
#define HLF_MANT_BITS         0x3ffUL /* 10 bits */
#define HLF_SIGN_BIT          (0x01UL << (HLF_EXP_LEN + HLF_MANT_LEN))
#define HLF_NAN_INF_EXP_BITS  0x7c00
#define HLF_QUIET_NAN         (0x1UL << (HLF_MANT_LEN-1))

/* Single-precision */
#define SGL_MANT_LEN          23
#define SGL_EXP_LEN           8
#define SGL_MANT_BITS         0x7fffffUL /* 23 bits */
#define SGL_SIGN_BIT          (0x01UL << (SGL_EXP_LEN + SGL_MANT_LEN))
#define SGL_EXP_BITS          0xffUL /* 8 bits */
#define SGL_QUIET_NAN         (0x1UL << (SGL_MANT_LEN - 1))
#define SGL_ZERO_SUBNORM_EXP  0
#define SGL_NAN_INF_EXP       255
#define SGL_NAN_INF_EXP_BITS  (SGL_NAN_INF_EXP << SGL_MANT_LEN)
#define SGL_MANT_ADD_ONE      (0x01UL << SGL_MANT_LEN)

/* Double-precision */
#define DBL_MANT_LEN          52
#define DBL_EXP_LEN           11
#define DBL_MANT_BITS         0xfffffffffffffULL /* 52 bits */
#define DBL_EXP_BITS          0x7ffULL /* 11 bits */
#define DBL_QUIET_NAN         (0x1UL << (DBL_MANT_LEN-1))
#define DBL_ZERO_SUBNORM_EXP  0
#define DBL_NAN_INF_EXP       2047
#define DBL_MANT_ADD_ONE      (0x01ULL << (DBL_MANT_LEN))

/* Converting single to half _EXP are biased single exponents */
#define S2H_SIGN_SHIFT ((SGL_MANT_LEN + SGL_EXP_LEN) - (HLF_MANT_LEN + HLF_EXP_LEN))
#define S2H_BIAS_DIFF_EXP        (127 - 15)
#define S2H_SMALLEST_NORM_EXP    113
#define S2H_SMALLEST_SUBNORM_EXP 102
#define S2H_SUBNORM_SHIFT        126
#define S2H_LARGEST_NORM_EXP     142
#define S2H_LOST_BITS            0xfffUL /* 23 - 10 bits */

/* Converting double to single; _EXP are biased double exponents */
#define D2S_SIGN_SHIFT ((DBL_MANT_LEN + DBL_EXP_LEN) - (SGL_MANT_LEN + SGL_EXP_LEN))
#define D2H_BIAS_DIFF_EXP        (1023 - 127)
#define D2S_SMALLEST_NORM_EXP    898
#define D2S_SMALLEST_SUBNORM_EXP 874
#define D2S_SUBNORM_SHIFT        926
#define D2S_LARGEST_NORM_EXP     1150
#define D2S_LOST_BITS            0x1fffffffUL /* 52 - 23 bits */


long pref_plus_single_to_half(unsigned long single) {
  const unsigned long sign = single >> S2H_SIGN_SHIFT & HLF_SIGN_BIT;
  const unsigned long mant = single & SGL_MANT_BITS;
  const unsigned long exp  = single >> SGL_MANT_LEN & SGL_EXP_BITS;

  if (exp == SGL_ZERO_SUBNORM_EXP) {
    if(mant == 0) {
      return (long)sign; /* +/- 0.0 */
    } else {
      return -1; /* single subnormals are out of range for half */
    }
  } else if (exp >= S2H_SMALLEST_SUBNORM_EXP && exp < S2H_SMALLEST_NORM_EXP) {
    if (mant & ((1UL << (S2H_SUBNORM_SHIFT - exp)) - 1)) {
      return -1;   /* bits lost in conversion to half subnormal */
    } else {
      return (long)(sign + /* converts to half subnormal */
                   ((mant + SGL_MANT_ADD_ONE) >> (S2H_SUBNORM_SHIFT - exp)));
    }
  } else if (exp >= S2H_SMALLEST_NORM_EXP && exp <= S2H_LARGEST_NORM_EXP) {
    if (mant & S2H_LOST_BITS) {
      return -1; /* bits lost in conversion */
    } else {
      return (long)(sign + /* Converts to normal */
                   ((exp - S2H_BIAS_DIFF_EXP) << HLF_MANT_LEN) +
                   (mant >> (SGL_MANT_LEN - HLF_MANT_LEN)));
      }
  } else if (exp == SGL_NAN_INF_EXP) {
    if(mant == 0) {
      return (long)(sign + HLF_NAN_INF_EXP_BITS); /* +/- inifinity */
    } else if (mant == SGL_QUIET_NAN && sign == 0) {
       return HLF_QUIET_NAN + HLF_NAN_INF_EXP_BITS; /* trivial NaN */
    } else {
       return -1; /* non-trivial NaN */
    }
  }
  return -1;
}


long long pref_plus_double_to_single(unsigned long long b64) {
  const unsigned long long sign = b64 >> D2S_SIGN_SHIFT & SGL_SIGN_BIT;
  const unsigned long long mant = b64 & DBL_MANT_BITS;
  const unsigned long long exp  = b64 >> DBL_MANT_LEN & DBL_EXP_BITS;

  if (exp == DBL_ZERO_SUBNORM_EXP) {
    if(mant == 0) {
      return (long long)sign; /* +/- 0.0 */
    } else {
      return -1; /* double subnormals are out of range for single */
    }
  } else if (exp >= D2S_SMALLEST_SUBNORM_EXP && exp < D2S_SMALLEST_NORM_EXP) {
    if (mant & ((1UL << (D2S_SUBNORM_SHIFT - exp)) - 1)) {
      return -1; /* bits lost in conversion to half subnormal */
    } else {
      return (long long)(sign + /* converts to half subnormal */
                        ((mant + DBL_MANT_ADD_ONE) >> (D2S_SUBNORM_SHIFT - exp)));
     }
  } else if (exp >= D2S_SMALLEST_NORM_EXP && exp <= D2S_LARGEST_NORM_EXP) {
    if (mant & D2S_LOST_BITS) {
      return -1; /* bits lost in conversion */
    } else {
       return (long long)(sign + /* Converts to normal */
                          ((exp - D2H_BIAS_DIFF_EXP) << SGL_MANT_LEN) +
                          (mant >> (DBL_MANT_LEN - SGL_MANT_LEN)));
    }
   } else if (exp == DBL_NAN_INF_EXP) {
    if(mant == 0) {
       return (long long)(sign + SGL_NAN_INF_EXP_BITS); /* +/- inifinity */
    } else if (mant == DBL_QUIET_NAN && sign == 0) {
       return SGL_QUIET_NAN + SGL_NAN_INF_EXP_BITS; /* quiet NaN */
    } else {
       return -1; /* non-trivial NaN */
    }
  }
  return -1;
}
