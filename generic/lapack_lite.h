/*This module contributed by Doug Heisterkamp
Modified by Jim Hugunin
More modifications by Jeff Whitaker
*/

#include "f2c.h"
#ifdef NO_APPEND_FORTRAN
# define FNAME(x) x
#else
# define FNAME(x) x##_
#endif

typedef struct { float r, i; } f2c_complex;
typedef struct { double r, i; } f2c_doublecomplex;
/* typedef long int (*L_fp)(); */

extern int FNAME(dgeev)(char *jobvl, char *jobvr, integer *n,
                         double a[], integer *lda, double wr[], double wi[],
                         double vl[], integer *ldvl, double vr[], integer *ldvr,
                         double work[], integer lwork[], integer *info);
extern int FNAME(zgeev)(char *jobvl, char *jobvr, integer *n,
                         f2c_doublecomplex a[], integer *lda,
                         f2c_doublecomplex w[],
                         f2c_doublecomplex vl[], integer *ldvl,
                         f2c_doublecomplex vr[], integer *ldvr,
                         f2c_doublecomplex work[], integer *lwork,
                         double rwork[], integer *info);

extern int FNAME(dsyevd)(char *jobz, char *uplo, integer *n,
                          double a[], integer *lda, double w[], double work[],
                          integer *lwork, integer iwork[], integer *liwork, integer *info);
extern int FNAME(zheevd)(char *jobz, char *uplo, integer *n,
                          f2c_doublecomplex a[], integer *lda,
                          double w[], f2c_doublecomplex work[],
                          integer *lwork, double rwork[], integer *lrwork, integer iwork[],
                          integer *liwork, integer *info);

extern int FNAME(dgelsd)(integer *m, integer *n, integer *nrhs,
                          double a[], integer *lda, double b[], integer *ldb,
                          double s[], double *rcond, integer *rank,
                          double work[], integer *lwork, integer iwork[], integer *info);
extern int FNAME(zgelsd)(integer *m, integer *n, integer *nrhs,
                          f2c_doublecomplex a[], integer *lda,
                          f2c_doublecomplex b[], integer *ldb,
                          double s[], double *rcond, integer *rank,
                          f2c_doublecomplex work[], integer *lwork,
                          double rwork[], integer iwork[], integer *info);

extern int FNAME(dgesv)(integer *n, integer *nrhs,
                         double a[], integer *lda, integer ipiv[],
                         double b[], integer *ldb, integer *info);
extern int FNAME(zgesv)(integer *n, integer *nrhs,
                         f2c_doublecomplex a[], integer *lda, integer ipiv[],
                         f2c_doublecomplex b[], integer *ldb, integer *info);

extern int FNAME(dgetrf)(integer *m, integer *n,
                          double a[], integer *lda, integer ipiv[], integer *info);
extern int FNAME(zgetrf)(integer *m, integer *n,
                          f2c_doublecomplex a[], integer *lda, integer ipiv[],
                          integer *info);

extern int FNAME(dpotrf)(char *uplo, integer *n, double a[], integer *lda, integer *info);
extern int FNAME(zpotrf)(char *uplo, integer *n,
                          f2c_doublecomplex a[], integer *lda, integer *info);

extern int FNAME(dgesdd)(char *jobz, integer *m, integer *n,
                          double a[], integer *lda, double s[], double u[],
                          integer *ldu, double vt[], integer *ldvt, double work[],
                          integer *lwork, integer iwork[], integer *info);
extern int FNAME(zgesdd)(char *jobz, integer *m, integer *n,
                          f2c_doublecomplex a[], integer *lda,
                          double s[], f2c_doublecomplex u[], integer *ldu,
                          f2c_doublecomplex vt[], integer *ldvt,
                          f2c_doublecomplex work[], integer *lwork,
                          double rwork[], integer iwork[], integer *info);

extern int FNAME(dgeqrf)(integer *m, integer *n, double a[], integer *lda,
                          double tau[], double work[],
                          integer *lwork, integer *info);

extern int FNAME(zgeqrf)(integer *m, integer *n, f2c_doublecomplex a[], integer *lda,
                          f2c_doublecomplex tau[], f2c_doublecomplex work[],
                          integer *lwork, integer *info);

extern int FNAME(dorgqr)(integer *m, integer *n, integer *k, double a[], integer *lda,
                          double tau[], double work[],
                          integer *lwork, integer *info);

extern int FNAME(zungqr)(integer *m, integer *n, integer *k, f2c_doublecomplex a[],
                          integer *lda, f2c_doublecomplex tau[],
                          f2c_doublecomplex work[], integer *lwork, integer *info);

extern int FNAME(xerbla)(char *srname, integer *info);


