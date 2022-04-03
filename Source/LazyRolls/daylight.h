typedef int fixedfloat_t;
// Must be double the length of fixedfloat_t.
typedef long long int fixedfloat_double_t;

/* Fixed-point format: Q10.21
   Whole numbers: 0-1024
   Bit 31 indicates sign */
#define FRAC_PLACES 21
#define FORMAT (1 << FRAC_PLACES)
#define FRAC_MASK (FORMAT - 1)
#define FRAC(a) ((a) & FRAC_MASK)

#define FROMFLOAT(a) ((fixedfloat_t)(FORMAT * (a)))
#define TOFLOAT(a) (((float) (a)) / FORMAT)
#define FROMINT(a) ((fixedfloat_t)((a) << FRAC_PLACES))
#define TOINT(a) ((int)((a) >> FRAC_PLACES))
//#define IFADD(a, b) ((a) + (b)) // Just for illustration
//#define IFSUB(a, b) ((a) - (b)) // Just for illustration
#define IFMUL(a, b) ((fixedfloat_t)((((fixedfloat_double_t)(a)) * (b)) >> FRAC_PLACES))
#define IFDIV(a, b) ((fixedfloat_t)((((fixedfloat_double_t)(a)) << FRAC_PLACES) / (b)))
#define IFMOD(a, b) ((a) % (b))
#define IFFLOORF(a) ((a) & ~FRAC_MASK)
#define IFABS(a) ((a) < 0 ? -(a) : (a))

#define FPI FROMFLOAT(3.141592653589793)
#define TWOPI FROMFLOAT(2 * 3.141592653589793)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Order was changed and some shifting is done, to avoid overflows.
#define RADIANS(a) IFDIV(IFMUL((a), FPI >> 1), FROMINT(180 >> 1))
#define DEGREES(a) IFMUL(IFDIV((a), FPI >> 1), FROMINT(180 >> 1))

extern fixedfloat_t ZENITH;
// Must be double the length of fixedfloat_t.
typedef long long int fixedfloat_double_t;
int calculateSunriseSunset(int dayOfYear, fixedfloat_t lat, fixedfloat_t lng, int localOffset, int daylightSavings, int sunrise);
int computeDayOfYear(int year, int month, int day);

#define ITERATIONS 20

fixedfloat_t ifsin(fixedfloat_t a);
fixedfloat_t ifcos(fixedfloat_t a);
void ifsincos(fixedfloat_t a, fixedfloat_t *sinout, fixedfloat_t *cosout);
fixedfloat_t iftan(fixedfloat_t a);
fixedfloat_t ifacos(fixedfloat_t a);
fixedfloat_t ifasin(fixedfloat_t a);
#define ifatan(a) ifatan2(FROMINT(1), a)
fixedfloat_t ifatan2(fixedfloat_t x, fixedfloat_t y);

#define sin(x) ifsin(x)
#define cos(x) ifcos(x)
#define sincos(x, y, z) ifsincos(x, y, z)
#define tan(x) iftan(x)
#define acos(x) ifacos(x)
#define asin(x) ifasin(x)
#define atan(x) ifatan(x)
#define atan2(x, y) ifatan2(x, y)

