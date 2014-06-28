#ifndef F2C_MATHLIB_H
#define F2C_MATHLIB_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

/* #include "arith.h" */
#define IEEE_8087
#define Arith_Kind_ASL 1
#define Long int
#define Intcast (int)(long)
#define Double_Align
#define X64_bit_pointers
#define NANCHECK
#define QNaN0 0x0
#define QNaN1 0xfff80000

/* math macros from f2c.h */
#define abs(x) ((x) >= 0 ? (x) : -(x))
#define dabs(x) (doublereal)abs(x)
#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))
#define dmin(a,b) (doublereal)min(a,b)
#define dmax(a,b) (doublereal)max(a,b)
#define bit_test(a,b)	((a) >> (b) & 1)
#define bit_clear(a,b)	((a) & ~((uinteger)1 << (b)))
#define bit_set(a,b)	((a) |  ((uinteger)1 << (b)))


static inline int abort_(void);
static inline double c_abs(complex *z);
static inline void c_cos(complex *r, complex *z);
static inline void c_div(complex *c, complex *a, complex *b);
static inline void c_exp(complex *r, complex *z);
static inline void c_log(complex *r, complex *z);
static inline void c_sin(complex *r, complex *z);
static inline void c_sqrt(complex *r, complex *z);
static inline double f__cabs(double real, double imag);
static inline double d_abs(doublereal *x);
static inline double d_acos(doublereal *x);
static inline double d_asin(doublereal *x);
static inline double d_atan(doublereal *x);
static inline double d_atn2(doublereal *x, doublereal *y);
static inline double d_cos(doublereal *x);
static inline double d_cosh(doublereal *x);
static inline double d_dim(doublereal *a, doublereal *b);
static inline double d_exp(doublereal *x);
static inline double d_imag(doublecomplex *z);
static inline double d_int(doublereal *x);
static inline double d_lg10(doublereal *x);
static inline double d_log(doublereal *x);
static inline double d_mod(doublereal *x, doublereal *y);
static inline double d_nint(doublereal *x);
static inline double d_prod(real *x, real *y);
static inline double d_sign(doublereal *a, doublereal *b);
static inline double d_sin(doublereal *x);
static inline double d_sinh(doublereal *x);
static inline double d_sqrt(doublereal *x);
static inline double d_tan(doublereal *x);
static inline double d_tanh(doublereal *x);
static inline double derf_(doublereal *x);
static inline double derfc_(doublereal *x);
static inline int ef1asc_(ftnint *a, ftnlen *la, ftnint *b, ftnlen *lb);
static inline integer ef1cmc_(ftnint *a, ftnlen *la, ftnint *b, ftnlen *lb);
static inline double erf_(real *x);
static inline double erfc_(real *x);
static inline integer i_abs(integer *x);
static inline integer i_sceiling(real *x);
static inline integer i_dceiling(doublereal *x);
static inline integer i_dim(integer *a, integer *b);
static inline integer i_dnnt(doublereal *x);
static inline integer i_indx(char *a, char *b, ftnlen la, ftnlen lb);
static inline integer i_len(char *s, ftnlen n);
static inline integer i_len_trim(char *s, ftnlen n);
static inline integer i_mod(integer *a, integer *b);
static inline integer i_nint(real *x);
static inline integer i_sign(integer *a, integer *b);
static inline void pow_ci(complex *p, complex *a, integer *b) 	/* p = a**b  */;
static inline double pow_dd(doublereal *ap, doublereal *bp);
static inline double pow_di(doublereal *ap, integer *bp);
static inline integer pow_ii(integer *ap, integer *bp);
static inline double pow_ri(real *ap, integer *bp);
static inline void pow_zi(doublecomplex *p, doublecomplex *a, integer *b) 	/* p = a**b  */;
static inline void pow_zz(doublecomplex *r, doublecomplex *a, doublecomplex *b);
static inline double r_abs(real *x);
static inline double r_acos(real *x);
static inline double r_asin(real *x);
static inline double r_atan(real *x);
static inline double r_atn2(real *x, real *y);
static inline double r_cos(real *x);
static inline double r_cosh(real *x);
static inline double r_dim(real *a, real *b);
static inline double r_exp(real *x);
static inline double r_imag(complex *z);
static inline double r_int(real *x);
static inline double r_lg10(real *x);
static inline double r_log(real *x);
static inline double r_mod(real *x, real *y);
static inline double r_nint(real *x);
static inline double r_sign(real *a, real *b);
static inline double r_sin(real *x);
static inline double r_sinh(real *x);
static inline double r_sqrt(real *x);
static inline double r_tan(real *x);
static inline double r_tanh(real *x);
static inline integer s_cmp(char *a0, char *b0, ftnlen la, ftnlen lb);
static inline int s_copy(register char *a, register char *b, ftnlen la, ftnlen lb);
static inline integer s_rnge(char *varn, ftnint offset, char *procn, ftnint line);
static inline int s_stop(char *s, ftnlen n);
static inline void sig_die(const char *s, int kill);
static inline double z_abs(doublecomplex *z);
static inline void z_cos(doublecomplex *r, doublecomplex *z);
static inline void z_div(doublecomplex *c, doublecomplex *a, doublecomplex *b);
static inline void z_exp(doublecomplex *r, doublecomplex *z);
static inline void z_log(doublecomplex *r, doublecomplex *z);
static inline void z_sin(doublecomplex *r, doublecomplex *z);
static inline void z_sqrt(doublecomplex *r, doublecomplex *z);

/* Routine for reading machine parameters */
static inline double dlamch_(char *request) {
	/* Try to retrieve the floating point characteristic
	 * at compile time */
	const double NaN = 0.0/0.0;
	switch (*request) {
		case 'E':
		case 'e': { return DBL_EPSILON; }
		case 'B':
		case 'b': { return 2.0; }
		case 'S':
		case 's': { return DBL_MIN;}
		case 'P':
		case 'p': { return 2.0*DBL_EPSILON; }
		case 'N':
		case 'n': { return DBL_MANT_DIG; }
		case 'r':
		case 'R': { return FLT_ROUNDS==1; }
		case 'M':
		case 'm': { return DBL_MIN_EXP; }
		case 'L': 
		case 'l': { return DBL_MAX_EXP; }
		case 'u':
		case 'U': { return DBL_MIN; }
		case 'O':
		case 'o': { return DBL_MAX; }
		default: { return NaN; }
	}
}


/* *********************************************************************** */

static inline doublereal dlamc3_(doublereal *a, doublereal *b)
{
    /* System generated locals */
    doublereal ret_val;

/*     .. Executable Statements .. */

    ret_val = *a + *b;

    return ret_val;

/*     End of DLAMC3 */

} /* dlamc3_ */


/* *********************************************************************** */
/* abort_.c */


static inline int abort_(void)
{
sig_die("Fortran abort routine called", 1);
return 0;	/* not reached */
}
/* c_abs.c */


static inline double c_abs(complex *z)
{
return( f__cabs( z->r, z->i ) );
}
/* c_cos.c */



static inline void c_cos(complex *r, complex *z)
{
	double zi = z->i, zr = z->r;
	r->r =   cos(zr) * cosh(zi);
	r->i = - sin(zr) * sinh(zi);
	}
/* c_div.c */

static inline void c_div(complex *c, complex *a, complex *b)
{
	double ratio, den;
	double abr, abi, cr;

	if( (abr = b->r) < 0.)
		abr = - abr;
	if( (abi = b->i) < 0.)
		abi = - abi;
	if( abr <= abi )
		{
		if(abi == 0) {
			float af, bf;
			af = bf = abr;
			if (a->i != 0 || a->r != 0)
				af = 1.;
			c->i = c->r = af / bf;
			return;
			}
		ratio = (double)b->r / b->i ;
		den = b->i * (1 + ratio*ratio);
		cr = (a->r*ratio + a->i) / den;
		c->i = (a->i*ratio - a->r) / den;
		}

	else
		{
		ratio = (double)b->i / b->r ;
		den = b->r * (1 + ratio*ratio);
		cr = (a->r + a->i*ratio) / den;
		c->i = (a->i - a->r*ratio) / den;
		}
	c->r = cr;
	}
/* c_exp.c */



static inline void c_exp(complex *r, complex *z)
{
	double expx, zi = z->i;

	expx = exp(z->r);
	r->r = expx * cos(zi);
	r->i = expx * sin(zi);
	}
/* c_log.c */



static inline void c_log(complex *r, complex *z)
{
	double zi, zr;
	r->i = atan2(zi = z->i, zr = z->r);
	r->r = log( f__cabs(zr, zi) );
	}
/* c_sin.c */



static inline void c_sin(complex *r, complex *z)
{
	double zi = z->i, zr = z->r;
	r->r = sin(zr) * cosh(zi);
	r->i = cos(zr) * sinh(zi);
	}
/* c_sqrt.c */



static inline void c_sqrt(complex *r, complex *z)
{
	double mag, t;
	double zi = z->i, zr = z->r;

	if( (mag = f__cabs(zr, zi)) == 0.)
		r->r = r->i = 0.;
	else if(zr > 0)
		{
		r->r = t = sqrt(0.5 * (mag + zr) );
		t = zi / t;
		r->i = 0.5 * t;
		}
	else
		{
		t = sqrt(0.5 * (mag - zr) );
		if(zi < 0)
			t = -t;
		r->i = t;
		t = zi / t;
		r->r = 0.5 * t;
		}
	}
/* cabs.c */

static inline double f__cabs(double real, double imag)
{
double temp;

if(real < 0)
	real = -real;
if(imag < 0)
	imag = -imag;
if(imag > real){
	temp = real;
	real = imag;
	imag = temp;
}
if((real+imag) == real)
	return(real);

temp = imag/real;
temp = real*sqrt(1.0 + temp*temp);  /*overflow!!*/
return(temp);
}
/* ctype.c */
#define My_ctype_DEF
/* d_abs.c */

static inline double d_abs(doublereal *x)
{
if(*x >= 0)
	return(*x);
return(- *x);
}
/* d_acos.c */


static inline double d_acos(doublereal *x)
{
return( acos(*x) );
}
/* d_asin.c */


static inline double d_asin(doublereal *x)
{
return( asin(*x) );
}
/* d_atan.c */


static inline double d_atan(doublereal *x)
{
return( atan(*x) );
}
/* d_atn2.c */


static inline double d_atn2(doublereal *x, doublereal *y)
{
return( atan2(*x,*y) );
}
/* d_cnjg.c */

static inline VOID d_cnjg(doublecomplex *r, doublecomplex *z)
{
	doublereal zi = z->i;
	r->r = z->r;
	r->i = -zi;
	}
/* d_cos.c */


static inline double d_cos(doublereal *x)
{
return( cos(*x) );
}
/* d_cosh.c */


static inline double d_cosh(doublereal *x)
{
return( cosh(*x) );
}
/* d_dim.c */

static inline double d_dim(doublereal *a, doublereal *b)
{
return( *a > *b ? *a - *b : 0);
}
/* d_exp.c */


static inline double d_exp(doublereal *x)
{
return( exp(*x) );
}
/* d_imag.c */

static inline double d_imag(doublecomplex *z)
{
return(z->i);
}
/* d_int.c */


static inline double d_int(doublereal *x)
{
return( (*x>0) ? floor(*x) : -floor(- *x) );
}
/* d_lg10.c */

#define log10e 0.43429448190325182765


static inline double d_lg10(doublereal *x)
{
return( log10e * log(*x) );
}
/* d_log.c */


static inline double d_log(doublereal *x)
{
return( log(*x) );
}
/* d_mod.c */

static inline double d_mod(doublereal *x, doublereal *y)
{
	double xa, ya, z;
	if ((ya = *y) < 0.)
		ya = -ya;
	z = drem(xa = *x, ya);
	if (xa > 0) {
		if (z < 0)
			z += ya;
		}
	else if (z > 0)
		z -= ya;
	return z;
}
/* d_nint.c */


static inline double d_nint(doublereal *x)
{
return( (*x)>=0 ?
	floor(*x + .5) : -floor(.5 - *x) );
}
/* d_prod.c */

static inline double d_prod(real *x, real *y)
{
return( (*x) * (*y) );
}
/* d_sign.c */

static inline double d_sign(doublereal *a, doublereal *b)
{
double x;
x = (*a >= 0 ? *a : - *a);
return( *b >= 0 ? x : -x);
}
/* d_sin.c */


static inline double d_sin(doublereal *x)
{
return( sin(*x) );
}
/* d_sinh.c */


static inline double d_sinh(doublereal *x)
{
return( sinh(*x) );
}
/* d_sqrt.c */


static inline double d_sqrt(doublereal *x)
{
return( sqrt(*x) );
}
/* d_tan.c */


static inline double d_tan(doublereal *x)
{
return( tan(*x) );
}
/* d_tanh.c */


static inline double d_tanh(doublereal *x)
{
return( tanh(*x) );
}
/* derf_.c */

static inline double derf_(doublereal *x)
{
return( erf(*x) );
}
/* derfc_.c */


static inline double derfc_(doublereal *x)
{
return( erfc(*x) );
}
/* ef1asc_.c */
/* EFL support routine to copy string b to string a */



#define M	( (long) (sizeof(long) - 1) )
#define EVEN(x)	( ( (x)+ M) & (~M) )

static inline int ef1asc_(ftnint *a, ftnlen *la, ftnint *b, ftnlen *lb)
{
s_copy( (char *)a, (char *)b, EVEN(*la), *lb );
return 0;	/* ignored return value */
}
/* ef1cmc_.c */
/* EFL support routine to compare two character strings */


static inline integer ef1cmc_(ftnint *a, ftnlen *la, ftnint *b, ftnlen *lb)
{
return( s_cmp( (char *)a, (char *)b, *la, *lb) );
}
/* erf_.c */

static inline double erf_(real *x)
{
return( erf((double)*x) );
}
/* erfc_.c */

static inline double erfc_(real *x)
{
return( erfc((double)*x) );
}
/* f77_aloc.c */

static integer memfailure = 3;

static inline char * F77_aloc(integer Len, const char *whence)
{
	char *rv;
	unsigned int uLen = (unsigned int) Len;	/* for K&R C */

	if (!(rv = (char*)malloc(uLen))) {
		fprintf(stderr, "malloc(%u) failure in %s\n",
			uLen, whence);
		exit(memfailure);
		}
	return rv;
	}
/* h_abs.c */

static inline shortint h_abs(shortint *x)
{
if(*x >= 0)
	return(*x);
return(- *x);
}
/* h_dim.c */

static inline shortint h_dim(shortint *a, shortint *b)
{
return( *a > *b ? *a - *b : 0);
}
/* h_dnnt.c */


static inline shortint h_dnnt(doublereal *x)
{
return (shortint)(*x >= 0. ? floor(*x + .5) : -floor(.5 - *x));
}
/* h_indx.c */

static inline shortint h_indx(char *a, char *b, ftnlen la, ftnlen lb)
{
ftnlen i, n;
char *s, *t, *bend;

n = la - lb + 1;
bend = b + lb;

for(i = 0 ; i < n ; ++i)
	{
	s = a + i;
	t = b;
	while(t < bend)
		if(*s++ != *t++)
			goto no;
	return((shortint)i+1);
	no: ;
	}
return(0);
}
/* h_len.c */

static inline shortint h_len(char *s, ftnlen n)
{
return(n);
}
/* h_mod.c */

static inline shortint h_mod(short *a, short *b)
{
return( *a % *b);
}
/* h_nint.c */


static inline shortint h_nint(real *x)
{
return (shortint)(*x >= 0 ? floor(*x + .5) : -floor(.5 - *x));
}
/* h_sign.c */

static inline shortint h_sign(shortint *a, shortint *b)
{
shortint x;
x = (*a >= 0 ? *a : - *a);
return( *b >= 0 ? x : -x);
}
/* hl_ge.c */

static inline shortlogical hl_ge(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) >= 0);
}
/* hl_gt.c */

static inline shortlogical hl_gt(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) > 0);
}
/* hl_le.c */

static inline shortlogical hl_le(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) <= 0);
}
/* hl_lt.c */

static inline shortlogical hl_lt(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) < 0);
}
/* i_abs.c */

static inline integer i_abs(integer *x)
{
if(*x >= 0)
	return(*x);
return(- *x);
}
/* i_ceiling.c */

static inline integer i_sceiling(real *x)
{
#define CEIL(x) ((int)(x) + ((x) > 0 && (x) != (int)(x)))

    return (integer) CEIL(*x);
}


static inline integer i_dceiling(doublereal *x)
{
#define CEIL(x) ((int)(x) + ((x) > 0 && (x) != (int)(x)))

    return (integer) CEIL(*x);
}
/* i_dim.c */

static inline integer i_dim(integer *a, integer *b)
{
return( *a > *b ? *a - *b : 0);
}
/* i_dnnt.c */


static inline integer i_dnnt(doublereal *x)
{
return (integer)(*x >= 0. ? floor(*x + .5) : -floor(.5 - *x));
}
/* i_indx.c */

static inline integer i_indx(char *a, char *b, ftnlen la, ftnlen lb)
{
ftnlen i, n;
char *s, *t, *bend;

n = la - lb + 1;
bend = b + lb;

for(i = 0 ; i < n ; ++i)
	{
	s = a + i;
	t = b;
	while(t < bend)
		if(*s++ != *t++)
			goto no;
	return(i+1);
	no: ;
	}
return(0);
}
/* i_len.c */

static inline integer i_len(char *s, ftnlen n)
{
return(n);
}
/* i_len_trim.c */

static inline integer i_len_trim(char *s, ftnlen n)
{
  int i;

  for(i=n-1;i>=0;i--)
    if(s[i] != ' ')
      return i + 1;

  return(0);
}
/* i_mod.c */

static inline integer i_mod(integer *a, integer *b)
{
return( *a % *b);
}
/* i_nint.c */


static inline integer i_nint(real *x)
{
return (integer)(*x >= 0 ? floor(*x + .5) : -floor(.5 - *x));
}
/* i_sign.c */

static inline integer i_sign(integer *a, integer *b)
{
integer x;
x = (*a >= 0 ? *a : - *a);
return( *b >= 0 ? x : -x);
}
/* l_ge.c */

static inline logical l_ge(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) >= 0);
}
/* l_gt.c */

static inline logical l_gt(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) > 0);
}
/* l_le.c */

static inline logical l_le(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) <= 0);
}
/* l_lt.c */

static inline logical l_lt(char *a, char *b, ftnlen la, ftnlen lb)
{
return(s_cmp(a,b,la,lb) < 0);
}
/* lbitbits.c */

#ifndef LONGBITS
#define LONGBITS (sizeof(long)*8)
#endif

static inline integer lbit_bits(integer a, integer b, integer len)
{
	/* Assume 2's complement arithmetic */

	unsigned long x, y;

	x = (unsigned long) a;
	y = (unsigned long)-1L;
	x >>= b;
	y <<= len;
	return (integer)(x & ~y);
	}

static inline integer lbit_cshift(integer a, integer b, integer len)
{
	unsigned long x, y, z;

	x = (unsigned long)a;
	if (len <= 0) {
		if (len == 0)
			return 0;
		goto full_len;
		}
	if (len >= LONGBITS) {
 full_len:
		if (b >= 0) {
			b %= LONGBITS;
			return (integer)(x << b | x >> (LONGBITS -b ));
			}
		b = -b;
		b %= LONGBITS;
		return (integer)(x << (LONGBITS - b) | x >> b);
		}
	y = z = (unsigned long)-1;
	y <<= len;
	z &= ~y;
	y &= x;
	x &= z;
	if (b >= 0) {
		b %= len;
		return (integer)(y | (z & (x << b | x >> (len - b))));
		}
	b = -b;
	b %= len;
	return (integer)(y | (z & (x >> b | x << (len - b))));
	}
/* lbitshft.c */

static inline integer
lbit_shift(integer a, integer b)
{
	return b >= 0 ? a << b : (integer)((uinteger)a >> -b);
	}
/* pow_ci.c */

static inline void pow_ci(complex *p, complex *a, integer *b) 	/* p = a**b  */
{
doublecomplex p1, a1;

a1.r = a->r;
a1.i = a->i;

pow_zi(&p1, &a1, b);

p->r = p1.r;
p->i = p1.i;
}
/* pow_dd.c */


static inline double pow_dd(doublereal *ap, doublereal *bp)
{
return(pow(*ap, *bp) );
}
/* pow_di.c */

static inline double pow_di(doublereal *ap, integer *bp)
{
double pow, x;
integer n;
unsigned long u;

pow = 1;
x = *ap;
n = *bp;

if(n != 0)
	{
	if(n < 0)
		{
		n = -n;
		x = 1/x;
		}
	for(u = n; ; )
		{
		if(u & 01)
			pow *= x;
		if(u >>= 1)
			x *= x;
		else
			break;
		}
	}
return(pow);
}
/* pow_hh.c */

static inline shortint pow_hh(shortint *ap, shortint *bp)
{
	shortint pow, x, n;
	unsigned u;

	x = *ap;
	n = *bp;

	if (n <= 0) {
		if (n == 0 || x == 1)
			return 1;
		if (x != -1)
			return x == 0 ? 1/x : 0;
		n = -n;
		}
	u = n;
	for(pow = 1; ; )
		{
		if(u & 01)
			pow *= x;
		if(u >>= 1)
			x *= x;
		else
			break;
		}
	return(pow);
	}
/* pow_ii.c */

static inline integer pow_ii(integer *ap, integer *bp)
{
	integer pow, x, n;
	unsigned long u;

	x = *ap;
	n = *bp;

	if (n <= 0) {
		if (n == 0 || x == 1)
			return 1;
		if (x != -1)
			return x == 0 ? 1/x : 0;
		n = -n;
		}
	u = n;
	for(pow = 1; ; )
		{
		if(u & 01)
			pow *= x;
		if(u >>= 1)
			x *= x;
		else
			break;
		}
	return(pow);
	}
/* pow_ri.c */

static inline double pow_ri(real *ap, integer *bp)
{
double pow, x;
integer n;
unsigned long u;

pow = 1;
x = *ap;
n = *bp;

if(n != 0)
	{
	if(n < 0)
		{
		n = -n;
		x = 1/x;
		}
	for(u = n; ; )
		{
		if(u & 01)
			pow *= x;
		if(u >>= 1)
			x *= x;
		else
			break;
		}
	}
return(pow);
}
/* pow_zi.c */

static inline void pow_zi(doublecomplex *p, doublecomplex *a, integer *b) 	/* p = a**b  */
{
	integer n;
	unsigned long u;
	double t;
	doublecomplex q, x;
	static doublecomplex one = {1.0, 0.0};

	n = *b;
	q.r = 1;
	q.i = 0;

	if(n == 0)
		goto done;
	if(n < 0)
		{
		n = -n;
		z_div(&x, &one, a);
		}
	else
		{
		x.r = a->r;
		x.i = a->i;
		}

	for(u = n; ; )
		{
		if(u & 01)
			{
			t = q.r * x.r - q.i * x.i;
			q.i = q.r * x.i + q.i * x.r;
			q.r = t;
			}
		if(u >>= 1)
			{
			t = x.r * x.r - x.i * x.i;
			x.i = 2 * x.r * x.i;
			x.r = t;
			}
		else
			break;
		}
 done:
	p->i = q.i;
	p->r = q.r;
	}
/* pow_zz.c */


static inline void pow_zz(doublecomplex *r, doublecomplex *a, doublecomplex *b)
{
double logr, logi, x, y;

logr = log( f__cabs(a->r, a->i) );
logi = atan2(a->i, a->r);

x = exp( logr * b->r - logi * b->i );
y = logr * b->i + logi * b->r;

r->r = x * cos(y);
r->i = x * sin(y);
}
/* r_abs.c */

static inline double r_abs(real *x)
{
if(*x >= 0)
	return(*x);
return(- *x);
}
/* r_acos.c */


static inline double r_acos(real *x)
{
return( acos(*x) );
}
/* r_asin.c */


static inline double r_asin(real *x)
{
return( asin(*x) );
}
/* r_atan.c */


static inline double r_atan(real *x)
{
return( atan(*x) );
}
/* r_atn2.c */


static inline double r_atn2(real *x, real *y)
{
return( atan2(*x,*y) );
}
/* r_cnjg.c */

static inline VOID r_cnjg(complex *r, complex *z)
{
	real zi = z->i;
	r->r = z->r;
	r->i = -zi;
	}
/* r_cos.c */


static inline double r_cos(real *x)
{
return( cos(*x) );
}
/* r_cosh.c */


static inline double r_cosh(real *x)
{
return( cosh(*x) );
}
/* r_dim.c */

static inline double r_dim(real *a, real *b)
{
return( *a > *b ? *a - *b : 0);
}
/* r_exp.c */


static inline double r_exp(real *x)
{
return( exp(*x) );
}
/* r_imag.c */

static inline double r_imag(complex *z)
{
return(z->i);
}
/* r_int.c */


static inline double r_int(real *x)
{
return( (*x>0) ? floor(*x) : -floor(- *x) );
}
/* r_lg10.c */

#define log10e 0.43429448190325182765


static inline double r_lg10(real *x)
{
return( log10e * log(*x) );
}
/* r_log.c */


static inline double r_log(real *x)
{
return( log(*x) );
}
/* r_mod.c */

static inline double r_mod(real *x, real *y)
{
	double xa, ya, z;
	if ((ya = *y) < 0.)
		ya = -ya;
	z = drem(xa = *x, ya);
	if (xa > 0) {
		if (z < 0)
			z += ya;
		}
	else if (z > 0)
		z -= ya;
	return z;
}
/* r_nint.c */


static inline double r_nint(real *x)
{
return( (*x)>=0 ?
	floor(*x + .5) : -floor(.5 - *x) );
}
/* r_sign.c */

static inline double r_sign(real *a, real *b)
{
double x;
x = (*a >= 0 ? *a : - *a);
return( *b >= 0 ? x : -x);
}
/* r_sin.c */


static inline double r_sin(real *x)
{
return( sin(*x) );
}
/* r_sinh.c */


static inline double r_sinh(real *x)
{
return( sinh(*x) );
}
/* r_sqrt.c */


static inline double r_sqrt(real *x)
{
return( sqrt(*x) );
}
/* r_tan.c */


static inline double r_tan(real *x)
{
return( tan(*x) );
}
/* r_tanh.c */


static inline double r_tanh(real *x)
{
return( tanh(*x) );
}
/* s_cat.c */

/* Unless compiled with -DNO_OVERWRITE, this variant of s_cat allows the
 * target of a concatenation to appear on its right-hand side (contrary
 * to the Fortran 77 Standard, but in accordance with Fortran 90).
 */


static inline int s_cat(char *lp, char *rpp[], ftnint rnp[], ftnint *np, ftnlen ll)
{
	ftnlen i, nc;
	char *rp;
	ftnlen n = *np;
#ifndef NO_OVERWRITE
	ftnlen L, m;
	char *lp0, *lp1;

	lp0 = 0;
	lp1 = lp;
	L = ll;
	i = 0;
	while(i < n) {
		rp = rpp[i];
		m = rnp[i++];
		if (rp >= lp1 || rp + m <= lp) {
			if ((L -= m) <= 0) {
				n = i;
				break;
				}
			lp1 += m;
			continue;
			}
		lp0 = lp;
		lp = lp1 = F77_aloc(L = ll, "s_cat");
		break;
		}
	lp1 = lp;
#endif /* NO_OVERWRITE */
	for(i = 0 ; i < n ; ++i) {
		nc = ll;
		if(rnp[i] < nc)
			nc = rnp[i];
		ll -= nc;
		rp = rpp[i];
		while(--nc >= 0)
			*lp++ = *rp++;
		}
	while(--ll >= 0)
		*lp++ = ' ';
#ifndef NO_OVERWRITE
	if (lp0) {
		memcpy(lp0, lp1, L);
		free(lp1);
		}
#endif
	return 0;
	}

/* s_cmp.c */

/* compare two strings */

static inline integer s_cmp(char *a0, char *b0, ftnlen la, ftnlen lb)
{
register unsigned char *a, *aend, *b, *bend;
a = (unsigned char *)a0;
b = (unsigned char *)b0;
aend = a + la;
bend = b + lb;

if(la <= lb)
	{
	while(a < aend)
		if(*a != *b)
			return( *a - *b );
		else
			{ ++a; ++b; }

	while(b < bend)
		if(*b != ' ')
			return( ' ' - *b );
		else	++b;
	}

else
	{
	while(b < bend)
		if(*a == *b)
			{ ++a; ++b; }
		else
			return( *a - *b );
	while(a < aend)
		if(*a != ' ')
			return(*a - ' ');
		else	++a;
	}
return(0);
}
/* s_copy.c */
/* Unless compiled with -DNO_OVERWRITE, this variant of s_copy allows the
 * target of an assignment to appear on its right-hand side (contrary
 * to the Fortran 77 Standard, but in accordance with Fortran 90),
 * as in  a(2:5) = a(4:7) .
 */


/* assign strings:  a = b */

static inline int s_copy(register char *a, register char *b, ftnlen la, ftnlen lb)
{
	register char *aend, *bend;

	aend = a + la;

	if(la <= lb)
#ifndef NO_OVERWRITE
		if (a <= b || a >= b + la)
#endif
			while(a < aend)
				*a++ = *b++;
#ifndef NO_OVERWRITE
		else
			for(b += la; a < aend; )
				*--aend = *--b;
#endif

	else {
		bend = b + lb;
#ifndef NO_OVERWRITE
		if (a <= b || a >= bend)
#endif
			while(b < bend)
				*a++ = *b++;
#ifndef NO_OVERWRITE
		else {
			a += lb;
			while(b < bend)
				*--a = *--bend;
			a += lb;
			}
#endif
		while(a < aend)
			*a++ = ' ';
		}
		return 0;
	}

/* s_rnge.c */

/* called when a subscript is out of range */

static inline integer s_rnge(char *varn, ftnint offset, char *procn, ftnint line)
{
register int i;

fprintf(stderr, "Subscript out of range on file line %ld, procedure ",
	(long)line);
while((i = *procn) && i != '_' && i != ' ')
	putc(*procn++, stderr);
fprintf(stderr, ".\nAttempt to access the %ld-th element of variable ",
	(long)offset+1);
while((i = *varn) && i != ' ')
	putc(*varn++, stderr);
sig_die(".", 1);
return 0;	/* not reached */
}
/* s_stop.c */




static inline int s_stop(char *s, ftnlen n)
{
int i;

if(n > 0)
	{
	fprintf(stderr, "STOP ");
	for(i = 0; i<n ; ++i)
		putc(*s++, stderr);
	fprintf(stderr, " statement executed\n");
	}
exit(0);

/* We cannot avoid (useless) compiler diagnostics here:		*/
/* some compilers complain if there is no return statement,	*/
/* and others complain that this one cannot be reached.		*/

return 0; /* NOT REACHED */
}

/* sig_die.c */
#ifndef SIGIOT
#ifdef SIGABRT
#define SIGIOT SIGABRT
#endif
#endif

static inline void sig_die(const char *s, int kill)
{
	/* print error message, then clear buffers */
	fprintf(stderr, "%s\n", s);

	if(kill)
		{
		fflush(stderr);
		/* now get a core */
		abort();
		}
	else {
		exit(1);
		}
	}

#if 0
/* signal_.c */

 ftnint
signal_(integer *sigp, sig_pf proc)
{
	int sig;
	sig = (int)*sigp;

	return (ftnint)signal(sig, proc);
	}
/* system_.c */
/* f77 interface to system routine */
#endif



static inline integer
system_(register char *s, ftnlen n)
{
	char buff0[256], *buff;
	register char *bp, *blast;
	integer rv;

	buff = bp = n < sizeof(buff0)
			? buff0 : F77_aloc(n+1, "system_");
	blast = bp + n;

	while(bp < blast && *s)
		*bp++ = *s++;
	*bp = 0;
	rv = system(buff);
	if (buff != buff0)
		free(buff);
	return rv;
	}
/* typesize.c */

/*
ftnlen f__typesize[] = { 0, 0, sizeof(shortint), sizeof(integer),
			sizeof(real), sizeof(doublereal),
			sizeof(complex), sizeof(doublecomplex),
			sizeof(logical), sizeof(char),
			0, sizeof(integer1),
			sizeof(logical1), sizeof(shortlogical),
#ifdef Allow_TYQUAD
			sizeof(longint),
#endif
			0}; */
/* uninit.c */

#ifdef IEEE_PATCH_FORTRAN
#define TYSHORT 2
#define TYLONG 3
#define TYREAL 4
#define TYDREAL 5
#define TYCOMPLEX 6
#define TYDCOMPLEX 7
#define TYINT1 11
#define TYQUAD 14
#ifndef Long
#define Long long
#endif

#ifdef __mips
#define RNAN	0xffc00000
#define DNAN0	0xfff80000
#define DNAN1	0
#endif

#ifdef _PA_RISC1_1
#define RNAN	0xffc00000
#define DNAN0	0xfff80000
#define DNAN1	0
#endif

#ifndef RNAN
#define RNAN	0xff800001
#ifdef IEEE_MC68k
#define DNAN0	0xfff00000
#define DNAN1	1
#else
#define DNAN0	1
#define DNAN1	0xfff00000
#endif
#endif /*RNAN*/

#ifdef KR_headers
#define Void /*void*/
#define FA7UL (unsigned Long) 0xfa7a7a7aL
#else
#define Void void
#define FA7UL 0xfa7a7a7aUL
#endif

#ifdef __cplusplus
extern "C" {
#endif

static void ieee0(Void);

static unsigned Long rnan = RNAN,
	dnan0 = DNAN0,
	dnan1 = DNAN1;

/*double _0 = 0.; */

static inline void
_uninit_f2c(void *x, int type, long len)
{
	static int first = 1;

	unsigned Long *lx, *lxe;

	if (first) {
		first = 0;
		ieee0();
		}
	if (len == 1)
	 switch(type) {
	  case TYINT1:
		*(char*)x = 'Z';
		return;
	  case TYSHORT:
		*(short*)x = 0xfa7a;
		break;
	  case TYLONG:
		*(unsigned Long*)x = FA7UL;
		return;
	  case TYQUAD:
	  case TYCOMPLEX:
	  case TYDCOMPLEX:
		break;
	  case TYREAL:
		*(unsigned Long*)x = rnan;
		return;
	  case TYDREAL:
		lx = (unsigned Long*)x;
		lx[0] = dnan0;
		lx[1] = dnan1;
		return;
	  default:
		printf("Surprise type %d in _uninit_f2c\n", type);
	  }
	switch(type) {
	  case TYINT1:
		memset(x, 'Z', len);
		break;
	  case TYSHORT:
		*(short*)x = 0xfa7a;
		break;
	  case TYQUAD:
		len *= 2;
		/* no break */
	  case TYLONG:
		lx = (unsigned Long*)x;
		lxe = lx + len;
		while(lx < lxe)
			*lx++ = FA7UL;
		break;
	  case TYCOMPLEX:
		len *= 2;
		/* no break */
	  case TYREAL:
		lx = (unsigned Long*)x;
		lxe = lx + len;
		while(lx < lxe)
			*lx++ = rnan;
		break;
	  case TYDCOMPLEX:
		len *= 2;
		/* no break */
	  case TYDREAL:
		lx = (unsigned Long*)x;
		for(lxe = lx + 2*len; lx < lxe; lx += 2) {
			lx[0] = dnan0;
			lx[1] = dnan1;
			}
	  }
	}
#ifdef __cplusplus
}
#endif

#ifndef MSpc
#ifdef MSDOS
#define MSpc
#else
#ifdef _WIN32
#define MSpc
#endif
#endif
#endif

#ifdef MSpc
#define IEEE0_done
#include "float.h"
#include "signal.h"

 static void
ieee0(Void)
{
#ifndef __alpha
#ifndef EM_DENORMAL
#define EM_DENORMAL _EM_DENORMAL
#endif
#ifndef EM_UNDERFLOW
#define EM_UNDERFLOW _EM_UNDERFLOW
#endif
#ifndef EM_INEXACT
#define EM_INEXACT _EM_INEXACT
#endif
#ifndef MCW_EM
#define MCW_EM _MCW_EM
#endif
	_control87(EM_DENORMAL | EM_UNDERFLOW | EM_INEXACT, MCW_EM);
#endif
	/* With MS VC++, compiling and linking with -Zi will permit */
	/* clicking to invoke the MS C++ debugger, which will show */
	/* the point of error -- provided SIGFPE is SIG_DFL. */
	signal(SIGFPE, SIG_DFL);
	}
#endif /* MSpc */

#ifdef __mips	/* must link with -lfpe */
#define IEEE0_done
/* code from Eric Grosse */
#include <stdlib.h>
#include <stdio.h>
#include "/usr/include/sigfpe.h"	/* full pathname for lcc -N */
#include "/usr/include/sys/fpu.h"

 static void
#ifdef KR_headers
ieeeuserhand(exception, val) unsigned exception[5]; int val[2];
#else
ieeeuserhand(unsigned exception[5], int val[2])
#endif
{
	fflush(stdout);
	fprintf(stderr,"ieee0() aborting because of ");
	if(exception[0]==_OVERFL) fprintf(stderr,"overflow\n");
	else if(exception[0]==_UNDERFL) fprintf(stderr,"underflow\n");
	else if(exception[0]==_DIVZERO) fprintf(stderr,"divide by 0\n");
	else if(exception[0]==_INVALID) fprintf(stderr,"invalid operation\n");
	else fprintf(stderr,"\tunknown reason\n");
	fflush(stderr);
	abort();
}

 static void
#ifdef KR_headers
ieeeuserhand2(j) unsigned int **j;
#else
ieeeuserhand2(unsigned int **j)
#endif
{
	fprintf(stderr,"ieee0() aborting because of confusion\n");
	abort();
}

 static void
ieee0(Void)
{
	int i;
	for(i=1; i<=4; i++){
		sigfpe_[i].count = 1000;
		sigfpe_[i].trace = 1;
		sigfpe_[i].repls = _USER_DETERMINED;
		}
	sigfpe_[1].repls = _ZERO;	/* underflow */
	handle_sigfpes( _ON,
		_EN_UNDERFL|_EN_OVERFL|_EN_DIVZERO|_EN_INVALID,
		ieeeuserhand,_ABORT_ON_ERROR,ieeeuserhand2);
	}
#endif /* mips */

#ifdef __linux__
#define IEEE0_done
#include "fpu_control.h"

#ifdef __alpha__
#ifndef USE_setfpucw
#define __setfpucw(x) __fpu_control = (x)
#endif
#endif

#ifndef _FPU_SETCW

#define Can_use__setfpucw
#endif

 static void
ieee0(Void)
{
#if (defined(__mc68000__) || defined(__mc68020__) || defined(mc68020) || defined (__mc68k__))
/* Reported 20010705 by Alan Bain <alanb@chiark.greenend.org.uk> */
/* Note that IEEE 754 IOP (illegal operation) */
/* = Signaling NAN (SNAN) + operation error (OPERR). */
#ifdef Can_use__setfpucw
	__setfpucw(_FPU_IEEE + _FPU_DOUBLE + _FPU_MASK_OPERR + _FPU_MASK_DZ + _FPU_MASK_SNAN+_FPU_MASK_OVFL);
#else
	__fpu_control = _FPU_IEEE + _FPU_DOUBLE + _FPU_MASK_OPERR + _FPU_MASK_DZ + _FPU_MASK_SNAN+_FPU_MASK_OVFL;
	_FPU_SETCW(__fpu_control);
#endif

#elif (defined(__powerpc__)||defined(_ARCH_PPC)||defined(_ARCH_PWR)) /* !__mc68k__ */
/* Reported 20011109 by Alan Bain <alanb@chiark.greenend.org.uk> */

#ifdef Can_use__setfpucw

/* The following is NOT a mistake -- the author of the fpu_control.h
for the PPC has erroneously defined IEEE mode to turn on exceptions
other than Inexact! Start from default then and turn on only the ones
which we want*/

	__setfpucw(_FPU_DEFAULT +  _FPU_MASK_IM+_FPU_MASK_OM+_FPU_MASK_UM);

#else /* PPC && !Can_use__setfpucw */

	__fpu_control = _FPU_DEFAULT +_FPU_MASK_OM+_FPU_MASK_IM+_FPU_MASK_UM;
	_FPU_SETCW(__fpu_control);

#endif /*Can_use__setfpucw*/

#else /* !(mc68000||powerpc) */

#ifdef _FPU_IEEE
#ifndef _FPU_EXTENDED /* e.g., ARM processor under Linux */
#define _FPU_EXTENDED 0
#endif
#ifndef _FPU_DOUBLE
#define _FPU_DOUBLE 0
#endif
#ifdef Can_use__setfpucw /* pre-1997 (?) Linux */
	__setfpucw(_FPU_IEEE - _FPU_MASK_IM - _FPU_MASK_ZM - _FPU_MASK_OM);
#else
#ifdef UNINIT_F2C_PRECISION_53 /* 20051004 */
	/* unmask invalid, etc., and change rounding precision to double */
	__fpu_control = _FPU_IEEE - _FPU_EXTENDED + _FPU_DOUBLE - _FPU_MASK_IM - _FPU_MASK_ZM - _FPU_MASK_OM;
	_FPU_SETCW(__fpu_control);
#else
	/* unmask invalid, etc., and keep current rounding precision */
	fpu_control_t cw;
	_FPU_GETCW(cw);
	cw &= ~(_FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM);
	_FPU_SETCW(cw);
#endif
#endif

#else /* !_FPU_IEEE */

	fprintf(stderr, "\n%s\n%s\n%s\n%s\n",
		"WARNING:  _uninit_f2c in libf2c does not know how",
		"to enable trapping on this system, so f2c's -trapuv",
		"option will not detect uninitialized variables unless",
		"you can enable trapping manually.");
	fflush(stderr);

#endif /* _FPU_IEEE */
#endif /* __mc68k__ */
	}
#endif /* __linux__ */

#ifdef __alpha
#ifndef IEEE0_done
#define IEEE0_done
#include <machine/fpu.h>
 static void
ieee0(Void)
{
	ieee_set_fp_control(IEEE_TRAP_ENABLE_INV);
	}
#endif /*IEEE0_done*/
#endif /*__alpha*/

#ifdef __hpux
#define IEEE0_done
#define _INCLUDE_HPUX_SOURCE
#include <math.h>

#ifndef FP_X_INV
#include <fenv.h>
#define fpsetmask fesettrapenable
#define FP_X_INV FE_INVALID
#endif

 static void
ieee0(Void)
{
	fpsetmask(FP_X_INV);
	}
#endif /*__hpux*/

#ifdef _AIX
#define IEEE0_done
#include <fptrap.h>

 static void
ieee0(Void)
{
	fp_enable(TRP_INVALID);
	fp_trap(FP_TRAP_SYNC);
	}
#endif /*_AIX*/

#ifdef __sun
#define IEEE0_done
#include <ieeefp.h>

 static void
ieee0(Void)
{
	fpsetmask(FP_X_INV);
	}
#endif /*__sparc*/

#ifndef IEEE0_done
 static void
ieee0(Void) {}
#endif
#endif 

/* z_abs.c */

static inline double z_abs(doublecomplex *z)
{
return( f__cabs( z->r, z->i ) );
}
/* z_cos.c */


static inline void z_cos(doublecomplex *r, doublecomplex *z)
{
	double zi = z->i, zr = z->r;
	r->r =   cos(zr) * cosh(zi);
	r->i = - sin(zr) * sinh(zi);
	}
/* z_div.c */

static inline void z_div(doublecomplex *c, doublecomplex *a, doublecomplex *b)
{
	double ratio, den;
	double abr, abi, cr;

	if( (abr = b->r) < 0.)
		abr = - abr;
	if( (abi = b->i) < 0.)
		abi = - abi;
	if( abr <= abi )
		{
		if(abi == 0) {
			if (a->i != 0 || a->r != 0)
				abi = 1.;
			c->i = c->r = abi / abr;
			return;
			}
		ratio = b->r / b->i ;
		den = b->i * (1 + ratio*ratio);
		cr = (a->r*ratio + a->i) / den;
		c->i = (a->i*ratio - a->r) / den;
		}

	else
		{
		ratio = b->i / b->r ;
		den = b->r * (1 + ratio*ratio);
		cr = (a->r + a->i*ratio) / den;
		c->i = (a->i - a->r*ratio) / den;
		}
	c->r = cr;
	}
/* z_exp.c */


static inline void z_exp(doublecomplex *r, doublecomplex *z)
{
	double expx, zi = z->i;

	expx = exp(z->r);
	r->r = expx * cos(zi);
	r->i = expx * sin(zi);
	}
/* z_log.c */
#define GCC_COMPARE_BUG_FIXED
#ifndef NO_DOUBLE_EXTENDED
#ifndef GCC_COMPARE_BUG_FIXED
#ifndef Pre20000310
#ifdef Comment
Some versions of gcc, such as 2.95.3 and 3.0.4, are buggy under -O2 or -O3:
on IA32 (Intel 80x87) systems, they may do comparisons on values computed
in extended-precision registers.  This can lead to the test "s > s0" that
was used below being carried out incorrectly.  The fix below cannot be
spoiled by overzealous optimization, since the compiler cannot know
whether gcc_bug_bypass_diff_F2C will be nonzero.  (We expect it always
to be zero.  The weird name is unlikely to collide with anything.)

An example (provided by Ulrich Jakobus) where the bug fix matters is

	double complex a, b
	a = (.1099557428756427618354862829619, .9857360542953131909982289471372)
	b = log(a)

An alternative to the fix below would be to use 53-bit rounding precision,
but the means of specifying this 80x87 feature are highly unportable.
#endif /*Comment*/
#define BYPASS_GCC_COMPARE_BUG
double (*gcc_bug_bypass_diff_F2C) (double*,double*);
static double
diff1(double *a, double *b)
{ return *a - *b; }
#endif /*Pre20000310*/
#endif /*GCC_COMPARE_BUG_FIXED*/
#endif /*NO_DOUBLE_EXTENDED*/

static inline void z_log(doublecomplex *r, doublecomplex *z)
{
	double s, s0, t, t2, u, v;
	double zi = z->i, zr = z->r;
#ifdef BYPASS_GCC_COMPARE_BUG
	double (*diff) (double*,double*);
#endif

	r->i = atan2(zi, zr);
#ifdef Pre20000310
	r->r = log( f__cabs( zr, zi ) );
#else
	if (zi < 0)
		zi = -zi;
	if (zr < 0)
		zr = -zr;
	if (zr < zi) {
		t = zi;
		zi = zr;
		zr = t;
		}
	t = zi/zr;
	s = zr * sqrt(1 + t*t);
	/* now s = f__cabs(zi,zr), and zr = |zr| >= |zi| = zi */
	if ((t = s - 1) < 0)
		t = -t;
	if (t > .01)
		r->r = log(s);
	else {

#ifdef Comment

	log(1+x) = x - x^2/2 + x^3/3 - x^4/4 + - ...

		 = x(1 - x/2 + x^2/3 -+...)

	[sqrt(y^2 + z^2) - 1] * [sqrt(y^2 + z^2) + 1] = y^2 + z^2 - 1, so

	sqrt(y^2 + z^2) - 1 = (y^2 + z^2 - 1) / [sqrt(y^2 + z^2) + 1]

#endif /*Comment*/

#ifdef BYPASS_GCC_COMPARE_BUG
		if (!(diff = gcc_bug_bypass_diff_F2C))
			diff = diff1;
#endif
		t = ((zr*zr - 1.) + zi*zi) / (s + 1);
		t2 = t*t;
		s = 1. - 0.5*t;
		u = v = 1;
		do {
			s0 = s;
			u *= t2;
			v += 2;
			s += u/v - t*u/(v+1);
			}
#ifdef BYPASS_GCC_COMPARE_BUG
			while(s - s0 > 1e-18 || (*diff)(&s,&s0) > 0.);
#else
			while(s > s0);
#endif
		r->r = s*t;
		}
#endif
	}
/* z_sin.c */


static inline void z_sin(doublecomplex *r, doublecomplex *z)
{
	double zi = z->i, zr = z->r;
	r->r = sin(zr) * cosh(zi);
	r->i = cos(zr) * sinh(zi);
	}
/* z_sqrt.c */


static inline void z_sqrt(doublecomplex *r, doublecomplex *z)
{
	double mag, zi = z->i, zr = z->r;

	if( (mag = f__cabs(zr, zi)) == 0.)
		r->r = r->i = 0.;
	else if(zr > 0)
		{
		r->r = sqrt(0.5 * (mag + zr) );
		r->i = zi / r->r / 2;
		}
	else
		{
		r->i = sqrt(0.5 * (mag - zr) );
		if(zi < 0)
			r->i = - r->i;
		r->r = zi / r->i / 2;
		}
	}
#endif
