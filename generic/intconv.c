#include "intconv.h"
#include <string.h>

const char digit_pairs[201] = {
  "00010203040506070809"
  "10111213141516171819"
  "20212223242526272829"
  "30313233343536373839"
  "40414243444546474849"
  "50515253545556575859"
  "60616263646566676869"
  "70717273747576777879"
  "80818283848586878889"
  "90919293949596979899"
};

#define MAXDIGITS 20

const uint64_t powersoften[MAXDIGITS] = {
	1u, 10u, 100, 1000, 10000, 100000,
	1000000u, 10000000, 100000000, 
	1000000000u, 10000000000, 100000000000, 
	1000000000000u, 10000000000000, 
	100000000000000u, 1000000000000000, 
	10000000000000000u, 100000000000000000, 
	1000000000000000000u, 10000000000000000000u
};

int format_uint64(uint64_t val, char *s)
{
    if(val==0)
    {
        s[0]='0';
		s[1]='\0';
        return 1;
    }

    int size=1;
    while (val >= powersoften[size] && size < MAXDIGITS) { size++; }
	
	char* c = s+size-1;
    while(val>=100)
    {
       int pos = val % 100;
       val /= 100;
       *(short*)(c-1)=*(short*)(digit_pairs+2*pos); 
       c-=2;
    }
    while(val>0)
    {
        *c--='0' + (val % 10);
        val /= 10;
    }
	s[size]='\0';
	return size;
} 

int format_int64(int64_t val, char *s) {
	if (val < 0) {
		s[0]='-';
		return format_uint64(-val, s+1)+1;
	} else {
		return format_uint64(val, s);
	}
}

int format_bool(int val, char *s) {
	if (val == 0) {
		strcpy(s, "false");
		return 5;
	} else {
		strcpy(s, "true");
		return 4;
	}	
}

/*
int main () {

	char out[NA_INTSPACE]; char out2[NA_INTSPACE];
	for (int i=-30; i<30; i++) {
		format_int64(i*100, out);
		format_uint64(i, out2);
		printf("%d %s %s\n", i, out, out2);
	}
	return 0;
} */
