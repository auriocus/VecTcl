#include <tcl.h>
#include <stdio.h>
#include "f2c.h"

/*
  From the original manpage:
  --------------------------
  XERBLA is an error handler for the LAPACK routines.
  It is called by an LAPACK routine if an input parameter has an invalid value.
  A message is printed and execution stops.

  Instead of printing a message and stopping the execution, a
  ValueError is raised with the message.

  Parameters:
  -----------
  srname: Subroutine name to use in error message, maximum six characters.
          Spaces at the end are skipped.
  info: Number of the invalid parameter.
*/

/* Grmbl: need to pass on Tcl_Interp in a thread-safe way.
 * and return codes. Currently unable to do it until I munge on he lapack sources
 * myself */

int xerbla_(char *srname, integer *info)
{
        const char* format = "On entry to %.*s" \
                " parameter number %d had an illegal value";
        char buf[57 + 6 + 4]; /* 57 for strlen(format),
                                 6 for name, 4 for param. num. */

        int len = 0; /* length of subroutine name*/

        while( len<6 && srname[len]!='\0' )
                len++;
        while( len && srname[len-1]==' ' )
                len--;
		/* just print out the message to stderr */
		fprintf(stderr, format, len, srname, *info);
        return 0;
}
