// code from last answer on
// https://stackoverflow.com/questions/41888621/microcontroller-based-sunrise-set-algorithm-implementation
// by SP193

// =========================== fixedpoint

#include "daylight.h"
// ====================================== CORDIC

/* Define if a fixed value of N will be used. If so, then define KPROD_VALUE, which determines the K value for correction.
   The value should be selected from the kprod array, with the index being the value of N.
   If N is greater than KPROD_LENGTH, choose the last value. */
#define FIXED_N     1
#define KPROD_VALUE FROMFLOAT(0.60725293500924945172)

fixedfloat_t arccos_cordic(fixedfloat_t t, fixedfloat_t n);
fixedfloat_t arcsin_cordic (fixedfloat_t t, int n );
fixedfloat_t arctan_cordic (fixedfloat_t x, fixedfloat_t y, int n );
void cossin_cordic(fixedfloat_t beta, int n, fixedfloat_t *c, fixedfloat_t *s);

fixedfloat_t ifsin(fixedfloat_t a) {
    fixedfloat_t cosVal;
    fixedfloat_t sinVal;
    sincos(a, &sinVal, &cosVal);
    return sinVal;
}

fixedfloat_t ifcos(fixedfloat_t a) {
    fixedfloat_t cosVal;
    fixedfloat_t sinVal;
    sincos(a, &sinVal, &cosVal);
    return cosVal;
}

void ifsincos(fixedfloat_t a, fixedfloat_t *sinout, fixedfloat_t *cosout) {
    cossin_cordic(a, ITERATIONS, cosout, sinout);
}

fixedfloat_t iftan(fixedfloat_t a) {
    fixedfloat_t cosVal;
    fixedfloat_t sinVal;
    sincos(a, &sinVal, &cosVal);
    if (cosVal >= 0 && cosVal < FROMFLOAT( 0.001f)) cosVal = FROMFLOAT( 0.001f);
    if (cosVal <  0 && cosVal > FROMFLOAT(-0.001f)) cosVal = FROMFLOAT(-0.001f);
    return IFDIV(sinVal, cosVal);
}

fixedfloat_t ifacos(fixedfloat_t a) {
    return arccos_cordic(a, ITERATIONS);
}

fixedfloat_t ifasin(fixedfloat_t a) {
    return arcsin_cordic(a, ITERATIONS);
}

fixedfloat_t ifatan2(fixedfloat_t x, fixedfloat_t y) {
    return arctan_cordic(x, y, ITERATIONS);
}


static fixedfloat_t angle_shift(fixedfloat_t alpha, fixedfloat_t beta);

# define ANGLES_LENGTH 60

static const fixedfloat_t angles[ANGLES_LENGTH] = {
  FROMFLOAT(7.8539816339744830962E-01),
  FROMFLOAT(4.6364760900080611621E-01),
  FROMFLOAT(2.4497866312686415417E-01),
  FROMFLOAT(1.2435499454676143503E-01),
  FROMFLOAT(6.2418809995957348474E-02),
  FROMFLOAT(3.1239833430268276254E-02),
  FROMFLOAT(1.5623728620476830803E-02),
  FROMFLOAT(7.8123410601011112965E-03),
  FROMFLOAT(3.9062301319669718276E-03),
  FROMFLOAT(1.9531225164788186851E-03),
  FROMFLOAT(9.7656218955931943040E-04),
  FROMFLOAT(4.8828121119489827547E-04),
  FROMFLOAT(2.4414062014936176402E-04),
  FROMFLOAT(1.2207031189367020424E-04),
  FROMFLOAT(6.1035156174208775022E-05),
  FROMFLOAT(3.0517578115526096862E-05),
  FROMFLOAT(1.5258789061315762107E-05),
  FROMFLOAT(7.6293945311019702634E-06),
  FROMFLOAT(3.8146972656064962829E-06),
  FROMFLOAT(1.9073486328101870354E-06),
  FROMFLOAT(9.5367431640596087942E-07),
  FROMFLOAT(4.7683715820308885993E-07),
  FROMFLOAT(2.3841857910155798249E-07),
  FROMFLOAT(1.1920928955078068531E-07),
  FROMFLOAT(5.9604644775390554414E-08),
  FROMFLOAT(2.9802322387695303677E-08),
  FROMFLOAT(1.4901161193847655147E-08),
  FROMFLOAT(7.4505805969238279871E-09),
  FROMFLOAT(3.7252902984619140453E-09),
  FROMFLOAT(1.8626451492309570291E-09),
  FROMFLOAT(9.3132257461547851536E-10),
  FROMFLOAT(4.6566128730773925778E-10),
  FROMFLOAT(2.3283064365386962890E-10),
  FROMFLOAT(1.1641532182693481445E-10),
  FROMFLOAT(5.8207660913467407226E-11),
  FROMFLOAT(2.9103830456733703613E-11),
  FROMFLOAT(1.4551915228366851807E-11),
  FROMFLOAT(7.2759576141834259033E-12),
  FROMFLOAT(3.6379788070917129517E-12),
  FROMFLOAT(1.8189894035458564758E-12),
  FROMFLOAT(9.0949470177292823792E-13),
  FROMFLOAT(4.5474735088646411896E-13),
  FROMFLOAT(2.2737367544323205948E-13),
  FROMFLOAT(1.1368683772161602974E-13),
  FROMFLOAT(5.6843418860808014870E-14),
  FROMFLOAT(2.8421709430404007435E-14),
  FROMFLOAT(1.4210854715202003717E-14),
  FROMFLOAT(7.1054273576010018587E-15),
  FROMFLOAT(3.5527136788005009294E-15),
  FROMFLOAT(1.7763568394002504647E-15),
  FROMFLOAT(8.8817841970012523234E-16),
  FROMFLOAT(4.4408920985006261617E-16),
  FROMFLOAT(2.2204460492503130808E-16),
  FROMFLOAT(1.1102230246251565404E-16),
  FROMFLOAT(5.5511151231257827021E-17),
  FROMFLOAT(2.7755575615628913511E-17),
  FROMFLOAT(1.3877787807814456755E-17),
  FROMFLOAT(6.9388939039072283776E-18),
  FROMFLOAT(3.4694469519536141888E-18),
  FROMFLOAT(1.7347234759768070944E-18) };

#ifdef FIXED_N

#ifndef KPROD_VALUE
    #error KPROD_VALUE must be defined (refer to the KPROD table, for N iterations)
#endif

#else

# define KPROD_LENGTH 33

static const fixedfloat_t kprod[KPROD_LENGTH] = {
  FROMFLOAT(0.70710678118654752440), //1
  FROMFLOAT(0.63245553203367586640),
  FROMFLOAT(0.61357199107789634961),
  FROMFLOAT(0.60883391251775242102),
  FROMFLOAT(0.60764825625616820093),
  FROMFLOAT(0.60735177014129595905),
  FROMFLOAT(0.60727764409352599905),
  FROMFLOAT(0.60725911229889273006),
  FROMFLOAT(0.60725447933256232972),
  FROMFLOAT(0.60725332108987516334), //10
  FROMFLOAT(0.60725303152913433540),
  FROMFLOAT(0.60725295913894481363),
  FROMFLOAT(0.60725294104139716351),
  FROMFLOAT(0.60725293651701023413),
  FROMFLOAT(0.60725293538591350073),
  FROMFLOAT(0.60725293510313931731),
  FROMFLOAT(0.60725293503244577146),
  FROMFLOAT(0.60725293501477238499),
  FROMFLOAT(0.60725293501035403837),
  FROMFLOAT(0.60725293500924945172), //20
  FROMFLOAT(0.60725293500897330506),
  FROMFLOAT(0.60725293500890426839),
  FROMFLOAT(0.60725293500888700922),
  FROMFLOAT(0.60725293500888269443),
  FROMFLOAT(0.60725293500888161574),
  FROMFLOAT(0.60725293500888134606),
  FROMFLOAT(0.60725293500888127864),
  FROMFLOAT(0.60725293500888126179),
  FROMFLOAT(0.60725293500888125757),
  FROMFLOAT(0.60725293500888125652), //30
  FROMFLOAT(0.60725293500888125626),
  FROMFLOAT(0.60725293500888125619),
  FROMFLOAT(0.60725293500888125617) };

#endif

/******************************************************************************/

static fixedfloat_t angle_shift (fixedfloat_t alpha, fixedfloat_t beta )

/******************************************************************************/
/*
  Purpose:

    ANGLE_SHIFT shifts angle ALPHA to lie between BETA and BETA+2PI.

  Discussion:

    The input angle ALPHA is shifted by multiples of 2 * PI to lie
    between BETA and BETA+2*PI.

    The resulting angle GAMMA has all the same trigonometric function
    values as ALPHA.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    19 January 2012

  Author:

    John Burkardt

  Parameters:

    Input, double ALPHA, the angle to be shifted.

    Input, double BETA, defines the lower endpoint of
    the angle range.

    Output, double ANGLE_SHIFT, the shifted angle.
*/
{
    return (alpha < beta) ? beta - IFMOD( beta - alpha, TWOPI) + TWOPI : beta + IFMOD( alpha - beta, TWOPI);
}
/******************************************************************************/

fixedfloat_t arccos_cordic (fixedfloat_t t, fixedfloat_t n )

/******************************************************************************/
/*
  Purpose:

    ARCCOS_CORDIC returns the arccosine of an angle using the CORDIC method.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    19 January 2012

  Author:

    John Burkardt

  Reference:

    Jean-Michel Muller,
    Elementary Functions: Algorithms and Implementation,
    Second Edition,
    Birkhaeuser, 2006,
    ISBN13: 978-0-8176-4372-0,
    LC: QA331.M866.

  Parameters:

    Input, double T, the cosine of an angle.  -1 <= T <= 1.

    Input, int N, the number of iterations to take.
    A value of 10 is low.  Good accuracy is achieved with 20 or more
    iterations.

    Output, double ARCCOS_CORDIC, an angle whose cosine is T.

  Local Parameters:

    Local, double ANGLES(60) = arctan ( (1/2)^(0:59) );
*/
{
  fixedfloat_t angle;
  int i;
  int j;
  int shiftamount;
  int sigma;
  fixedfloat_t theta;
  fixedfloat_t x1;
  fixedfloat_t x2;
  fixedfloat_t y1;
  fixedfloat_t y2;

  theta = FROMINT(0);
  x1 = FROMINT(1);
  y1 = FROMINT(0);

  for ( j = 1; j <= n; j++ )
  {
    //sign_z2 = ( y1 < 0 ) ? -1 : 1;
    sigma = ( t <= x1 ) ? +1.0 : -1.0;
    if (((sigma>0) ^ (x1>0)) && y1 < 0) sigma = -sigma; // bug fixed by ACE 03.04.2022
    angle = (j <= ANGLES_LENGTH) ? angles[j - 1] : angle >> 1;

    shiftamount = j - 1; // Index of the root to compute, where 3 = square root.
    for ( i = 1; i <= 2; i++ )
    {
      // sigma * poweroftwo, where poweroftwo = 0.5, 0.25...
      x2 = x1 - ((sigma < 0 ? -y1 : y1) >> shiftamount);
      y2 = ((sigma < 0 ? -x1 : x1) >> shiftamount) + y1;

      x1 = x2;
      y1 = y2;
    }

    theta  = theta + ((sigma < 0 ? -angle : angle) << 1); // 2 * sigma * angle

    t = t + ((t >> shiftamount) >> shiftamount); // t + t * poweroftwo * poweroftwo
  }

  return theta;
}
/******************************************************************************/

fixedfloat_t arcsin_cordic (fixedfloat_t t, int n )

/******************************************************************************/
/*
  Purpose:

    ARCSIN_CORDIC returns the arcsine of an angle using the CORDIC method.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    19 January 2012

  Author:

    John Burkardt

  Reference:

    Jean-Michel Muller,
    Elementary Functions: Algorithms and Implementation,
    Second Edition,
    Birkhaeuser, 2006,
    ISBN13: 978-0-8176-4372-0,
    LC: QA331.M866.

  Parameters:

    Input, double T, the sine of an angle.  -1 <= T <= 1.

    Input, int N, the number of iterations to take.
    A value of 10 is low.  Good accuracy is achieved with 20 or more
    iterations.

    Output, double ARCSIN_CORDIC, an angle whose sine is T.

  Local Parameters:

    Local, double ANGLES(60) = arctan ( (1/2)^(0:59) );
*/
{
  fixedfloat_t angle;
  int i;
  int j;
  int shiftamount;
  int sigma;
  int sign_z1;
  fixedfloat_t theta;
  fixedfloat_t x1;
  fixedfloat_t x2;
  fixedfloat_t y1;
  fixedfloat_t y2;

  theta = FROMINT(0);
  x1 = FROMINT(1);
  y1 = FROMINT(0);

  for ( j = 1; j <= n; j++ )
  {
    sign_z1 = ( x1 < 0 ) ? -1 : 1;
    sigma = ( y1 <= t ) ? +sign_z1 : -sign_z1;
    angle = ( j <= ANGLES_LENGTH ) ? angles[j-1] : angle >> 1;

    shiftamount = j - 1; // Index of the root to compute, where 3 = square root.
    for ( i = 1; i <= 2; i++ )
    {
      // sigma * poweroftwo, where poweroftwo = 0.5, 0.25...
      x2 = x1 - ((sigma < 0 ? -y1 : y1) >> shiftamount);
      y2 = ((sigma < 0 ? -x1 : x1) >> shiftamount) + y1;

      x1 = x2;
      y1 = y2;
    }

    theta  = theta + ((sigma < 0 ? -angle : angle) << 1); // 2 * sigma * angle

    t = t + ((t >> shiftamount) >> shiftamount);
  }

  return theta;
}
/******************************************************************************/

fixedfloat_t arctan_cordic (fixedfloat_t x, fixedfloat_t y, int n )

/******************************************************************************/
/*
  Purpose:

    ARCTAN_CORDIC returns the arctangent of an angle using the CORDIC method.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    15 June 2007

  Author:

    John Burkardt

  Reference:

    Jean-Michel Muller,
    Elementary Functions: Algorithms and Implementation,
    Second Edition,
    Birkhaeuser, 2006,
    ISBN13: 978-0-8176-4372-0,
    LC: QA331.M866.

  Parameters:

    Input, double X, Y, define the tangent of an angle as Y/X.

    Input, int N, the number of iterations to take.
    A value of 10 is low.  Good accuracy is achieved with 20 or more
    iterations.

    Output, double ARCTAN_CORDIC, the angle whose tangent is Y/X.

  Local Parameters:

    Local, double ANGLES(60) = arctan ( (1/2)^(0:59) );
*/
{
  fixedfloat_t angle;
  int j;
  int shiftamount;
  int sigma;
  int sign_factor;
  fixedfloat_t theta;
  fixedfloat_t x1;
  fixedfloat_t x2;
  fixedfloat_t y1;
  fixedfloat_t y2;

  x1 = x;
  y1 = y;
/*
  Account for signs.
*/
  if ( x1 < 0 && y1 < 0 )
  {
    x1 = -x1;
    y1 = -y1;
  }

  if ( x1 < 0 )
  {
    x1 = -x1;
    sign_factor = -1;
  }
  else if ( y1 < 0 )
  {
    y1 = -y1;
    sign_factor = -1;
  }
  else
  {
    sign_factor = 1;
  }

  theta = FROMINT(0);

  for ( j = 1; j <= n; j++ )
  {
    sigma = (y1 <= 0) ? 1 : -1;
    angle = ( j <= ANGLES_LENGTH ) ? angles[j-1] : angle >> 1;

    shiftamount = j - 1; // Index of the root to compute, where 3 = square root.
    // sigma * poweroftwo, where poweroftwo = 0.5, 0.25...
    x2 = x1 - ((sigma < 0 ? -y1 : y1) >> shiftamount);
    y2 = ((sigma < 0 ? -x1 : x1) >> shiftamount) + y1;
    theta  = theta - (sigma < 0 ? -angle : angle);

    x1 = x2;
    y1 = y2;
  }

  theta = sign_factor < 0 ? -theta : theta;
if (theta >= FPI/2) theta = FPI/2 - 1;
if (theta <= -FPI/2) theta = -FPI/2 + 1;
  return theta;
}
/******************************************************************************/

void cossin_cordic (fixedfloat_t beta, int n, fixedfloat_t *c, fixedfloat_t *s )

/******************************************************************************/
/*
  Purpose:

    COSSIN_CORDIC returns the sine and cosine of an angle by the CORDIC method.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    19 January 2012

  Author:

    Based on MATLAB code in a Wikipedia article.
    C++ version by John Burkardt

  Parameters:

    Input, double BETA, the angle (in radians).

    Input, int N, the number of iterations to take.
    A value of 10 is low.  Good accuracy is achieved with 20 or more
    iterations.

    Output, double *C, *S, the cosine and sine of the angle.

  Local Parameters:

    Local, double ANGLES(60) = arctan ( (1/2)^(0:59) );

    Local, double KPROD(33).  KPROD(j) = product ( 0 <= i <= j ) K(i)
    where K(i) = 1 / sqrt ( 1 + (1/2)^(2i) ).
*/
{

  fixedfloat_t angle;
  fixedfloat_t c2;
  int j;
  int shiftamount;
  fixedfloat_t s2;
  int sigma;
  int sign_factor;
  fixedfloat_t theta;
/*
  Shift angle to interval [-pi,pi].
*/
  theta = angle_shift ( beta, -FPI);
/*
  Shift angle to interval [-pi/2,pi/2] and account for signs.
*/
  if ( theta < - (FPI >> 1) ) // theta < -0.5 * pi
  {
    theta = theta + FPI;
    sign_factor = -1;
  }
  else if ( (FPI >> 1) < theta ) // 0.5 * pi < theta
  {
    theta = theta - FPI;
    sign_factor = -1;
  }
  else
  {
    sign_factor = 1;
  }
/*
  Initialize loop variables:
*/
  *c = FROMINT(1);
  *s = FROMINT(0);

  angle = angles[0];
/*
  Iterations
*/
  for ( j = 1; j <= n; j++ )
  {
    sigma = (theta < 0) ? -1 : 1;

    shiftamount = j - 1; // Index of the root to compute, where 3 = square root.
    // sigma * poweroftwo, where poweroftwo = 0.5, 0.25...
    c2 = *c - ((sigma < 0 ? -*s : *s) >> shiftamount);
    s2 = ((sigma < 0 ? -*c : *c) >> shiftamount) + *s;

    *c = c2;
    *s = s2;
/*
  Update the remaining angle.
*/
    theta = theta - (sigma < 0 ? -angle : angle);
/*
  Update the angle from table, or eventually by just dividing by two.
*/
    angle = (ANGLES_LENGTH < j + 1) ? angle >> 1 : angles[j];
  }
/*
  Adjust length of output vector to be [cos(beta), sin(beta)]

  KPROD is essentially constant after a certain point, so if N is
  large, just take the last available value.
*/
 #ifndef FIXED_N
  if ( 0 < n )
  {
    *c = IFMUL(*c, kprod [ MIN ( n, KPROD_LENGTH ) - 1 ]);
    *s = IFMUL(*s, kprod [ MIN( n, KPROD_LENGTH ) - 1 ]);
  }
#else
  *c = IFMUL(*c, KPROD_VALUE);
  *s = IFMUL(*s, KPROD_VALUE);
#endif
/*
  Adjust for possible sign change because angle was originally
  not in quadrant 1 or 4.
*/
  if (sign_factor < 0) {
      *c = -*c;
      *s = -*s;
  }

// if (*s >= FROMINT(1)) *s = FROMINT(1);
// if (*c >= FROMINT(1)) *c = FROMINT(1);
// if (*s <= FROMINT(-1)) *s = FROMINT(-1)+1;
// if (*c <= FROMINT(-1)) *c = FROMINT(-1)+1;
  return;
}
/******************************************************************************/


// =========================== main

//#define ZENITH FROMFLOAT(-.83f)
fixedfloat_t ZENITH = FROMFLOAT(-.83f);

/*
    Computes the sunrise/sunset.
    Based on the algorithm described here:
    - https://stackoverflow.com/questions/7064531/sunrise-sunset-times-in-c
    - https://edwilliams.org/sunrise_sunset_algorithm.htm
    - https://www.edwilliams.org/sunrise_sunset_example.htm

    @param dayOfYear The day of year (1 - 365).
    @param lat The latitude, in decimal degrees.
    @param lng The longitude, in decimal degrees.
    @param localOffset The timezone offset in minutes.
    @param daylightSavings Whether DST is in effect. 1 = DST in effect, 0 = no DST.
    @param sunrise Whether to compute the sunrise. Otherwise, the sunset will be computed.
    @return The sunrise/sunset, in minutes since 00:00 of local time. */
int calculateSunriseSunset(int dayOfYear, fixedfloat_t lat, fixedfloat_t lng, int localOffset, int daylightSavings, int sunrise);

static fixedfloat_t sunLocalHourAngle(fixedfloat_t lat, fixedfloat_t sinDec, fixedfloat_t cosDec);

/**
  * Computes the day of year (where 1 = January 1st).
  * Explained here: https://astronomy.stackexchange.com/questions/2407/calculate-day-of-the-year-for-a-given-date
  * @param year The year
  * @param month The month within the year (1-12)
  * @param day The day of the month (1-31)
  */
int computeDayOfYear(int year, int month, int day) {
    //1. first calculate the day of the year
    int N1 = 275 * month / 9;
    int N2 = (month + 9) / 12;
    int N3 = (1 + ((year - 4 * (year / 4) + 2) / 3));
    int N = N1 - (N2 * N3) + day - 30;

    return N;
}

int calculateSunriseSunset(int dayOfYear, fixedfloat_t lat, fixedfloat_t lng, int localOffset, int daylightSavings, int sunrise) {
    fixedfloat_t sinDec;
    fixedfloat_t cosDec;
    fixedfloat_t cosH;
    fixedfloat_t H;
    fixedfloat_t time;
    int utc;
    fixedfloat_t t;
    fixedfloat_t M;
    fixedfloat_t L;
    fixedfloat_t RA;
    int Lquadrant;
    int RAquadrant;
    int r;
    fixedfloat_t temp;

    //2. convert the longitude to hour value and calculate an approximate time
    const fixedfloat_t lngHour = IFDIV(lng, FROMINT(15));
    t = FROMINT(dayOfYear) + IFDIV((FROMINT(sunrise ? 6 : 18) - lngHour), FROMINT(24));

    //3. calculate the Sun's mean anomaly  
    // from, https://physics.stackexchange.com/questions/80034: M = nt + M0, where n = 0.9856, M0 = 3.289
    //M = (0.9856f * t) - 3.289f;
    M = IFMUL(FROMFLOAT(0.9856f), t) - FROMFLOAT(3.289f);

    //4. calculate the Sun's true longitude
    //L = fmodf(M + (1.916f * sin((PI / 180)*M)) + (0.020f * sin(2 * (PI / 180) * M)) + 282.634f, 360.0f);
    // Break up this expression to avoid arithmetic overflows.
    temp = M + (IFMUL(FROMFLOAT(1.916f), sin(RADIANS(M)))) + IFMUL(FROMFLOAT(0.020f), sin(RADIANS(M) << 1));
    if (TOINT(temp) + 283 >= 360) {
        temp -= FROMINT(360); // Avoid overflow.
    }
    L = IFMOD(temp + FROMFLOAT(282.634f), FROMINT(360));

    //5a. calculate the Sun's right ascension     
    //RA = fmodf(180 / PI * atan(0.91764f * tan((PI / 180)*L)), 360.0f);
    RA = IFMOD(DEGREES(atan(IFMUL(FROMFLOAT(0.91764f), tan(RADIANS(L))))), FROMINT(360));

    //5b. right ascension value needs to be in the same quadrant as Lval
    Lquadrant  = TOINT(IFDIV(L, FROMINT(90))) * 90;
    RAquadrant = TOINT(IFDIV(RA, FROMINT(90))) * 90;
    RA = RA + FROMINT(Lquadrant - RAquadrant);

    //5c. right ascension value needs to be converted into hours   
    RA = IFDIV(RA, FROMINT(15));

    //6. calculate the Sun's declination
    //sinDec = 0.39782f * sin((PI / 180) * L);
    sinDec = IFMUL(FROMFLOAT(0.39782f), sin(RADIANS(L)));
    cosDec = cos(asin(sinDec));

    //7a. calculate the Sun's local hour angle
    //cosH = (sin((PI/180)*ZENITH) - (sinDec * sin((PI/180)*lat))) / (cosDec * cos((PI/180)*lat));
    cosH = sunLocalHourAngle(lat, sinDec, cosDec);

    /*
    if (cosH >  1) 
    the sun never rises on this location (on the specified date)
    if (cosH < -1)
    the sun never sets on this location (on the specified date)
    */

    //7b. finish calculating H and convert into hours
    H = sunrise ? FROMINT(360) - DEGREES(acos(cosH)) : DEGREES(acos(cosH));
    H = IFDIV(H, FROMINT(15));

    //8. calculate local mean time of rising/setting      
    //time = H + RA - (0.06571 * t) - 6.622;
    time = H + RA - IFMUL(FROMFLOAT(0.06571), t) - FROMFLOAT(6.622);

    //9. adjust back to UTC
    utc = (TOINT(time - lngHour) % 24 * 60) + TOINT(IFMUL(FRAC(time - lngHour), FROMINT(60)));

    //10. convert UTC value to local time zone of latitude/longitude
    r = utc + localOffset + daylightSavings * 60;

    return r < 0 ? r + (24 * 60) : (r % (24 * 60));
}

static fixedfloat_t sunLocalHourAngle(fixedfloat_t lat, fixedfloat_t sinDec, fixedfloat_t cosDec) {
    //cosH = (sin((PI/180)*ZENITH) - (sinDec * sin((PI/180)*lat))) / (cosDec * cos((PI/180)*lat));
    fixedfloat_t sinValue, cosValue;
    sincos(RADIANS(lat), &sinValue, &cosValue);
    return IFDIV(sin(RADIANS(ZENITH)) - IFMUL(sinDec, sinValue), IFMUL(cosDec, cosValue));
}

// int main(int argc, char *argv[]) {
//     time_t now;
//     float lat;
//     float lng;
//     int localOffset;
//     struct tm *tm;
//     int localSunriseT, localSunsetT;

//     if (argc != 4) {
//         printf("%s <latitude> <longitude> <timezone offset in minutes>\nExample: %s 1.3364926804464332 103.74412865673372 480\n", argv[0], argv[0]);
//         return EINVAL;
//     }

//     lat = (float) atof(argv[1]);
//     lng = (float) atof(argv[2]);
//     localOffset = atoi(argv[3]);

//     printf("Coordinates: %f,%f\nTimezone Offset: %d\n", lat, lng, localOffset);

//     now = time(NULL);
//     tm = localtime(&now);
//     // Get the time of day.
//     //const int dayOfYear = computeDayOfYear(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
//     const int dayOfYear = tm->tm_yday + 1; // If you have a source for the day in the year (1 to 365, inclusive).
//     const int dst = tm->tm_isdst;
//     // OR, set it manually:
//     /* const int dayOfYear = computeDayOfYear(2021, 1, 1);
//        const int dst = 0; */
//     localSunriseT =  calculateSunriseSunset(dayOfYear, FROMFLOAT(lat), FROMFLOAT(lng), localOffset, dst, 1);
//     localSunsetT =  calculateSunriseSunset(dayOfYear, FROMFLOAT(lat), FROMFLOAT(lng), localOffset, dst, 0);

//     printf("Sunrise: %02d:%02d\nSunset: %02d:%02d\n", localSunriseT / 60, localSunriseT % 60, localSunsetT / 60, localSunsetT % 60);

//     return 0;
// }