	/* -*- c -*- */
	
	#include <tcl.h>
	#include <string.h>
	#include <stdlib.h>
	#include <ctype.h>
	#define SCOPE static

#line 1 "rde_critcl/util.h"

	#ifndef _RDE_UTIL_H
	#define _RDE_UTIL_H 1
	#ifndef SCOPE
	#define SCOPE
	#endif
	#define ALLOC(type)    (type *) ckalloc (sizeof (type))
	#define NALLOC(n,type) (type *) ckalloc ((n) * sizeof (type))
	#undef  RDE_DEBUG
	#define RDE_DEBUG 1
	#undef  RDE_TRACE
	#ifdef RDE_DEBUG
	#define STOPAFTER(x) { static int count = (x); count --; if (!count) { Tcl_Panic ("stop"); } }
	#define XSTR(x) #x
	#define STR(x) XSTR(x)
	#define RANGEOK(i,n) ((0 <= (i)) && (i < (n)))
	#define ASSERT(x,msg) if (!(x)) { Tcl_Panic (msg " (" #x "), in file " __FILE__ " @line " STR(__LINE__));}
	#define ASSERT_BOUNDS(i,n) ASSERT (RANGEOK(i,n),"array index out of bounds: " STR(i) " >= " STR(n))
	#else
	#define STOPAFTER(x)
	#define ASSERT(x,msg)
	#define ASSERT_BOUNDS(i,n)
	#endif
	#ifdef RDE_TRACE
	SCOPE void trace_enter (const char* fun);
	SCOPE void trace_return (const char *pat, ...);
	SCOPE void trace_printf (const char *pat, ...);
	#define ENTER(fun)          trace_enter (fun)
	#define RETURN(format,x)    trace_return (format,x) ; return x
	#define RETURNVOID          trace_return ("%s","(void)") ; return
	#define TRACE0(x)           trace_printf0 x
	#define TRACE(x)            trace_printf x
	#else
	#define ENTER(fun)
	#define RETURN(f,x) return x
	#define RETURNVOID  return
	#define TRACE0(x)
	#define TRACE(x)
	#endif
	#endif 
	

#line 1 "rde_critcl/stack.h"

	#ifndef _RDE_DS_STACK_H
	#define _RDE_DS_STACK_H 1
	typedef void (*RDE_STACK_CELL_FREE) (void* cell);
	typedef struct RDE_STACK_* RDE_STACK;
	static const int RDE_STACK_INITIAL_SIZE = 256;
	#endif 
	

#line 1 "rde_critcl/tc.h"

	#ifndef _RDE_DS_TC_H
	#define _RDE_DS_TC_H 1
	typedef struct RDE_TC_* RDE_TC;
	#endif 
	

#line 1 "rde_critcl/param.h"

	#ifndef _RDE_DS_PARAM_H
	#define _RDE_DS_PARAM_H 1
	typedef struct RDE_PARAM_* RDE_PARAM;
	typedef struct ERROR_STATE {
	    int       refCount;
	    long int  loc;
	    RDE_STACK msg; 
	} ERROR_STATE;
	typedef struct NC_STATE {
	    long int     CL;
	    long int     ST;
	    Tcl_Obj*     SV;
	    ERROR_STATE* ER;
	} NC_STATE;
	#endif 
	

#line 1 "rde_critcl/util.c"

	#ifdef RDE_TRACE
	typedef struct F_STACK {
	    const char*     str;
	    struct F_STACK* down;
	} F_STACK;
	static F_STACK* top   = 0;
	static int      level = 0;
	static void
	push (const char* str)
	{
	    F_STACK* new = ALLOC (F_STACK);
	    new->str = str;
	    new->down = top;
	    top = new;
	    level += 4;
	}
	static void
	pop (void)
	{
	    F_STACK* next = top->down;
	    level -= 4;
	    ckfree ((char*)top);
	    top = next;
	}
	static void
	indent (void)
	{
	    int i;
	    for (i = 0; i < level; i++) {
		fwrite(" ", 1, 1, stdout);
		fflush           (stdout);
	    }
	    if (top) {
		fwrite(top->str, 1, strlen(top->str), stdout);
		fflush                               (stdout);
	    }
	    fwrite(" ", 1, 1, stdout);
	    fflush           (stdout);
	}
	SCOPE void
	trace_enter (const char* fun)
	{
	    push (fun);
	    indent();
	    fwrite("ENTER\n", 1, 6, stdout);
	    fflush                 (stdout);
	}
	static char msg [1024*1024];
	SCOPE void
	trace_return (const char *pat, ...)
	{
	    int len;
	    va_list args;
	    indent();
	    fwrite("RETURN = ", 1, 9, stdout);
	    fflush                   (stdout);
	    va_start(args, pat);
	    len = vsprintf(msg, pat, args);
	    va_end(args);
	    msg[len++] = '\n';
	    msg[len] = '\0';
	    fwrite(msg, 1, len, stdout);
	    fflush             (stdout);
	    pop();
	}
	SCOPE void
	trace_printf (const char *pat, ...)
	{
	    int len;
	    va_list args;
	    indent();
	    va_start(args, pat);
	    len = vsprintf(msg, pat, args);
	    va_end(args);
	    msg[len++] = '\n';
	    msg[len] = '\0';
	    fwrite(msg, 1, len, stdout);
	    fflush             (stdout);
	}
	SCOPE void
	trace_printf0 (const char *pat, ...)
	{
	    int len;
	    va_list args;
	    va_start(args, pat);
	    len = vsprintf(msg, pat, args);
	    va_end(args);
	    msg[len++] = '\n';
	    msg[len] = '\0';
	    fwrite(msg, 1, len, stdout);
	    fflush             (stdout);
	}
	#endif
	

#line 1 "rde_critcl/stack.c"

	typedef struct RDE_STACK_ {
	    long int            max;   
	    long int            top;   
	    RDE_STACK_CELL_FREE freeCellProc; 
	    void**              cell;  
	} RDE_STACK_;
	
	SCOPE RDE_STACK
	rde_stack_new (RDE_STACK_CELL_FREE freeCellProc)
	{
	    RDE_STACK s = ALLOC (RDE_STACK_);
	    s->cell = NALLOC (RDE_STACK_INITIAL_SIZE, void*);
	    s->max  = RDE_STACK_INITIAL_SIZE;
	    s->top  = 0;
	    s->freeCellProc = freeCellProc;
	    return s;
	}
	SCOPE void
	rde_stack_del (RDE_STACK s)
	{
	    if (s->freeCellProc && s->top) {
		long int i;
		for (i=0; i < s->top; i++) {
		    ASSERT_BOUNDS(i,s->max);
		    s->freeCellProc ( s->cell [i] );
		}
	    }
	    ckfree ((char*) s->cell);
	    ckfree ((char*) s);
	}
	SCOPE void
	rde_stack_push (RDE_STACK s, void* item)
	{
	    if (s->top >= s->max) {
		long int new  = s->max ? (2 * s->max) : RDE_STACK_INITIAL_SIZE;
		void**   cell = (void**) ckrealloc ((char*) s->cell, new * sizeof(void*));
		ASSERT (cell,"Memory allocation failure for RDE stack");
		s->max  = new;
		s->cell = cell;
	    }
	    ASSERT_BOUNDS(s->top,s->max);
	    s->cell [s->top] = item;
	    s->top ++;
	}
	SCOPE void*
	rde_stack_top (RDE_STACK s)
	{
	    ASSERT_BOUNDS(s->top-1,s->max);
	    return s->cell [s->top - 1];
	}
	SCOPE void
	rde_stack_pop (RDE_STACK s, long int n)
	{
	    ASSERT (n >= 0, "Bad pop count");
	    if (n == 0) return;
	    if (s->freeCellProc) {
		while (n) {
		    s->top --;
		    ASSERT_BOUNDS(s->top,s->max);
		    s->freeCellProc ( s->cell [s->top] );
		    n --;
		}
	    } else {
		s->top -= n;
	    }
	}
	SCOPE void
	rde_stack_trim (RDE_STACK s, long int n)
	{
	    ASSERT (n >= 0, "Bad trimsize");
	    if (s->freeCellProc) {
		while (s->top > n) {
		    s->top --;
		    ASSERT_BOUNDS(s->top,s->max);
		    s->freeCellProc ( s->cell [s->top] );
		}
	    } else {
		s->top = n;
	    }
	}
	SCOPE void
	rde_stack_drop (RDE_STACK s, long int n)
	{
	    ASSERT (n >= 0, "Bad pop count");
	    if (n == 0) return;
	    s->top -= n;
	}
	SCOPE void
	rde_stack_move (RDE_STACK dst, RDE_STACK src)
	{
	    ASSERT (dst->freeCellProc == src->freeCellProc, "Ownership mismatch");
	    
	    while (src->top > 0) {
		src->top --;
		ASSERT_BOUNDS(src->top,src->max);
		rde_stack_push (dst, src->cell [src->top] );
	    }
	}
	SCOPE void
	rde_stack_get (RDE_STACK s, long int* cn, void*** cc)
	{
	    *cn = s->top;
	    *cc = s->cell;
	}
	SCOPE long int
	rde_stack_size (RDE_STACK s)
	{
	    return s->top;
	}
	

#line 1 "rde_critcl/tc.c"

	typedef struct RDE_TC_ {
	    int       max;   
	    int       num;   
	    char*     str;   
	    RDE_STACK off;   
	} RDE_TC_;
	
	SCOPE RDE_TC
	rde_tc_new (void)
	{
	    RDE_TC tc = ALLOC (RDE_TC_);
	    tc->max   = RDE_STACK_INITIAL_SIZE;
	    tc->num   = 0;
	    tc->str   = NALLOC (RDE_STACK_INITIAL_SIZE, char);
	    tc->off   = rde_stack_new (NULL);
	    return tc;
	}
	SCOPE void
	rde_tc_del (RDE_TC tc)
	{
	    rde_stack_del (tc->off);
	    ckfree (tc->str);
	    ckfree ((char*) tc);
	}
	SCOPE long int
	rde_tc_size (RDE_TC tc)
	{
	    return rde_stack_size (tc->off);
	}
	SCOPE void
	rde_tc_clear (RDE_TC tc)
	{
	    tc->num   = 0;
	    rde_stack_trim (tc->off,  0);
	}
	SCOPE char*
	rde_tc_append (RDE_TC tc, char* string, long int len)
	{
	    long int base = tc->num;
	    long int off  = tc->num;
	    char* ch;
	    int clen;
	    Tcl_UniChar uni;
	    if (len < 0) {
		len = strlen (ch);
	    }
	    
	    if ((tc->num + len) >= tc->max) {
		int   new = len + (tc->max ? (2 * tc->max) : RDE_STACK_INITIAL_SIZE);
		char* str = ckrealloc (tc->str, new * sizeof(char));
		ASSERT (str,"Memory allocation failure for token character array");
		tc->max = new;
		tc->str = str;
	    }
	    tc->num += len;
	    ASSERT_BOUNDS(tc->num,tc->max);
	    ASSERT_BOUNDS(off,tc->max);
	    ASSERT_BOUNDS(off+len-1,tc->max);
	    ASSERT_BOUNDS(off+len-1,tc->num);
	    memcpy (tc->str + off, string, len);
	    
	    ch = string;
	    while (ch < (string + len)) {
		ASSERT_BOUNDS(off,tc->num);
		rde_stack_push (tc->off,  (void*) off);
		clen = Tcl_UtfToUniChar (ch, &uni);
		off += clen;
		ch  += clen;
	    }
	    return tc->str + base;
	}
	SCOPE void
	rde_tc_get (RDE_TC tc, int at, char** ch, long int* len)
	{
	    long int  oc, off, top, end;
	    long int* ov;
	    rde_stack_get (tc->off, &oc, (void***) &ov);
	    ASSERT_BOUNDS(at,oc);
	    off = ov [at];
	    if ((at+1) == oc) {
		end = tc->num;
	    } else {
		end = ov [at+1];
	    }
	    TRACE (("rde_tc_get (RDE_TC %p, @ %d) => %d.[%d ... %d]/%d",tc,at,end-off,off,end-1,tc->num));
	    ASSERT_BOUNDS(off,tc->num);
	    ASSERT_BOUNDS(end-1,tc->num);
	    *ch = tc->str + off;
	    *len = end - off;
	}
	SCOPE void
	rde_tc_get_s (RDE_TC tc, int at, int last, char** ch, long int* len)
	{
	    long int  oc, off, top, end;
	    long int* ov;
	    rde_stack_get (tc->off, &oc, (void***) &ov);
	    ASSERT_BOUNDS(at,oc);
	    ASSERT_BOUNDS(last,oc);
	    off = ov [at];
	    if ((last+1) == oc) {
		end = tc->num;
	    } else {
		end = ov [last+1];
	    }
	    TRACE (("rde_tc_get_s (RDE_TC %p, @ %d .. %d) => %d.[%d ... %d]/%d",tc,at,last,end-off,off,end-1,tc->num));
	    ASSERT_BOUNDS(off,tc->num);
	    ASSERT_BOUNDS(end-1,tc->num);
	    *ch = tc->str + off;
	    *len = end - off;
	}
	

#line 1 "rde_critcl/param.c"

	typedef struct RDE_PARAM_ {
	    Tcl_Channel   IN;
	    Tcl_Obj*      readbuf;
	    char*         CC; 
	    long int      CC_len;
	    RDE_TC        TC;
	    long int      CL;
	    RDE_STACK     LS; 
	    ERROR_STATE*  ER;
	    RDE_STACK     ES; 
	    long int      ST;
	    Tcl_Obj*      SV;
	    Tcl_HashTable NC;
	    
	    RDE_STACK    ast  ; 
	    RDE_STACK    mark ; 
	    
	    long int numstr; 
	    char**  string;
	    
	    ClientData clientData;
	} RDE_PARAM_;
	typedef int (*UniCharClass) (int);
	typedef enum test_class_id {
	    tc_alnum,
	    tc_alpha,
	    tc_ascii,
	    tc_ddigit,
	    tc_digit,
	    tc_graph,
	    tc_lower,
	    tc_printable,
	    tc_punct,
	    tc_space,
	    tc_upper,
	    tc_wordchar,
	    tc_xdigit
	} test_class_id;
	static void ast_node_free    (void* n);
	static void error_state_free (void* es);
	static void error_set        (RDE_PARAM p, int s);
	static void nc_clear         (RDE_PARAM p);
	static int UniCharIsAscii    (int character);
	static int UniCharIsHexDigit (int character);
	static int UniCharIsDecDigit (int character);
	static void test_class (RDE_PARAM p, UniCharClass class, test_class_id id);
	static int  er_int_compare (const void* a, const void* b);
	#define SV_INIT(p)             \
	    p->SV = NULL; \
	    TRACE (("SV_INIT (%p => %p)", (p), (p)->SV))
	#define SV_SET(p,newsv)             \
	    if (((p)->SV) != (newsv)) { \
	        TRACE (("SV_CLEAR/set (%p => %p)", (p), (p)->SV)); \
	        if ((p)->SV) {                  \
		    Tcl_DecrRefCount ((p)->SV); \
	        }				    \
	        (p)->SV = (newsv);		    \
	        TRACE (("SV_SET       (%p => %p)", (p), (p)->SV)); \
	        if ((p)->SV) {                  \
		    Tcl_IncrRefCount ((p)->SV); \
	        } \
	    }
	#define SV_CLEAR(p)                 \
	    TRACE (("SV_CLEAR (%p => %p)", (p), (p)->SV)); \
	    if ((p)->SV) {                  \
		Tcl_DecrRefCount ((p)->SV); \
	    }				    \
	    (p)->SV = NULL
	#define ER_INIT(p)             \
	    p->ER = NULL; \
	    TRACE (("ER_INIT (%p => %p)", (p), (p)->ER))
	#define ER_CLEAR(p)             \
	    error_state_free ((p)->ER);	\
	    (p)->ER = NULL
	SCOPE RDE_PARAM
	rde_param_new (long int nstr, char** strings)
	{
	    RDE_PARAM p;
	    ENTER ("rde_param_new");
	    TRACE (("\tINT %d strings @ %p", nstr, strings));
	    p = ALLOC (RDE_PARAM_);
	    p->numstr = nstr;
	    p->string = strings;
	    p->readbuf = Tcl_NewObj ();
	    Tcl_IncrRefCount (p->readbuf);
	    TRACE (("\tTcl_Obj* readbuf %p used %d", p->readbuf,p->readbuf->refCount));
	    Tcl_InitHashTable (&p->NC, TCL_ONE_WORD_KEYS);
	    p->IN   = NULL;
	    p->CL   = -1;
	    p->ST   = 0;
	    ER_INIT (p);
	    SV_INIT (p);
	    p->CC   = NULL;
	    p->CC_len = 0;
	    p->TC   = rde_tc_new ();
	    p->ES   = rde_stack_new (error_state_free);
	    p->LS   = rde_stack_new (NULL);
	    p->ast  = rde_stack_new (ast_node_free);
	    p->mark = rde_stack_new (NULL);
	    RETURN ("%p", p);
	}
	SCOPE void 
	rde_param_del (RDE_PARAM p)
	{
	    ENTER ("rde_param_del");
	    TRACE (("RDE_PARAM %p",p));
	    ER_CLEAR (p);                 TRACE (("\ter_clear"));
	    SV_CLEAR (p);                 TRACE (("\tsv_clear"));
	    nc_clear (p);                 TRACE (("\tnc_clear"));
	    Tcl_DeleteHashTable (&p->NC); TRACE (("\tnc hashtable delete"));
	    rde_tc_del    (p->TC);        TRACE (("\ttc clear"));
	    rde_stack_del (p->ES);        TRACE (("\tes clear"));
	    rde_stack_del (p->LS);        TRACE (("\tls clear"));
	    rde_stack_del (p->ast);       TRACE (("\tast clear"));
	    rde_stack_del (p->mark);      TRACE (("\tmark clear"));
	    TRACE (("\tTcl_Obj* readbuf %p used %d", p->readbuf,p->readbuf->refCount));
	    Tcl_DecrRefCount (p->readbuf);
	    ckfree ((char*) p);
	    RETURNVOID;
	}
	SCOPE void 
	rde_param_reset (RDE_PARAM p, Tcl_Channel chan)
	{
	    ENTER ("rde_param_reset");
	    TRACE (("RDE_PARAM   %p",p));
	    TRACE (("Tcl_Channel %p",chan));
	    p->IN  = chan;
	    p->CL  = -1;
	    p->ST  = 0;
	    p->CC  = NULL;
	    p->CC_len = 0;
	    ER_CLEAR (p);
	    SV_CLEAR (p);
	    nc_clear (p);
	    rde_tc_clear   (p->TC);
	    rde_stack_trim (p->ES,   0);
	    rde_stack_trim (p->LS,   0);
	    rde_stack_trim (p->ast,  0);
	    rde_stack_trim (p->mark, 0);
	    TRACE (("\tTcl_Obj* readbuf %p used %d", p->readbuf,p->readbuf->refCount));
	    RETURNVOID;
	}
	SCOPE void
	rde_param_update_strings (RDE_PARAM p, long int nstr, char** strings)
	{
	    ENTER ("rde_param_update_strings");
	    TRACE (("RDE_PARAM %p", p));
	    TRACE (("INT       %d strings", nstr));
	    p->numstr = nstr;
	    p->string = strings;
	    RETURNVOID;
	}
	SCOPE void
	rde_param_data (RDE_PARAM p, char* buf, long int len)
	{
	    (void) rde_tc_append (p->TC, buf, len);
	}
	SCOPE void
	rde_param_clientdata (RDE_PARAM p, ClientData clientData)
	{
	    p->clientData = clientData;
	}
	static void
	nc_clear (RDE_PARAM p)
	{
	    Tcl_HashSearch hs;
	    Tcl_HashEntry* he;
	    Tcl_HashTable* tablePtr;
	    for(he = Tcl_FirstHashEntry(&p->NC, &hs);
		he != NULL;
		he = Tcl_FirstHashEntry(&p->NC, &hs)) {
		Tcl_HashSearch hsc;
		Tcl_HashEntry* hec;
		tablePtr = (Tcl_HashTable*) Tcl_GetHashValue (he);
		for(hec = Tcl_FirstHashEntry(tablePtr, &hsc);
		    hec != NULL;
		    hec = Tcl_NextHashEntry(&hsc)) {
		    NC_STATE* scs = Tcl_GetHashValue (hec);
		    error_state_free (scs->ER);
		    if (scs->SV) { Tcl_DecrRefCount (scs->SV); }
		    ckfree ((char*) scs);
		}
		Tcl_DeleteHashTable (tablePtr);
		ckfree ((char*) tablePtr);
		Tcl_DeleteHashEntry (he);
	    }
	}
	SCOPE ClientData
	rde_param_query_clientdata (RDE_PARAM p)
	{
	    return p->clientData;
	}
	SCOPE void
	rde_param_query_amark (RDE_PARAM p, long int* mc, long int** mv)
	{
	    rde_stack_get (p->mark, mc, (void***) mv);
	}
	SCOPE void
	rde_param_query_ast (RDE_PARAM p, long int* ac, Tcl_Obj*** av)
	{
	    rde_stack_get (p->ast, ac, (void***) av);
	}
	SCOPE const char*
	rde_param_query_in (RDE_PARAM p)
	{
	    return p->IN
		? Tcl_GetChannelName (p->IN)
		: "";
	}
	SCOPE const char*
	rde_param_query_cc (RDE_PARAM p, long int* len)
	{
	    *len = p->CC_len;
	    return p->CC;
	}
	SCOPE int
	rde_param_query_cl (RDE_PARAM p)
	{
	    return p->CL;
	}
	SCOPE const ERROR_STATE*
	rde_param_query_er (RDE_PARAM p)
	{
	    return p->ER;
	}
	SCOPE Tcl_Obj*
	rde_param_query_er_tcl (RDE_PARAM p, const ERROR_STATE* er)
	{
	    Tcl_Obj* res;
	    if (!er) {
		
		res = Tcl_NewStringObj ("", 0);
	    } else {
		Tcl_Obj* ov [2];
		Tcl_Obj** mov;
		long int  mc, i, j;
		long int* mv;
		int lastid;
		const char* msg;
		rde_stack_get (er->msg, &mc, (void***) &mv);
		
		qsort (mv, mc, sizeof (long int), er_int_compare);
		
		mov = NALLOC (mc, Tcl_Obj*);
		lastid = -1;
		for (i=0, j=0; i < mc; i++) {
		    ASSERT_BOUNDS (i,mc);
		    if (mv [i] == lastid) continue;
		    lastid = mv [i];
		    ASSERT_BOUNDS(mv[i],p->numstr);
		    msg = p->string [mv[i]]; 
		    ASSERT_BOUNDS (j,mc);
		    mov [j] = Tcl_NewStringObj (msg, -1);
		    j++;
		}
		
		ov [0] = Tcl_NewIntObj  (er->loc);
		ov [1] = Tcl_NewListObj (j, mov);
		res = Tcl_NewListObj (2, ov);
		ckfree ((char*) mov);
	    }
	    return res;
	}
	SCOPE void
	rde_param_query_es (RDE_PARAM p, long int* ec, ERROR_STATE*** ev)
	{
	    rde_stack_get (p->ES, ec, (void***) ev);
	}
	SCOPE void
	rde_param_query_ls (RDE_PARAM p, long int* lc, long int** lv)
	{
	    rde_stack_get (p->LS, lc, (void***) lv);
	}
	SCOPE Tcl_HashTable*
	rde_param_query_nc (RDE_PARAM p)
	{
	    return &p->NC;
	}
	SCOPE int
	rde_param_query_st (RDE_PARAM p)
	{
	    return p->ST;
	}
	SCOPE Tcl_Obj*
	rde_param_query_sv (RDE_PARAM p)
	{
	    TRACE (("SV_QUERY %p => (%p)", (p), (p)->SV)); \
	    return p->SV;
	}
	SCOPE long int
	rde_param_query_tc_size (RDE_PARAM p)
	{
	    return rde_tc_size (p->TC);
	}
	SCOPE void
	rde_param_query_tc_get_s (RDE_PARAM p, long int at, long int last, char** ch, long int* len)
	{
	    rde_tc_get_s (p->TC, at, last, ch, len);
	}
	SCOPE const char*
	rde_param_query_string (RDE_PARAM p, long int id)
	{
	    TRACE (("rde_param_query_string (RDE_PARAM %p, %d/%d)", p, id, p->numstr));
	    ASSERT_BOUNDS(id,p->numstr);
	    return p->string [id];
	}
	SCOPE void
	rde_param_i_ast_pop_discard (RDE_PARAM p)
	{
	    rde_stack_pop (p->mark, 1);
	}
	SCOPE void
	rde_param_i_ast_pop_rewind (RDE_PARAM p)
	{
	    long int trim = (long int) rde_stack_top (p->mark);
	    ENTER ("rde_param_i_ast_pop_rewind");
	    TRACE (("RDE_PARAM %p",p));
	    rde_stack_pop  (p->mark, 1);
	    rde_stack_trim (p->ast, (int) trim);
	    TRACE (("SV = (%p rc%d '%s')",
		    p->SV,
		    p->SV ? p->SV->refCount       : -1,
		    p->SV ? Tcl_GetString (p->SV) : ""));
	    RETURNVOID;
	}
	SCOPE void
	rde_param_i_ast_rewind (RDE_PARAM p)
	{
	    long int trim = (long int) rde_stack_top (p->mark);
	    ENTER ("rde_param_i_ast_rewind");
	    TRACE (("RDE_PARAM %p",p));
	    rde_stack_trim (p->ast, (int) trim);
	    TRACE (("SV = (%p rc%d '%s')",
		    p->SV,
		    p->SV ? p->SV->refCount       : -1,
		    p->SV ? Tcl_GetString (p->SV) : ""));
	    RETURNVOID;
	}
	SCOPE void
	rde_param_i_ast_push (RDE_PARAM p)
	{
	    rde_stack_push (p->mark, (void*) rde_stack_size (p->ast));
	}
	SCOPE void
	rde_param_i_ast_value_push (RDE_PARAM p)
	{
	    ENTER ("rde_param_i_ast_value_push");
	    TRACE (("RDE_PARAM %p",p));
	    ASSERT(p->SV,"Unable to push undefined semantic value");
	    TRACE (("rde_param_i_ast_value_push %p => (%p)", p, p->SV));
	    TRACE (("SV = (%p rc%d '%s')", p->SV, p->SV->refCount, Tcl_GetString (p->SV)));
	    rde_stack_push (p->ast, p->SV);
	    Tcl_IncrRefCount (p->SV);
	    RETURNVOID;
	}
	static void
	ast_node_free (void* n)
	{
	    Tcl_DecrRefCount ((Tcl_Obj*) n);
	}
	SCOPE void
	rde_param_i_error_clear (RDE_PARAM p)
	{
	    ER_CLEAR (p);
	}
	SCOPE void
	rde_param_i_error_nonterminal (RDE_PARAM p, int s)
	{
	    long int pos;
	    if (!p->ER) return;
	    pos = 1 + (long int) rde_stack_top (p->LS);
	    if (p->ER->loc != pos) return;
	    error_set (p, s);
	    p->ER->loc = pos;
	}
	SCOPE void
	rde_param_i_error_pop_merge (RDE_PARAM p)
	{
	    ERROR_STATE* top = (ERROR_STATE*) rde_stack_top (p->ES);
	    
	    if (top == p->ER) {
		rde_stack_pop (p->ES, 1);
		return;
	    }
	    
	    if (!top) {
		rde_stack_pop (p->ES, 1);
		return;
	    }
	    
	    if (!p->ER) {
		rde_stack_drop (p->ES, 1);
		p->ER = top;
		
		return;
	    }
	    
	    if (top->loc < p->ER->loc) {
		rde_stack_pop (p->ES, 1);
		return;
	    }
	    
	    if (top->loc > p->ER->loc) {
		rde_stack_drop (p->ES, 1);
		error_state_free (p->ER);
		p->ER = top;
		
		return;
	    }
	    
	    rde_stack_move (p->ER->msg, top->msg);
	    rde_stack_pop  (p->ES, 1);
	}
	SCOPE void
	rde_param_i_error_push (RDE_PARAM p)
	{
	    rde_stack_push (p->ES, p->ER);
	    if (p->ER) { p->ER->refCount ++; }
	}
	static void
	error_set (RDE_PARAM p, int s)
	{
	    error_state_free (p->ER);
	    p->ER = ALLOC (ERROR_STATE);
	    p->ER->refCount = 1;
	    p->ER->loc      = p->CL;
	    p->ER->msg      = rde_stack_new (NULL);
	    ASSERT_BOUNDS(s,p->numstr);
	    rde_stack_push (p->ER->msg, (void*) s);
	}
	static void
	error_state_free (void* esx)
	{
	    ERROR_STATE* es = esx;
	    if (!es) return;
	    es->refCount --;
	    if (es->refCount > 0) return;
	    rde_stack_del (es->msg);
	    ckfree ((char*) es);
	}
	SCOPE void
	rde_param_i_loc_pop_discard (RDE_PARAM p)
	{
	    rde_stack_pop (p->LS, 1);
	}
	SCOPE void
	rde_param_i_loc_pop_rewind (RDE_PARAM p)
	{
	    p->CL = (long int) rde_stack_top (p->LS);
	    rde_stack_pop (p->LS, 1);
	}
	SCOPE void
	rde_param_i_loc_push (RDE_PARAM p)
	{
	    rde_stack_push (p->LS, (void*) p->CL);
	}
	SCOPE void
	rde_param_i_loc_rewind (RDE_PARAM p)
	{
	    p->CL = (long int) rde_stack_top (p->LS);
	}
	SCOPE void
	rde_param_i_input_next (RDE_PARAM p, int m)
	{
	    int leni;
	    char* ch;
	    ASSERT_BOUNDS(m,p->numstr);
	    p->CL ++;
	    if (p->CL < rde_tc_size (p->TC)) {
		
		rde_tc_get (p->TC, p->CL, &p->CC, &p->CC_len);
		ASSERT_BOUNDS (p->CC_len, TCL_UTF_MAX);
		p->ST = 1;
		ER_CLEAR (p);
		return;
	    }
	    if (!p->IN || 
		Tcl_Eof (p->IN) ||
		(Tcl_ReadChars (p->IN, p->readbuf, 1, 0) <= 0)) {
		
		p->ST = 0;
		error_set (p, m);
		return;
	    }
	    
	    ch = Tcl_GetStringFromObj (p->readbuf, &leni);
	    ASSERT_BOUNDS (leni, TCL_UTF_MAX);
	    p->CC = rde_tc_append (p->TC, ch, leni);
	    p->CC_len = leni;
	    p->ST = 1;
	    ER_CLEAR (p);
	}
	SCOPE void
	rde_param_i_status_fail (RDE_PARAM p)
	{
	    p->ST = 0;
	}
	SCOPE void
	rde_param_i_status_ok (RDE_PARAM p)
	{
	    p->ST = 1;
	}
	SCOPE void
	rde_param_i_status_negate (RDE_PARAM p)
	{
	    p->ST = !p->ST;
	}
	SCOPE int 
	rde_param_i_symbol_restore (RDE_PARAM p, int s)
	{
	    NC_STATE*      scs;
	    Tcl_HashEntry* hPtr;
	    Tcl_HashTable* tablePtr;
	    
	    hPtr = Tcl_FindHashEntry (&p->NC, (char*) p->CL);
	    if (!hPtr) { return 0; }
	    tablePtr = (Tcl_HashTable*) Tcl_GetHashValue (hPtr);
	    hPtr = Tcl_FindHashEntry (tablePtr, (char*) s);
	    if (!hPtr) { return 0; }
	    
	    scs = Tcl_GetHashValue (hPtr);
	    p->CL = scs->CL;
	    p->ST = scs->ST;
	    error_state_free (p->ER);
	    p->ER = scs->ER;
	    if (p->ER) { p->ER->refCount ++; }
	    TRACE (("SV_RESTORE (%p) '%s'",scs->SV, scs->SV ? Tcl_GetString (scs->SV):""));
	    SV_SET (p, scs->SV);
	    return 1;
	}
	SCOPE void
	rde_param_i_symbol_save (RDE_PARAM p, int s)
	{
	    long int       at = (long int) rde_stack_top (p->LS);
	    NC_STATE*      scs;
	    Tcl_HashEntry* hPtr;
	    Tcl_HashTable* tablePtr;
	    int            isnew;
	    ENTER ("rde_param_i_symbol_save");
	    TRACE (("RDE_PARAM %p",p));
	    TRACE (("INT       %d",s));
	    
	    hPtr = Tcl_CreateHashEntry (&p->NC, (char*) at, &isnew);
	    if (isnew) {
		tablePtr = ALLOC (Tcl_HashTable);
		Tcl_InitHashTable (tablePtr, TCL_ONE_WORD_KEYS);
		Tcl_SetHashValue (hPtr, tablePtr);
	    } else {
		tablePtr = (Tcl_HashTable*) Tcl_GetHashValue (hPtr);
	    }
	    hPtr = Tcl_CreateHashEntry (tablePtr, (char*) s, &isnew);
	    if (isnew) {
		
		scs = ALLOC (NC_STATE);
		scs->CL = p->CL;
		scs->ST = p->ST;
		TRACE (("SV_CACHE (%p '%s')", p->SV, p->SV ? Tcl_GetString(p->SV) : ""));
		scs->SV = p->SV;
		if (scs->SV) { Tcl_IncrRefCount (scs->SV); }
		scs->ER = p->ER;
		if (scs->ER) { scs->ER->refCount ++; }
		Tcl_SetHashValue (hPtr, scs);
	    } else {
		
		scs = (NC_STATE*) Tcl_GetHashValue (hPtr);
		scs->CL = p->CL;
		scs->ST = p->ST;
		TRACE (("SV_CACHE/over (%p '%s')", p->SV, p->SV ? Tcl_GetString(p->SV) : "" ));
		if (scs->SV) { Tcl_DecrRefCount (scs->SV); }
		scs->SV = p->SV;
		if (scs->SV) { Tcl_IncrRefCount (scs->SV); }
		error_state_free (scs->ER);
		scs->ER = p->ER;
		if (scs->ER) { scs->ER->refCount ++; }
	    }
	    TRACE (("SV = (%p rc%d '%s')",
		    p->SV,
		    p->SV ? p->SV->refCount       : -1,
		    p->SV ? Tcl_GetString (p->SV) : ""));
	    RETURNVOID;
	}
	SCOPE void
	rde_param_i_test_alnum (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsAlnum, tc_alnum);
	}
	SCOPE void
	rde_param_i_test_alpha (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsAlpha, tc_alpha);
	}
	SCOPE void
	rde_param_i_test_ascii (RDE_PARAM p)
	{
	    test_class (p, UniCharIsAscii, tc_ascii);
	}
	SCOPE void
	rde_param_i_test_char (RDE_PARAM p, char* c, int msg)
	{
	    ASSERT_BOUNDS(msg,p->numstr);
	    p->ST = Tcl_UtfNcmp (p->CC, c, 1) == 0;
	    if (p->ST) {
		ER_CLEAR (p);
	    } else {
		error_set (p, msg);
		p->CL --;
	    }
	}
	SCOPE void
	rde_param_i_test_ddigit (RDE_PARAM p)
	{
	    test_class (p, UniCharIsDecDigit, tc_ddigit);
	}
	SCOPE void
	rde_param_i_test_digit (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsDigit, tc_digit);
	}
	SCOPE void
	rde_param_i_test_graph (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsGraph, tc_graph);
	}
	SCOPE void
	rde_param_i_test_lower (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsLower, tc_lower);
	}
	SCOPE void
	rde_param_i_test_print (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsPrint, tc_printable);
	}
	SCOPE void
	rde_param_i_test_punct (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsPunct, tc_punct);
	}
	SCOPE void
	rde_param_i_test_range (RDE_PARAM p, char* s, char* e, int msg)
	{
	    ASSERT_BOUNDS(msg,p->numstr);
	    p->ST =
		(Tcl_UtfNcmp (s, p->CC, 1) <= 0) &&
		(Tcl_UtfNcmp (p->CC, e, 1) <= 0);
	    if (p->ST) {
		ER_CLEAR (p);
	    } else {
		error_set (p, msg);
		p->CL --;
	    }
	}
	SCOPE void
	rde_param_i_test_space (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsSpace, tc_space);
	}
	SCOPE void
	rde_param_i_test_upper (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsUpper, tc_upper);
	}
	SCOPE void
	rde_param_i_test_wordchar (RDE_PARAM p)
	{
	    test_class (p, Tcl_UniCharIsWordChar, tc_wordchar);
	}
	SCOPE void
	rde_param_i_test_xdigit (RDE_PARAM p)
	{
	    test_class (p, UniCharIsHexDigit, tc_xdigit);
	}
	static void
	test_class (RDE_PARAM p, UniCharClass class, test_class_id id)
	{
	    Tcl_UniChar ch;
	    Tcl_UtfToUniChar(p->CC, &ch);
	    ASSERT_BOUNDS(id,p->numstr);
	    p->ST = !!class (ch);
	    
	    if (p->ST) {
		ER_CLEAR (p);
	    } else {
		error_set (p, id);
		p->CL --;
	    }
	}
	static int
	UniCharIsAscii (int character)
	{
	    return (character >= 0) && (character < 0x80);
	}
	static int
	UniCharIsHexDigit (int character)
	{
	    return (character >= 0) && (character < 0x80) && isxdigit(character);
	}
	static int
	UniCharIsDecDigit (int character)
	{
	    return (character >= 0) && (character < 0x80) && isdigit(character);
	}
	SCOPE void
	rde_param_i_value_clear (RDE_PARAM p)
	{
	    SV_CLEAR (p);
	}
	SCOPE void
	rde_param_i_value_leaf (RDE_PARAM p, int s)
	{
	    Tcl_Obj* newsv;
	    Tcl_Obj* ov [3];
	    long int pos = 1 + (long int) rde_stack_top (p->LS);
	    ASSERT_BOUNDS(s,p->numstr);
	    ov [0] = Tcl_NewStringObj (p->string[s], -1);
	    ov [1] = Tcl_NewIntObj (pos);
	    ov [2] = Tcl_NewIntObj (p->CL);
	    newsv = Tcl_NewListObj (3, ov);
	    TRACE (("rde_param_i_value_leaf => '%s'",Tcl_GetString (newsv)));
	    SV_SET (p, newsv);
	}
	SCOPE void
	rde_param_i_value_reduce (RDE_PARAM p, int s)
	{
	    Tcl_Obj*  newsv;
	    int       oc, i, j;
	    Tcl_Obj** ov;
	    long int  ac;
	    Tcl_Obj** av;
	    long int pos   = 1 + (long int) rde_stack_top (p->LS);
	    long int mark  = (long int) rde_stack_top (p->mark);
	    long int asize = rde_stack_size (p->ast);
	    long int new   = asize - mark;
	    ASSERT (new >= 0, "Bad number of elements to reduce");
	    ov = NALLOC (3+new, Tcl_Obj*);
	    ASSERT_BOUNDS(s,p->numstr);
	    ov [0] = Tcl_NewStringObj (p->string[s], -1);
	    ov [1] = Tcl_NewIntObj (pos);
	    ov [2] = Tcl_NewIntObj (p->CL);
	    rde_stack_get (p->ast, &ac, (void***) &av);
	    for (i = 3, j = mark; j < asize; i++, j++) {
		ASSERT_BOUNDS (i, 3+new);
		ASSERT_BOUNDS (j, ac);
		ov [i] = av [j];
	    }
	    ASSERT (i == 3+new, "Reduction result incomplete");
	    newsv = Tcl_NewListObj (3+new, ov);
	    TRACE (("rde_param_i_value_reduce => '%s'",Tcl_GetString (newsv)));
	    SV_SET (p, newsv);
	    ckfree ((char*) ov);
	}
	static int
	er_int_compare (const void* a, const void* b)
	{
	    long int ai = *((long int*) a);
	    long int bi = *((long int*) b);
	    if (ai < bi) { return -1; }
	    if (ai > bi) { return  1; }
	    return 0;
	}
	SCOPE int
	rde_param_i_symbol_start (RDE_PARAM p, int s)
	{
	    if (rde_param_i_symbol_restore (p, s)) {
		if (p->ST) {
		    rde_stack_push (p->ast, p->SV);
		    Tcl_IncrRefCount (p->SV);
		}
		return 1;
	    }
	    rde_stack_push (p->LS, (void*) p->CL);
	    return 0;
	}
	SCOPE int
	rde_param_i_symbol_start_d (RDE_PARAM p, int s)
	{
	    if (rde_param_i_symbol_restore (p, s)) {
		if (p->ST) {
		    rde_stack_push (p->ast, p->SV);
		    Tcl_IncrRefCount (p->SV);
		}
		return 1;
	    }
	    rde_stack_push (p->LS,   (void*) p->CL);
	    rde_stack_push (p->mark, (void*) rde_stack_size (p->ast));
	    return 0;
	}
	SCOPE int
	rde_param_i_symbol_void_start (RDE_PARAM p, int s)
	{
	    if (rde_param_i_symbol_restore (p, s)) return 1;
	    rde_stack_push (p->LS, (void*) p->CL);
	    return 0;
	}
	SCOPE int
	rde_param_i_symbol_void_start_d (RDE_PARAM p, int s)
	{
	    if (rde_param_i_symbol_restore (p, s)) return 1;
	    rde_stack_push (p->LS,   (void*) p->CL);
	    rde_stack_push (p->mark, (void*) rde_stack_size (p->ast));
	    return 0;
	}
	SCOPE void
	rde_param_i_symbol_done_d_reduce (RDE_PARAM p, int s, int m)
	{
	    if (p->ST) {
		rde_param_i_value_reduce (p, s);
	    } else {
		SV_CLEAR (p);
	    }
	    rde_param_i_symbol_save       (p, s);
	    rde_param_i_error_nonterminal (p, m);
	    rde_param_i_ast_pop_rewind    (p);
	    rde_stack_pop (p->LS, 1);
	    if (p->ST) {
		rde_stack_push (p->ast, p->SV);
		Tcl_IncrRefCount (p->SV);
	    }
	}
	SCOPE void
	rde_param_i_symbol_done_leaf (RDE_PARAM p, int s, int m)
	{
	    if (p->ST) {
		rde_param_i_value_leaf (p, s);
	    } else {
		SV_CLEAR (p);
	    }
	    rde_param_i_symbol_save       (p, s);
	    rde_param_i_error_nonterminal (p, m);
	    rde_stack_pop (p->LS, 1);
	    if (p->ST) {
		rde_stack_push (p->ast, p->SV);
		Tcl_IncrRefCount (p->SV);
	    }
	}
	SCOPE void
	rde_param_i_symbol_done_d_leaf (RDE_PARAM p, int s, int m)
	{
	    if (p->ST) {
		rde_param_i_value_leaf (p, s);
	    } else {
		SV_CLEAR (p);
	    }
	    rde_param_i_symbol_save       (p, s);
	    rde_param_i_error_nonterminal (p, m);
	    rde_param_i_ast_pop_rewind    (p);
	    rde_stack_pop (p->LS, 1);
	    if (p->ST) {
		rde_stack_push (p->ast, p->SV);
		Tcl_IncrRefCount (p->SV);
	    }
	}
	SCOPE void
	rde_param_i_symbol_done_void (RDE_PARAM p, int s, int m)
	{
	    SV_CLEAR (p);
	    rde_param_i_symbol_save       (p, s);
	    rde_param_i_error_nonterminal (p, m);
	    rde_stack_pop (p->LS, 1);
	}
	SCOPE void
	rde_param_i_symbol_done_d_void (RDE_PARAM p, int s, int m)
	{
	    SV_CLEAR (p);
	    rde_param_i_symbol_save       (p, s);
	    rde_param_i_error_nonterminal (p, m);
	    rde_param_i_ast_pop_rewind    (p);
	    rde_stack_pop (p->LS, 1);
	}
	SCOPE void
	rde_param_i_next_char (RDE_PARAM p, char* c, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_char (p, c, m);
	}
	SCOPE void
	rde_param_i_next_range (RDE_PARAM p, char* s, char* e, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_range (p, s, e, m);
	}
	SCOPE void
	rde_param_i_next_alnum (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_alnum (p);
	}
	SCOPE void
	rde_param_i_next_alpha (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_alpha (p);
	}
	SCOPE void
	rde_param_i_next_ascii (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_ascii (p);
	}
	SCOPE void
	rde_param_i_next_ddigit (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_ddigit (p);
	}
	SCOPE void
	rde_param_i_next_digit (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_digit (p);
	}
	SCOPE void
	rde_param_i_next_graph (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_graph (p);
	}
	SCOPE void
	rde_param_i_next_lower (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_lower (p);
	}
	SCOPE void
	rde_param_i_next_print (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_print (p);
	}
	SCOPE void
	rde_param_i_next_punct (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_punct (p);
	}
	SCOPE void
	rde_param_i_next_space (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_space (p);
	}
	SCOPE void
	rde_param_i_next_upper (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_upper (p);
	}
	SCOPE void
	rde_param_i_next_wordchar (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_wordchar (p);
	}
	SCOPE void
	rde_param_i_next_xdigit (RDE_PARAM p, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    rde_param_i_test_xdigit (p);
	}
	SCOPE void
	rde_param_i_notahead_start_d (RDE_PARAM p)
	{
	    rde_stack_push (p->LS, (void*) p->CL);
	    rde_stack_push (p->mark, (void*) rde_stack_size (p->ast));
	}
	SCOPE void
	rde_param_i_notahead_exit_d (RDE_PARAM p)
	{
	    if (p->ST) {
		rde_param_i_ast_pop_rewind (p); 
	    } else {
		rde_stack_pop (p->mark, 1);
	    }
	    p->CL = (long int) rde_stack_top (p->LS);
	    rde_stack_pop (p->LS, 1);
	    p->ST = !p->ST;
	}
	SCOPE void
	rde_param_i_notahead_exit (RDE_PARAM p)
	{
	    p->CL = (long int) rde_stack_top (p->LS);
	    rde_stack_pop (p->LS, 1);
	    p->ST = !p->ST;
	}
	SCOPE void
	rde_param_i_state_push_2 (RDE_PARAM p)
	{
	    
	    rde_stack_push (p->LS, (void*) p->CL);
	    rde_stack_push (p->ES, p->ER);
	    if (p->ER) { p->ER->refCount ++; }
	}
	SCOPE void
	rde_param_i_state_push_void (RDE_PARAM p)
	{
	    rde_stack_push (p->LS, (void*) p->CL);
	    ER_CLEAR (p);
	    rde_stack_push (p->ES, p->ER);
	    
	}
	SCOPE void
	rde_param_i_state_push_value (RDE_PARAM p)
	{
	    rde_stack_push (p->mark, (void*) rde_stack_size (p->ast));
	    rde_stack_push (p->LS, (void*) p->CL);
	    ER_CLEAR (p);
	    rde_stack_push (p->ES, p->ER);
	    
	}
	SCOPE void
	rde_param_i_state_merge_ok (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (!p->ST) {
		p->ST = 1;
		p->CL = (long int) rde_stack_top (p->LS);
	    }
	    rde_stack_pop (p->LS, 1);
	}
	SCOPE void
	rde_param_i_state_merge_void (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (!p->ST) {
		p->CL = (long int) rde_stack_top (p->LS);
	    }
	    rde_stack_pop (p->LS, 1);
	}
	SCOPE void
	rde_param_i_state_merge_value (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (!p->ST) {
		long int trim = (long int) rde_stack_top (p->mark);
		rde_stack_trim (p->ast, (int) trim);
		p->CL = (long int) rde_stack_top (p->LS);
	    }
	    rde_stack_pop (p->mark, 1);
	    rde_stack_pop (p->LS, 1);
	}
	SCOPE int
	rde_param_i_kleene_close (RDE_PARAM p)
	{
	    int stop = !p->ST;
	    rde_param_i_error_pop_merge (p);
	    if (stop) {
		p->ST = 1;
		p->CL = (long int) rde_stack_top (p->LS);
	    }
	    rde_stack_pop (p->LS, 1);
	    return stop;
	}
	SCOPE int
	rde_param_i_kleene_abort (RDE_PARAM p)
	{
	    int stop = !p->ST;
	    if (stop) {
		p->CL = (long int) rde_stack_top (p->LS);
	    }
	    rde_stack_pop (p->LS, 1);
	    return stop;
	}
	SCOPE int
	rde_param_i_seq_void2void (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (p->ST) {
		rde_stack_push (p->ES, p->ER);
		if (p->ER) { p->ER->refCount ++; }
		return 0;
	    } else {
		p->CL = (long int) rde_stack_top (p->LS);
		rde_stack_pop (p->LS, 1);
		return 1;
	    }
	}
	SCOPE int
	rde_param_i_seq_void2value (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (p->ST) {
		rde_stack_push (p->mark, (void*) rde_stack_size (p->ast));
		rde_stack_push (p->ES, p->ER);
		if (p->ER) { p->ER->refCount ++; }
		return 0;
	    } else {
		p->CL = (long int) rde_stack_top (p->LS);
		rde_stack_pop (p->LS, 1);
		return 1;
	    }
	}
	SCOPE int
	rde_param_i_seq_value2value (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (p->ST) {
		rde_stack_push (p->ES, p->ER);
		if (p->ER) { p->ER->refCount ++; }
		return 0;
	    } else {
		long int trim = (long int) rde_stack_top (p->mark);
		rde_stack_pop  (p->mark, 1);
		rde_stack_trim (p->ast, (int) trim);
		p->CL = (long int) rde_stack_top (p->LS);
		rde_stack_pop (p->LS, 1);
		return 1;
	    }
	}
	SCOPE int
	rde_param_i_bra_void2void (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (p->ST) {
		rde_stack_pop (p->LS, 1);
	    } else {
		p->CL = (long int) rde_stack_top (p->LS);
		rde_stack_push (p->ES, p->ER);
		if (p->ER) { p->ER->refCount ++; }
	    }
	    return p->ST;
	}
	SCOPE int
	rde_param_i_bra_void2value (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (p->ST) {
		rde_stack_pop (p->LS, 1);
	    } else {
		rde_stack_push (p->mark, (void*) rde_stack_size (p->ast));
		p->CL = (long int) rde_stack_top (p->LS);
		rde_stack_push (p->ES, p->ER);
		if (p->ER) { p->ER->refCount ++; }
	    }
	    return p->ST;
	}
	SCOPE int
	rde_param_i_bra_value2void (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (p->ST) {
		rde_stack_pop (p->mark, 1);
		rde_stack_pop (p->LS, 1);
	    } else {
		long int trim = (long int) rde_stack_top (p->mark);
		rde_stack_pop  (p->mark, 1);
		rde_stack_trim (p->ast, (int) trim);
		p->CL = (long int) rde_stack_top (p->LS);
		rde_stack_push (p->ES, p->ER);
		if (p->ER) { p->ER->refCount ++; }
	    }
	    return p->ST;
	}
	SCOPE int
	rde_param_i_bra_value2value (RDE_PARAM p)
	{
	    rde_param_i_error_pop_merge (p);
	    if (p->ST) {
		rde_stack_pop (p->mark, 1);
		rde_stack_pop (p->LS, 1);
	    } else {
		long int trim = (long int) rde_stack_top (p->mark);
		rde_stack_trim (p->ast, (int) trim);
		p->CL = (long int) rde_stack_top (p->LS);
		rde_stack_push (p->ES, p->ER);
		if (p->ER) { p->ER->refCount ++; }
	    }
	    return p->ST;
	}
	SCOPE void
	rde_param_i_next_str (RDE_PARAM p, char* str, int m)
	{
	    int at = p->CL;
	    while (*str) {
		rde_param_i_input_next (p, m);
		if (!p->ST) {
		    p->CL = at;
		    return;
		}
		rde_param_i_test_char (p, str, m);
		if (!p->ST) {
		    p->CL = at;
		    return;
		}
		str = Tcl_UtfNext (str);
	    }
	}
	SCOPE void
	rde_param_i_next_class (RDE_PARAM p, char* class, int m)
	{
	    rde_param_i_input_next (p, m);
	    if (!p->ST) return;
	    while (*class) {
		p->ST = Tcl_UtfNcmp (p->CC, class, 1) == 0;
		if (p->ST) {
		    ER_CLEAR (p);
		    return;
		}
		class = Tcl_UtfNext (class);
	    }
	    error_set (p, m);
	    p->CL --;
	}
	
#line 1631 "parse_test.c"
	/* -*- c -*- */

        /*
         * Declaring the parse functions
         */
        
        static void choice_5 (RDE_PARAM p);
        static void sym_AddOp (RDE_PARAM p);
        static void sequence_13 (RDE_PARAM p);
        static void kleene_15 (RDE_PARAM p);
        static void sequence_21 (RDE_PARAM p);
        static void sym_Assignment (RDE_PARAM p);
        static void choice_33 (RDE_PARAM p);
        static void sym_AssignOp (RDE_PARAM p);
        static void notahead_38 (RDE_PARAM p);
        static void sequence_41 (RDE_PARAM p);
        static void kleene_43 (RDE_PARAM p);
        static void sequence_45 (RDE_PARAM p);
        static void sym_Comment (RDE_PARAM p);
        static void optional_49 (RDE_PARAM p);
        static void sequence_54 (RDE_PARAM p);
        static void optional_56 (RDE_PARAM p);
        static void sequence_58 (RDE_PARAM p);
        static void sym_ComplexNumber (RDE_PARAM p);
        static void sym_Empty (RDE_PARAM p);
        static void sym_EOL (RDE_PARAM p);
        static void sequence_70 (RDE_PARAM p);
        static void kleene_72 (RDE_PARAM p);
        static void sequence_74 (RDE_PARAM p);
        static void sym_Expression (RDE_PARAM p);
        static void sequence_82 (RDE_PARAM p);
        static void choice_85 (RDE_PARAM p);
        static void sym_Factor (RDE_PARAM p);
        static void sequence_99 (RDE_PARAM p);
        static void sym_ForLoop (RDE_PARAM p);
        static void sequence_108 (RDE_PARAM p);
        static void choice_113 (RDE_PARAM p);
        static void sym_Fragment (RDE_PARAM p);
        static void sequence_122 (RDE_PARAM p);
        static void kleene_124 (RDE_PARAM p);
        static void sequence_126 (RDE_PARAM p);
        static void optional_128 (RDE_PARAM p);
        static void sequence_131 (RDE_PARAM p);
        static void sym_Function (RDE_PARAM p);
        static void sym_FunctionName (RDE_PARAM p);
        static void choice_138 (RDE_PARAM p);
        static void choice_142 (RDE_PARAM p);
        static void kleene_144 (RDE_PARAM p);
        static void sequence_146 (RDE_PARAM p);
        static void sym_Identifier (RDE_PARAM p);
        static void poskleene_150 (RDE_PARAM p);
        static void sequence_155 (RDE_PARAM p);
        static void optional_157 (RDE_PARAM p);
        static void optional_161 (RDE_PARAM p);
        static void sequence_165 (RDE_PARAM p);
        static void optional_167 (RDE_PARAM p);
        static void sequence_170 (RDE_PARAM p);
        static void sym_ImaginaryNumber (RDE_PARAM p);
        static void sequence_176 (RDE_PARAM p);
        static void sym_IndexExpr (RDE_PARAM p);
        static void choice_183 (RDE_PARAM p);
        static void poskleene_186 (RDE_PARAM p);
        static void sequence_191 (RDE_PARAM p);
        static void kleene_193 (RDE_PARAM p);
        static void sequence_197 (RDE_PARAM p);
        static void sym_Literal (RDE_PARAM p);
        static void choice_204 (RDE_PARAM p);
        static void sym_MulOp (RDE_PARAM p);
        static void choice_209 (RDE_PARAM p);
        static void sym_Number (RDE_PARAM p);
        static void sequence_217 (RDE_PARAM p);
        static void sym_OpAssignment (RDE_PARAM p);
        static void choice_224 (RDE_PARAM p);
        static void sym_PowOp (RDE_PARAM p);
        static void choice_230 (RDE_PARAM p);
        static void kleene_232 (RDE_PARAM p);
        static void sequence_234 (RDE_PARAM p);
        static void sym_Program (RDE_PARAM p);
        static void sequence_251 (RDE_PARAM p);
        static void sym_RealNumber (RDE_PARAM p);
        static void optional_255 (RDE_PARAM p);
        static void sequence_258 (RDE_PARAM p);
        static void choice_261 (RDE_PARAM p);
        static void sym_Separator (RDE_PARAM p);
        static void sequence_270 (RDE_PARAM p);
        static void kleene_272 (RDE_PARAM p);
        static void sequence_275 (RDE_PARAM p);
        static void sym_Sequence (RDE_PARAM p);
        static void sym_Sign (RDE_PARAM p);
        static void sym_SignedNumber (RDE_PARAM p);
        static void sequence_294 (RDE_PARAM p);
        static void optional_296 (RDE_PARAM p);
        static void sequence_298 (RDE_PARAM p);
        static void optional_300 (RDE_PARAM p);
        static void sequence_302 (RDE_PARAM p);
        static void choice_305 (RDE_PARAM p);
        static void sym_SliceExpr (RDE_PARAM p);
        static void choice_312 (RDE_PARAM p);
        static void sym_Statement (RDE_PARAM p);
        static void sequence_320 (RDE_PARAM p);
        static void kleene_322 (RDE_PARAM p);
        static void sequence_324 (RDE_PARAM p);
        static void sequence_334 (RDE_PARAM p);
        static void choice_336 (RDE_PARAM p);
        static void sym_Term (RDE_PARAM p);
        static void sequence_341 (RDE_PARAM p);
        static void choice_344 (RDE_PARAM p);
        static void sym_Transpose (RDE_PARAM p);
        static void sym_TransposeOp (RDE_PARAM p);
        static void sym_Var (RDE_PARAM p);
        static void sequence_359 (RDE_PARAM p);
        static void kleene_361 (RDE_PARAM p);
        static void sequence_365 (RDE_PARAM p);
        static void optional_367 (RDE_PARAM p);
        static void sequence_369 (RDE_PARAM p);
        static void sym_VarSlice (RDE_PARAM p);
        static void sequence_374 (RDE_PARAM p);
        static void sequence_379 (RDE_PARAM p);
        static void choice_381 (RDE_PARAM p);
        static void kleene_383 (RDE_PARAM p);
        static void sym_WS (RDE_PARAM p);
        
        /*
         * Precomputed table of strings (symbols, error messages, etc.).
         */
        
        static char const* p_string [113] = {
            /*        0 = */   "cl '+-'",
            /*        1 = */   "str '.+'",
            /*        2 = */   "str '.-'",
            /*        3 = */   "n AddOp",
            /*        4 = */   "AddOp",
            /*        5 = */   "t ,",
            /*        6 = */   "t =",
            /*        7 = */   "n Assignment",
            /*        8 = */   "Assignment",
            /*        9 = */   "str '+='",
            /*       10 = */   "str '-='",
            /*       11 = */   "str '.+='",
            /*       12 = */   "str '.-='",
            /*       13 = */   "str '.*='",
            /*       14 = */   "str './='",
            /*       15 = */   "str '.^='",
            /*       16 = */   "str '.**='",
            /*       17 = */   "n AssignOp",
            /*       18 = */   "AssignOp",
            /*       19 = */   "t #",
            /*       20 = */   "dot",
            /*       21 = */   "n Comment",
            /*       22 = */   "Comment",
            /*       23 = */   "n ComplexNumber",
            /*       24 = */   "ComplexNumber",
            /*       25 = */   "n Empty",
            /*       26 = */   "Empty",
            /*       27 = */   "t \n",
            /*       28 = */   "n EOL",
            /*       29 = */   "EOL",
            /*       30 = */   "n Expression",
            /*       31 = */   "Expression",
            /*       32 = */   "n Factor",
            /*       33 = */   "Factor",
            /*       34 = */   "str 'for'",
            /*       35 = */   "t \173",
            /*       36 = */   "t \175",
            /*       37 = */   "n ForLoop",
            /*       38 = */   "ForLoop",
            /*       39 = */   "t \50",
            /*       40 = */   "t \51",
            /*       41 = */   "n Fragment",
            /*       42 = */   "Fragment",
            /*       43 = */   "n Function",
            /*       44 = */   "Function",
            /*       45 = */   "n FunctionName",
            /*       46 = */   "FunctionName",
            /*       47 = */   "cl '_:'",
            /*       48 = */   "alpha",
            /*       49 = */   "alnum",
            /*       50 = */   "n Identifier",
            /*       51 = */   "Identifier",
            /*       52 = */   "ddigit",
            /*       53 = */   "t .",
            /*       54 = */   "cl 'eE'",
            /*       55 = */   "cl 'iI'",
            /*       56 = */   "n ImaginaryNumber",
            /*       57 = */   "ImaginaryNumber",
            /*       58 = */   "n IndexExpr",
            /*       59 = */   "IndexExpr",
            /*       60 = */   "space",
            /*       61 = */   "n Literal",
            /*       62 = */   "Literal",
            /*       63 = */   "cl '*/'",
            /*       64 = */   "str '.*'",
            /*       65 = */   "str './'",
            /*       66 = */   "cl '\134%'",
            /*       67 = */   "n MulOp",
            /*       68 = */   "MulOp",
            /*       69 = */   "n Number",
            /*       70 = */   "Number",
            /*       71 = */   "n OpAssignment",
            /*       72 = */   "OpAssignment",
            /*       73 = */   "t ^",
            /*       74 = */   "str '**'",
            /*       75 = */   "str '.^'",
            /*       76 = */   "str '.**'",
            /*       77 = */   "n PowOp",
            /*       78 = */   "PowOp",
            /*       79 = */   "n Program",
            /*       80 = */   "Program",
            /*       81 = */   "n RealNumber",
            /*       82 = */   "RealNumber",
            /*       83 = */   "t \73",
            /*       84 = */   "n Separator",
            /*       85 = */   "Separator",
            /*       86 = */   "n Sequence",
            /*       87 = */   "Sequence",
            /*       88 = */   "n Sign",
            /*       89 = */   "Sign",
            /*       90 = */   "n SignedNumber",
            /*       91 = */   "SignedNumber",
            /*       92 = */   "t :",
            /*       93 = */   "n SliceExpr",
            /*       94 = */   "SliceExpr",
            /*       95 = */   "n Statement",
            /*       96 = */   "Statement",
            /*       97 = */   "n Term",
            /*       98 = */   "Term",
            /*       99 = */   "n Transpose",
            /*      100 = */   "Transpose",
            /*      101 = */   "t '",
            /*      102 = */   "n TransposeOp",
            /*      103 = */   "TransposeOp",
            /*      104 = */   "n Var",
            /*      105 = */   "Var",
            /*      106 = */   "t \133",
            /*      107 = */   "t \135",
            /*      108 = */   "n VarSlice",
            /*      109 = */   "VarSlice",
            /*      110 = */   "t \134",
            /*      111 = */   "n WS",
            /*      112 = */   "WS"
        };
        
        /*
         * Grammar Start Expression
         */
        
        static void MAIN (RDE_PARAM p) {
            sym_Sequence (p);
            return;
        }
        
        /*
         * leaf Symbol 'AddOp'
         */
        
        static void sym_AddOp (RDE_PARAM p) {
           /*
            * /
            *     [+-]
            *     ".+"
            *     ".-"
            */
        
            if (rde_param_i_symbol_start (p, 4)) return ;
            choice_5 (p);
            rde_param_i_symbol_done_leaf (p, 4, 3);
            return;
        }
        
        static void choice_5 (RDE_PARAM p) {
           /*
            * /
            *     [+-]
            *     ".+"
            *     ".-"
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_class (p, "+-", 0);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".+", 1);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".-", 2);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        /*
         * value Symbol 'Assignment'
         */
        
        static void sym_Assignment (RDE_PARAM p) {
           /*
            * x
            *     (VarSlice)
            *     *
            *         x
            *             (WS)
            *             ','
            *             (WS)
            *             (VarSlice)
            *     (WS)
            *     '='
            *     (WS)
            *     (Expression)
            */
        
            if (rde_param_i_symbol_start_d (p, 8)) return ;
            sequence_21 (p);
            rde_param_i_symbol_done_d_reduce (p, 8, 7);
            return;
        }
        
        static void sequence_21 (RDE_PARAM p) {
           /*
            * x
            *     (VarSlice)
            *     *
            *         x
            *             (WS)
            *             ','
            *             (WS)
            *             (VarSlice)
            *     (WS)
            *     '='
            *     (WS)
            *     (Expression)
            */
        
            rde_param_i_state_push_value (p);
            sym_VarSlice (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_15 (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "=", 6);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Expression (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_15 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         (WS)
            *         ','
            *         (WS)
            *         (VarSlice)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_13 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_13 (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     ','
            *     (WS)
            *     (VarSlice)
            */
        
            rde_param_i_state_push_void (p);
            sym_WS (p);
            if (rde_param_i_seq_void2void(p)) return;
            rde_param_i_next_char (p, ",", 5);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_VarSlice (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * leaf Symbol 'AssignOp'
         */
        
        static void sym_AssignOp (RDE_PARAM p) {
           /*
            * /
            *     '='
            *     "+="
            *     "-="
            *     ".+="
            *     ".-="
            *     ".*="
            *     "./="
            *     ".^="
            *     ".**="
            */
        
            if (rde_param_i_symbol_start (p, 18)) return ;
            choice_33 (p);
            rde_param_i_symbol_done_leaf (p, 18, 17);
            return;
        }
        
        static void choice_33 (RDE_PARAM p) {
           /*
            * /
            *     '='
            *     "+="
            *     "-="
            *     ".+="
            *     ".-="
            *     ".*="
            *     "./="
            *     ".^="
            *     ".**="
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, "=", 6);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, "+=", 9);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, "-=", 10);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".+=", 11);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".-=", 12);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".*=", 13);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, "./=", 14);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".^=", 15);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".**=", 16);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        /*
         * void Symbol 'Comment'
         */
        
        static void sym_Comment (RDE_PARAM p) {
           /*
            * x
            *     '#'
            *     *
            *         x
            *             !
            *                 (EOL)
            *             <dot>
            */
        
            if (rde_param_i_symbol_void_start (p, 22)) return ;
            sequence_45 (p);
            rde_param_i_symbol_done_void (p, 22, 21);
            return;
        }
        
        static void sequence_45 (RDE_PARAM p) {
           /*
            * x
            *     '#'
            *     *
            *         x
            *             !
            *                 (EOL)
            *             <dot>
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, "#", 19);
            if (rde_param_i_seq_void2void(p)) return;
            kleene_43 (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void kleene_43 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         !
            *             (EOL)
            *         <dot>
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_41 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_41 (RDE_PARAM p) {
           /*
            * x
            *     !
            *         (EOL)
            *     <dot>
            */
        
            rde_param_i_state_push_void (p);
            notahead_38 (p);
            if (rde_param_i_seq_void2void(p)) return;
            rde_param_i_input_next (p, 20);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void notahead_38 (RDE_PARAM p) {
           /*
            * !
            *     (EOL)
            */
        
            rde_param_i_loc_push (p);
            sym_EOL (p);
            rde_param_i_notahead_exit (p);
            return;
        }
        
        /*
         * value Symbol 'ComplexNumber'
         */
        
        static void sym_ComplexNumber (RDE_PARAM p) {
           /*
            * x
            *     ?
            *         (Sign)
            *     (RealNumber)
            *     ?
            *         x
            *             (Sign)
            *             (ImaginaryNumber)
            */
        
            if (rde_param_i_symbol_start_d (p, 24)) return ;
            sequence_58 (p);
            rde_param_i_symbol_done_d_reduce (p, 24, 23);
            return;
        }
        
        static void sequence_58 (RDE_PARAM p) {
           /*
            * x
            *     ?
            *         (Sign)
            *     (RealNumber)
            *     ?
            *         x
            *             (Sign)
            *             (ImaginaryNumber)
            */
        
            rde_param_i_state_push_value (p);
            optional_49 (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_RealNumber (p);
            if (rde_param_i_seq_value2value(p)) return;
            optional_56 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void optional_49 (RDE_PARAM p) {
           /*
            * ?
            *     (Sign)
            */
        
            rde_param_i_state_push_2 (p);
            sym_Sign (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void optional_56 (RDE_PARAM p) {
           /*
            * ?
            *     x
            *         (Sign)
            *         (ImaginaryNumber)
            */
        
            rde_param_i_state_push_2 (p);
            sequence_54 (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void sequence_54 (RDE_PARAM p) {
           /*
            * x
            *     (Sign)
            *     (ImaginaryNumber)
            */
        
            rde_param_i_state_push_value (p);
            sym_Sign (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_ImaginaryNumber (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Empty'
         */
        
        static void sym_Empty (RDE_PARAM p) {
           /*
            * (WS)
            */
        
            if (rde_param_i_symbol_start (p, 26)) return ;
            sym_WS (p);
            rde_param_i_symbol_done_leaf (p, 26, 25);
            return;
        }
        
        /*
         * void Symbol 'EOL'
         */
        
        static void sym_EOL (RDE_PARAM p) {
           /*
            * '\n'
            */
        
            if (rde_param_i_symbol_void_start (p, 29)) return ;
            rde_param_i_next_char (p, "\n", 27);
            rde_param_i_symbol_done_void (p, 29, 28);
            return;
        }
        
        /*
         * value Symbol 'Expression'
         */
        
        static void sym_Expression (RDE_PARAM p) {
           /*
            * x
            *     (Term)
            *     *
            *         x
            *             (WS)
            *             (AddOp)
            *             (WS)
            *             (Term)
            */
        
            if (rde_param_i_symbol_start_d (p, 31)) return ;
            sequence_74 (p);
            rde_param_i_symbol_done_d_reduce (p, 31, 30);
            return;
        }
        
        static void sequence_74 (RDE_PARAM p) {
           /*
            * x
            *     (Term)
            *     *
            *         x
            *             (WS)
            *             (AddOp)
            *             (WS)
            *             (Term)
            */
        
            rde_param_i_state_push_value (p);
            sym_Term (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_72 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_72 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         (WS)
            *         (AddOp)
            *         (WS)
            *         (Term)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_70 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_70 (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     (AddOp)
            *     (WS)
            *     (Term)
            */
        
            rde_param_i_state_push_void (p);
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_AddOp (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Term (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Factor'
         */
        
        static void sym_Factor (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (Transpose)
            *         (WS)
            *         (PowOp)
            *         (WS)
            *         (Factor)
            *     (Transpose)
            */
        
            if (rde_param_i_symbol_start_d (p, 33)) return ;
            choice_85 (p);
            rde_param_i_symbol_done_d_reduce (p, 33, 32);
            return;
        }
        
        static void choice_85 (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (Transpose)
            *         (WS)
            *         (PowOp)
            *         (WS)
            *         (Factor)
            *     (Transpose)
            */
        
            rde_param_i_state_push_value (p);
            sequence_82 (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_Transpose (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void sequence_82 (RDE_PARAM p) {
           /*
            * x
            *     (Transpose)
            *     (WS)
            *     (PowOp)
            *     (WS)
            *     (Factor)
            */
        
            rde_param_i_state_push_value (p);
            sym_Transpose (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_PowOp (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Factor (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'ForLoop'
         */
        
        static void sym_ForLoop (RDE_PARAM p) {
           /*
            * x
            *     "for"
            *     (WS)
            *     (Var)
            *     (WS)
            *     '='
            *     (WS)
            *     (Expression)
            *     (WS)
            *     '\{'
            *     (Sequence)
            *     '\}'
            */
        
            if (rde_param_i_symbol_start_d (p, 38)) return ;
            sequence_99 (p);
            rde_param_i_symbol_done_d_reduce (p, 38, 37);
            return;
        }
        
        static void sequence_99 (RDE_PARAM p) {
           /*
            * x
            *     "for"
            *     (WS)
            *     (Var)
            *     (WS)
            *     '='
            *     (WS)
            *     (Expression)
            *     (WS)
            *     '\{'
            *     (Sequence)
            *     '\}'
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_str (p, "for", 34);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_Var (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "=", 6);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Expression (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "\173", 35);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Sequence (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "\175", 36);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Fragment'
         */
        
        static void sym_Fragment (RDE_PARAM p) {
           /*
            * /
            *     (Number)
            *     x
            *         '\('
            *         (WS)
            *         (Expression)
            *         (WS)
            *         '\)'
            *     (Function)
            *     (VarSlice)
            *     (Literal)
            */
        
            if (rde_param_i_symbol_start_d (p, 42)) return ;
            choice_113 (p);
            rde_param_i_symbol_done_d_reduce (p, 42, 41);
            return;
        }
        
        static void choice_113 (RDE_PARAM p) {
           /*
            * /
            *     (Number)
            *     x
            *         '\('
            *         (WS)
            *         (Expression)
            *         (WS)
            *         '\)'
            *     (Function)
            *     (VarSlice)
            *     (Literal)
            */
        
            rde_param_i_state_push_value (p);
            sym_Number (p);
            if (rde_param_i_bra_value2value(p)) return;
            sequence_108 (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_Function (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_VarSlice (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_Literal (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void sequence_108 (RDE_PARAM p) {
           /*
            * x
            *     '\('
            *     (WS)
            *     (Expression)
            *     (WS)
            *     '\)'
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, "\50", 39);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_Expression (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "\51", 40);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Function'
         */
        
        static void sym_Function (RDE_PARAM p) {
           /*
            * x
            *     (FunctionName)
            *     '\('
            *     ?
            *         x
            *             (Expression)
            *             *
            *                 x
            *                     ','
            *                     (WS)
            *                     (Expression)
            *     '\)'
            */
        
            if (rde_param_i_symbol_start_d (p, 44)) return ;
            sequence_131 (p);
            rde_param_i_symbol_done_d_reduce (p, 44, 43);
            return;
        }
        
        static void sequence_131 (RDE_PARAM p) {
           /*
            * x
            *     (FunctionName)
            *     '\('
            *     ?
            *         x
            *             (Expression)
            *             *
            *                 x
            *                     ','
            *                     (WS)
            *                     (Expression)
            *     '\)'
            */
        
            rde_param_i_state_push_value (p);
            sym_FunctionName (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "\50", 39);
            if (rde_param_i_seq_value2value(p)) return;
            optional_128 (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "\51", 40);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void optional_128 (RDE_PARAM p) {
           /*
            * ?
            *     x
            *         (Expression)
            *         *
            *             x
            *                 ','
            *                 (WS)
            *                 (Expression)
            */
        
            rde_param_i_state_push_2 (p);
            sequence_126 (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void sequence_126 (RDE_PARAM p) {
           /*
            * x
            *     (Expression)
            *     *
            *         x
            *             ','
            *             (WS)
            *             (Expression)
            */
        
            rde_param_i_state_push_value (p);
            sym_Expression (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_124 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_124 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         ','
            *         (WS)
            *         (Expression)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_122 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_122 (RDE_PARAM p) {
           /*
            * x
            *     ','
            *     (WS)
            *     (Expression)
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, ",", 5);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_Expression (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * leaf Symbol 'FunctionName'
         */
        
        static void sym_FunctionName (RDE_PARAM p) {
           /*
            * (Identifier)
            */
        
            if (rde_param_i_symbol_start_d (p, 46)) return ;
            sym_Identifier (p);
            rde_param_i_symbol_done_d_leaf (p, 46, 45);
            return;
        }
        
        /*
         * leaf Symbol 'Identifier'
         */
        
        static void sym_Identifier (RDE_PARAM p) {
           /*
            * x
            *     /
            *         [_:]
            *         <alpha>
            *     *
            *         /
            *             [_:]
            *             <alnum>
            */
        
            if (rde_param_i_symbol_start (p, 51)) return ;
            sequence_146 (p);
            rde_param_i_symbol_done_leaf (p, 51, 50);
            return;
        }
        
        static void sequence_146 (RDE_PARAM p) {
           /*
            * x
            *     /
            *         [_:]
            *         <alpha>
            *     *
            *         /
            *             [_:]
            *             <alnum>
            */
        
            rde_param_i_state_push_void (p);
            choice_138 (p);
            if (rde_param_i_seq_void2void(p)) return;
            kleene_144 (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void choice_138 (RDE_PARAM p) {
           /*
            * /
            *     [_:]
            *     <alpha>
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_class (p, "_:", 47);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_alpha (p, 48);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void kleene_144 (RDE_PARAM p) {
           /*
            * *
            *     /
            *         [_:]
            *         <alnum>
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                choice_142 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void choice_142 (RDE_PARAM p) {
           /*
            * /
            *     [_:]
            *     <alnum>
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_class (p, "_:", 47);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_alnum (p, 49);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        /*
         * leaf Symbol 'ImaginaryNumber'
         */
        
        static void sym_ImaginaryNumber (RDE_PARAM p) {
           /*
            * x
            *     +
            *         <ddigit>
            *     ?
            *         x
            *             '.'
            *             +
            *                 <ddigit>
            *     ?
            *         x
            *             [eE]
            *             ?
            *                 [+-]
            *             +
            *                 <ddigit>
            *     [iI]
            */
        
            if (rde_param_i_symbol_start (p, 57)) return ;
            sequence_170 (p);
            rde_param_i_symbol_done_leaf (p, 57, 56);
            return;
        }
        
        static void sequence_170 (RDE_PARAM p) {
           /*
            * x
            *     +
            *         <ddigit>
            *     ?
            *         x
            *             '.'
            *             +
            *                 <ddigit>
            *     ?
            *         x
            *             [eE]
            *             ?
            *                 [+-]
            *             +
            *                 <ddigit>
            *     [iI]
            */
        
            rde_param_i_state_push_void (p);
            poskleene_150 (p);
            if (rde_param_i_seq_void2void(p)) return;
            optional_157 (p);
            if (rde_param_i_seq_void2void(p)) return;
            optional_167 (p);
            if (rde_param_i_seq_void2void(p)) return;
            rde_param_i_next_class (p, "iI", 55);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void poskleene_150 (RDE_PARAM p) {
           /*
            * +
            *     <ddigit>
            */
        
            rde_param_i_loc_push (p);
            rde_param_i_next_ddigit (p, 52);
            if (rde_param_i_kleene_abort(p)) return;
            while (1) {
                rde_param_i_state_push_2 (p);
                rde_param_i_next_ddigit (p, 52);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void optional_157 (RDE_PARAM p) {
           /*
            * ?
            *     x
            *         '.'
            *         +
            *             <ddigit>
            */
        
            rde_param_i_state_push_2 (p);
            sequence_155 (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void sequence_155 (RDE_PARAM p) {
           /*
            * x
            *     '.'
            *     +
            *         <ddigit>
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, ".", 53);
            if (rde_param_i_seq_void2void(p)) return;
            poskleene_150 (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void optional_167 (RDE_PARAM p) {
           /*
            * ?
            *     x
            *         [eE]
            *         ?
            *             [+-]
            *         +
            *             <ddigit>
            */
        
            rde_param_i_state_push_2 (p);
            sequence_165 (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void sequence_165 (RDE_PARAM p) {
           /*
            * x
            *     [eE]
            *     ?
            *         [+-]
            *     +
            *         <ddigit>
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_class (p, "eE", 54);
            if (rde_param_i_seq_void2void(p)) return;
            optional_161 (p);
            if (rde_param_i_seq_void2void(p)) return;
            poskleene_150 (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void optional_161 (RDE_PARAM p) {
           /*
            * ?
            *     [+-]
            */
        
            rde_param_i_state_push_2 (p);
            rde_param_i_next_class (p, "+-", 0);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        /*
         * value Symbol 'IndexExpr'
         */
        
        static void sym_IndexExpr (RDE_PARAM p) {
           /*
            * x
            *     ?
            *         (Sign)
            *     (RealNumber)
            */
        
            if (rde_param_i_symbol_start_d (p, 59)) return ;
            sequence_176 (p);
            rde_param_i_symbol_done_d_reduce (p, 59, 58);
            return;
        }
        
        static void sequence_176 (RDE_PARAM p) {
           /*
            * x
            *     ?
            *         (Sign)
            *     (RealNumber)
            */
        
            rde_param_i_state_push_value (p);
            optional_49 (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_RealNumber (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Literal'
         */
        
        static void sym_Literal (RDE_PARAM p) {
           /*
            * x
            *     '\{'
            *     (WS)
            *     /
            *         (ComplexNumber)
            *         (Literal)
            *     *
            *         x
            *             +
            *                 <space>
            *             /
            *                 (ComplexNumber)
            *                 (Literal)
            *     (WS)
            *     '\}'
            */
        
            if (rde_param_i_symbol_start_d (p, 62)) return ;
            sequence_197 (p);
            rde_param_i_symbol_done_d_reduce (p, 62, 61);
            return;
        }
        
        static void sequence_197 (RDE_PARAM p) {
           /*
            * x
            *     '\{'
            *     (WS)
            *     /
            *         (ComplexNumber)
            *         (Literal)
            *     *
            *         x
            *             +
            *                 <space>
            *             /
            *                 (ComplexNumber)
            *                 (Literal)
            *     (WS)
            *     '\}'
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, "\173", 35);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            choice_183 (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_193 (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "\175", 36);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void choice_183 (RDE_PARAM p) {
           /*
            * /
            *     (ComplexNumber)
            *     (Literal)
            */
        
            rde_param_i_state_push_value (p);
            sym_ComplexNumber (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_Literal (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_193 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         +
            *             <space>
            *         /
            *             (ComplexNumber)
            *             (Literal)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_191 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_191 (RDE_PARAM p) {
           /*
            * x
            *     +
            *         <space>
            *     /
            *         (ComplexNumber)
            *         (Literal)
            */
        
            rde_param_i_state_push_void (p);
            poskleene_186 (p);
            if (rde_param_i_seq_void2value(p)) return;
            choice_183 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void poskleene_186 (RDE_PARAM p) {
           /*
            * +
            *     <space>
            */
        
            rde_param_i_loc_push (p);
            rde_param_i_next_space (p, 60);
            if (rde_param_i_kleene_abort(p)) return;
            while (1) {
                rde_param_i_state_push_2 (p);
                rde_param_i_next_space (p, 60);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        /*
         * leaf Symbol 'MulOp'
         */
        
        static void sym_MulOp (RDE_PARAM p) {
           /*
            * /
            *     [/]
            *     ".*"
            *     "./"
            *     [\%]
            */
        
            if (rde_param_i_symbol_start (p, 68)) return ;
            choice_204 (p);
            rde_param_i_symbol_done_leaf (p, 68, 67);
            return;
        }
        
        static void choice_204 (RDE_PARAM p) {
           /*
            * /
            *     [/]
            *     ".*"
            *     "./"
            *     [\%]
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_class (p, "*/", 63);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".*", 64);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, "./", 65);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_class (p, "\134%", 66);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        /*
         * leaf Symbol 'Number'
         */
        
        static void sym_Number (RDE_PARAM p) {
           /*
            * /
            *     (ImaginaryNumber)
            *     (RealNumber)
            */
        
            if (rde_param_i_symbol_start_d (p, 70)) return ;
            choice_209 (p);
            rde_param_i_symbol_done_d_leaf (p, 70, 69);
            return;
        }
        
        static void choice_209 (RDE_PARAM p) {
           /*
            * /
            *     (ImaginaryNumber)
            *     (RealNumber)
            */
        
            rde_param_i_state_push_value (p);
            sym_ImaginaryNumber (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_RealNumber (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'OpAssignment'
         */
        
        static void sym_OpAssignment (RDE_PARAM p) {
           /*
            * x
            *     (VarSlice)
            *     (WS)
            *     (AssignOp)
            *     (WS)
            *     (Expression)
            */
        
            if (rde_param_i_symbol_start_d (p, 72)) return ;
            sequence_217 (p);
            rde_param_i_symbol_done_d_reduce (p, 72, 71);
            return;
        }
        
        static void sequence_217 (RDE_PARAM p) {
           /*
            * x
            *     (VarSlice)
            *     (WS)
            *     (AssignOp)
            *     (WS)
            *     (Expression)
            */
        
            rde_param_i_state_push_value (p);
            sym_VarSlice (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_AssignOp (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Expression (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * leaf Symbol 'PowOp'
         */
        
        static void sym_PowOp (RDE_PARAM p) {
           /*
            * /
            *     '^'
            *     "**"
            *     ".^"
            *     ".**"
            */
        
            if (rde_param_i_symbol_start (p, 78)) return ;
            choice_224 (p);
            rde_param_i_symbol_done_leaf (p, 78, 77);
            return;
        }
        
        static void choice_224 (RDE_PARAM p) {
           /*
            * /
            *     '^'
            *     "**"
            *     ".^"
            *     ".**"
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, "^", 73);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, "**", 74);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".^", 75);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_str (p, ".**", 76);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        /*
         * value Symbol 'Program'
         */
        
        static void sym_Program (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     *
            *         /
            *             (Sequence)
            *             (ForLoop)
            */
        
            if (rde_param_i_symbol_start_d (p, 80)) return ;
            sequence_234 (p);
            rde_param_i_symbol_done_d_reduce (p, 80, 79);
            return;
        }
        
        static void sequence_234 (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     *
            *         /
            *             (Sequence)
            *             (ForLoop)
            */
        
            rde_param_i_state_push_void (p);
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            kleene_232 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_232 (RDE_PARAM p) {
           /*
            * *
            *     /
            *         (Sequence)
            *         (ForLoop)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                choice_230 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void choice_230 (RDE_PARAM p) {
           /*
            * /
            *     (Sequence)
            *     (ForLoop)
            */
        
            rde_param_i_state_push_value (p);
            sym_Sequence (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_ForLoop (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * leaf Symbol 'RealNumber'
         */
        
        static void sym_RealNumber (RDE_PARAM p) {
           /*
            * x
            *     +
            *         <ddigit>
            *     ?
            *         x
            *             '.'
            *             +
            *                 <ddigit>
            *     ?
            *         x
            *             [eE]
            *             ?
            *                 [+-]
            *             +
            *                 <ddigit>
            */
        
            if (rde_param_i_symbol_start (p, 82)) return ;
            sequence_251 (p);
            rde_param_i_symbol_done_leaf (p, 82, 81);
            return;
        }
        
        static void sequence_251 (RDE_PARAM p) {
           /*
            * x
            *     +
            *         <ddigit>
            *     ?
            *         x
            *             '.'
            *             +
            *                 <ddigit>
            *     ?
            *         x
            *             [eE]
            *             ?
            *                 [+-]
            *             +
            *                 <ddigit>
            */
        
            rde_param_i_state_push_void (p);
            poskleene_150 (p);
            if (rde_param_i_seq_void2void(p)) return;
            optional_157 (p);
            if (rde_param_i_seq_void2void(p)) return;
            optional_167 (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        /*
         * void Symbol 'Separator'
         */
        
        static void sym_Separator (RDE_PARAM p) {
           /*
            * /
            *     x
            *         ?
            *             (Comment)
            *         (EOL)
            *     ';'
            */
        
            if (rde_param_i_symbol_void_start (p, 85)) return ;
            choice_261 (p);
            rde_param_i_symbol_done_void (p, 85, 84);
            return;
        }
        
        static void choice_261 (RDE_PARAM p) {
           /*
            * /
            *     x
            *         ?
            *             (Comment)
            *         (EOL)
            *     ';'
            */
        
            rde_param_i_state_push_void (p);
            sequence_258 (p);
            if (rde_param_i_bra_void2void(p)) return;
            rde_param_i_next_char (p, "\73", 83);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void sequence_258 (RDE_PARAM p) {
           /*
            * x
            *     ?
            *         (Comment)
            *     (EOL)
            */
        
            rde_param_i_state_push_void (p);
            optional_255 (p);
            if (rde_param_i_seq_void2void(p)) return;
            sym_EOL (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void optional_255 (RDE_PARAM p) {
           /*
            * ?
            *     (Comment)
            */
        
            rde_param_i_state_push_2 (p);
            sym_Comment (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        /*
         * value Symbol 'Sequence'
         */
        
        static void sym_Sequence (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     (Statement)
            *     *
            *         x
            *             (WS)
            *             (Separator)
            *             (WS)
            *             (Statement)
            *     (WS)
            */
        
            if (rde_param_i_symbol_start_d (p, 87)) return ;
            sequence_275 (p);
            rde_param_i_symbol_done_d_reduce (p, 87, 86);
            return;
        }
        
        static void sequence_275 (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     (Statement)
            *     *
            *         x
            *             (WS)
            *             (Separator)
            *             (WS)
            *             (Statement)
            *     (WS)
            */
        
            rde_param_i_state_push_void (p);
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_Statement (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_272 (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_272 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         (WS)
            *         (Separator)
            *         (WS)
            *         (Statement)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_270 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_270 (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     (Separator)
            *     (WS)
            *     (Statement)
            */
        
            rde_param_i_state_push_void (p);
            sym_WS (p);
            if (rde_param_i_seq_void2void(p)) return;
            sym_Separator (p);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_Statement (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * leaf Symbol 'Sign'
         */
        
        static void sym_Sign (RDE_PARAM p) {
           /*
            * [+-]
            */
        
            if (rde_param_i_symbol_start (p, 89)) return ;
            rde_param_i_next_class (p, "+-", 0);
            rde_param_i_symbol_done_leaf (p, 89, 88);
            return;
        }
        
        /*
         * value Symbol 'SignedNumber'
         */
        
        static void sym_SignedNumber (RDE_PARAM p) {
           /*
            * x
            *     ?
            *         (Sign)
            *     (RealNumber)
            */
        
            if (rde_param_i_symbol_start_d (p, 91)) return ;
            sequence_176 (p);
            rde_param_i_symbol_done_d_reduce (p, 91, 90);
            return;
        }
        
        /*
         * value Symbol 'SliceExpr'
         */
        
        static void sym_SliceExpr (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (IndexExpr)
            *         (WS)
            *         ?
            *             x
            *                 ':'
            *                 (WS)
            *                 (IndexExpr)
            *                 (WS)
            *                 ?
            *                     x
            *                         ':'
            *                         (WS)
            *                         (IndexExpr)
            *     ':'
            */
        
            if (rde_param_i_symbol_start_d (p, 94)) return ;
            choice_305 (p);
            rde_param_i_symbol_done_d_reduce (p, 94, 93);
            return;
        }
        
        static void choice_305 (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (IndexExpr)
            *         (WS)
            *         ?
            *             x
            *                 ':'
            *                 (WS)
            *                 (IndexExpr)
            *                 (WS)
            *                 ?
            *                     x
            *                         ':'
            *                         (WS)
            *                         (IndexExpr)
            *     ':'
            */
        
            rde_param_i_state_push_value (p);
            sequence_302 (p);
            if (rde_param_i_bra_value2void(p)) return;
            rde_param_i_next_char (p, ":", 92);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void sequence_302 (RDE_PARAM p) {
           /*
            * x
            *     (IndexExpr)
            *     (WS)
            *     ?
            *         x
            *             ':'
            *             (WS)
            *             (IndexExpr)
            *             (WS)
            *             ?
            *                 x
            *                     ':'
            *                     (WS)
            *                     (IndexExpr)
            */
        
            rde_param_i_state_push_value (p);
            sym_IndexExpr (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            optional_300 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void optional_300 (RDE_PARAM p) {
           /*
            * ?
            *     x
            *         ':'
            *         (WS)
            *         (IndexExpr)
            *         (WS)
            *         ?
            *             x
            *                 ':'
            *                 (WS)
            *                 (IndexExpr)
            */
        
            rde_param_i_state_push_2 (p);
            sequence_298 (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void sequence_298 (RDE_PARAM p) {
           /*
            * x
            *     ':'
            *     (WS)
            *     (IndexExpr)
            *     (WS)
            *     ?
            *         x
            *             ':'
            *             (WS)
            *             (IndexExpr)
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, ":", 92);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_IndexExpr (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            optional_296 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void optional_296 (RDE_PARAM p) {
           /*
            * ?
            *     x
            *         ':'
            *         (WS)
            *         (IndexExpr)
            */
        
            rde_param_i_state_push_2 (p);
            sequence_294 (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void sequence_294 (RDE_PARAM p) {
           /*
            * x
            *     ':'
            *     (WS)
            *     (IndexExpr)
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, ":", 92);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_IndexExpr (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Statement'
         */
        
        static void sym_Statement (RDE_PARAM p) {
           /*
            * /
            *     (Assignment)
            *     (OpAssignment)
            *     (Expression)
            *     (Empty)
            */
        
            if (rde_param_i_symbol_start_d (p, 96)) return ;
            choice_312 (p);
            rde_param_i_symbol_done_d_reduce (p, 96, 95);
            return;
        }
        
        static void choice_312 (RDE_PARAM p) {
           /*
            * /
            *     (Assignment)
            *     (OpAssignment)
            *     (Expression)
            *     (Empty)
            */
        
            rde_param_i_state_push_value (p);
            sym_Assignment (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_OpAssignment (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_Expression (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_Empty (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Term'
         */
        
        static void sym_Term (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (Factor)
            *         *
            *             x
            *                 (WS)
            *                 (MulOp)
            *                 (WS)
            *                 (Factor)
            *     x
            *         (Sign)
            *         (Factor)
            *         *
            *             x
            *                 (WS)
            *                 (MulOp)
            *                 (WS)
            *                 (Factor)
            */
        
            if (rde_param_i_symbol_start_d (p, 98)) return ;
            choice_336 (p);
            rde_param_i_symbol_done_d_reduce (p, 98, 97);
            return;
        }
        
        static void choice_336 (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (Factor)
            *         *
            *             x
            *                 (WS)
            *                 (MulOp)
            *                 (WS)
            *                 (Factor)
            *     x
            *         (Sign)
            *         (Factor)
            *         *
            *             x
            *                 (WS)
            *                 (MulOp)
            *                 (WS)
            *                 (Factor)
            */
        
            rde_param_i_state_push_value (p);
            sequence_324 (p);
            if (rde_param_i_bra_value2value(p)) return;
            sequence_334 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void sequence_324 (RDE_PARAM p) {
           /*
            * x
            *     (Factor)
            *     *
            *         x
            *             (WS)
            *             (MulOp)
            *             (WS)
            *             (Factor)
            */
        
            rde_param_i_state_push_value (p);
            sym_Factor (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_322 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_322 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         (WS)
            *         (MulOp)
            *         (WS)
            *         (Factor)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_320 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_320 (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     (MulOp)
            *     (WS)
            *     (Factor)
            */
        
            rde_param_i_state_push_void (p);
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_MulOp (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Factor (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void sequence_334 (RDE_PARAM p) {
           /*
            * x
            *     (Sign)
            *     (Factor)
            *     *
            *         x
            *             (WS)
            *             (MulOp)
            *             (WS)
            *             (Factor)
            */
        
            rde_param_i_state_push_value (p);
            sym_Sign (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_Factor (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_322 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * value Symbol 'Transpose'
         */
        
        static void sym_Transpose (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (Fragment)
            *         (TransposeOp)
            *     (Fragment)
            */
        
            if (rde_param_i_symbol_start_d (p, 100)) return ;
            choice_344 (p);
            rde_param_i_symbol_done_d_reduce (p, 100, 99);
            return;
        }
        
        static void choice_344 (RDE_PARAM p) {
           /*
            * /
            *     x
            *         (Fragment)
            *         (TransposeOp)
            *     (Fragment)
            */
        
            rde_param_i_state_push_value (p);
            sequence_341 (p);
            if (rde_param_i_bra_value2value(p)) return;
            sym_Fragment (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void sequence_341 (RDE_PARAM p) {
           /*
            * x
            *     (Fragment)
            *     (TransposeOp)
            */
        
            rde_param_i_state_push_value (p);
            sym_Fragment (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_TransposeOp (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * leaf Symbol 'TransposeOp'
         */
        
        static void sym_TransposeOp (RDE_PARAM p) {
           /*
            * '''
            */
        
            if (rde_param_i_symbol_start (p, 103)) return ;
            rde_param_i_next_char (p, "'", 101);
            rde_param_i_symbol_done_leaf (p, 103, 102);
            return;
        }
        
        /*
         * leaf Symbol 'Var'
         */
        
        static void sym_Var (RDE_PARAM p) {
           /*
            * (Identifier)
            */
        
            if (rde_param_i_symbol_start_d (p, 105)) return ;
            sym_Identifier (p);
            rde_param_i_symbol_done_d_leaf (p, 105, 104);
            return;
        }
        
        /*
         * value Symbol 'VarSlice'
         */
        
        static void sym_VarSlice (RDE_PARAM p) {
           /*
            * x
            *     (Var)
            *     ?
            *         x
            *             (WS)
            *             '['
            *             (WS)
            *             (SliceExpr)
            *             *
            *                 x
            *                     ','
            *                     (WS)
            *                     (SliceExpr)
            *             (WS)
            *             ']'
            */
        
            if (rde_param_i_symbol_start_d (p, 109)) return ;
            sequence_369 (p);
            rde_param_i_symbol_done_d_reduce (p, 109, 108);
            return;
        }
        
        static void sequence_369 (RDE_PARAM p) {
           /*
            * x
            *     (Var)
            *     ?
            *         x
            *             (WS)
            *             '['
            *             (WS)
            *             (SliceExpr)
            *             *
            *                 x
            *                     ','
            *                     (WS)
            *                     (SliceExpr)
            *             (WS)
            *             ']'
            */
        
            rde_param_i_state_push_value (p);
            sym_Var (p);
            if (rde_param_i_seq_value2value(p)) return;
            optional_367 (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void optional_367 (RDE_PARAM p) {
           /*
            * ?
            *     x
            *         (WS)
            *         '['
            *         (WS)
            *         (SliceExpr)
            *         *
            *             x
            *                 ','
            *                 (WS)
            *                 (SliceExpr)
            *         (WS)
            *         ']'
            */
        
            rde_param_i_state_push_2 (p);
            sequence_365 (p);
            rde_param_i_state_merge_ok (p);
            return;
        }
        
        static void sequence_365 (RDE_PARAM p) {
           /*
            * x
            *     (WS)
            *     '['
            *     (WS)
            *     (SliceExpr)
            *     *
            *         x
            *             ','
            *             (WS)
            *             (SliceExpr)
            *     (WS)
            *     ']'
            */
        
            rde_param_i_state_push_void (p);
            sym_WS (p);
            if (rde_param_i_seq_void2void(p)) return;
            rde_param_i_next_char (p, "\133", 106);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_SliceExpr (p);
            if (rde_param_i_seq_value2value(p)) return;
            kleene_361 (p);
            if (rde_param_i_seq_value2value(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_value2value(p)) return;
            rde_param_i_next_char (p, "\135", 107);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        static void kleene_361 (RDE_PARAM p) {
           /*
            * *
            *     x
            *         ','
            *         (WS)
            *         (SliceExpr)
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                sequence_359 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void sequence_359 (RDE_PARAM p) {
           /*
            * x
            *     ','
            *     (WS)
            *     (SliceExpr)
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, ",", 5);
            if (rde_param_i_seq_void2void(p)) return;
            sym_WS (p);
            if (rde_param_i_seq_void2value(p)) return;
            sym_SliceExpr (p);
            rde_param_i_state_merge_value (p);
            return;
        }
        
        /*
         * void Symbol 'WS'
         */
        
        static void sym_WS (RDE_PARAM p) {
           /*
            * *
            *     /
            *         x
            *             '\'
            *             (EOL)
            *         x
            *             !
            *                 (EOL)
            *             <space>
            */
        
            if (rde_param_i_symbol_void_start (p, 112)) return ;
            kleene_383 (p);
            rde_param_i_symbol_done_void (p, 112, 111);
            return;
        }
        
        static void kleene_383 (RDE_PARAM p) {
           /*
            * *
            *     /
            *         x
            *             '\'
            *             (EOL)
            *         x
            *             !
            *                 (EOL)
            *             <space>
            */
        
            while (1) {
                rde_param_i_state_push_2 (p);
                choice_381 (p);
                if (rde_param_i_kleene_close(p)) return;
            }
            return;
        }
        
        static void choice_381 (RDE_PARAM p) {
           /*
            * /
            *     x
            *         '\'
            *         (EOL)
            *     x
            *         !
            *             (EOL)
            *         <space>
            */
        
            rde_param_i_state_push_void (p);
            sequence_374 (p);
            if (rde_param_i_bra_void2void(p)) return;
            sequence_379 (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void sequence_374 (RDE_PARAM p) {
           /*
            * x
            *     '\'
            *     (EOL)
            */
        
            rde_param_i_state_push_void (p);
            rde_param_i_next_char (p, "\134", 110);
            if (rde_param_i_seq_void2void(p)) return;
            sym_EOL (p);
            rde_param_i_state_merge_void (p);
            return;
        }
        
        static void sequence_379 (RDE_PARAM p) {
           /*
            * x
            *     !
            *         (EOL)
            *     <space>
            */
        
            rde_param_i_state_push_void (p);
            notahead_38 (p);
            if (rde_param_i_seq_void2void(p)) return;
            rde_param_i_next_space (p, 60);
            rde_param_i_state_merge_void (p);
            return;
        }
        
    /* -*- c -*- */

	typedef struct PARSERg {
	    long int counter;
	    char     buf [50];
	} PARSERg;

	static void
	PARSERgRelease (ClientData cd, Tcl_Interp* interp)
	{
	    ckfree((char*) cd);
	}

	static const char*
	PARSERnewName (Tcl_Interp* interp)
	{
#define KEY "tcllib/parser/VMParser/critcl"

	    Tcl_InterpDeleteProc* proc = PARSERgRelease;
	    PARSERg*                  parserg;

	    parserg = Tcl_GetAssocData (interp, KEY, &proc);
	    if (parserg  == NULL) {
		parserg = (PARSERg*) ckalloc (sizeof (PARSERg));
		parserg->counter = 0;

		Tcl_SetAssocData (interp, KEY, proc,
				  (ClientData) parserg);
	    }

	    parserg->counter ++;
	    sprintf (parserg->buf, "VMathParser%d", (int)parserg->counter);
	    return parserg->buf;
#undef  KEY
	}

	static void
	PARSERdeleteCmd (ClientData clientData)
	{
	    /*
	     * Release the whole PARSER
	     * (Low-level engine only actually).
	     */
	    rde_param_del ((RDE_PARAM) clientData);
	}
	
	static int  COMPLETE (RDE_PARAM p, Tcl_Interp* interp);

	static int parser_PARSE  (RDE_PARAM p, Tcl_Interp* interp, int objc, Tcl_Obj* CONST* objv)
	{
	    int mode;
	    Tcl_Channel chan;

	    if (objc != 3) {
		Tcl_WrongNumArgs (interp, 2, objv, "chan");
		return TCL_ERROR;
	    }

	    chan = Tcl_GetChannel(interp,
				  Tcl_GetString (objv[2]),
				  &mode);

	    if (!chan) {
		return TCL_ERROR;
	    }

	    rde_param_reset (p, chan);
	    MAIN (p) ; /* Entrypoint for the generated code. */
	    return COMPLETE (p, interp);
	}

	static int parser_PARSET (RDE_PARAM p, Tcl_Interp* interp, int objc, Tcl_Obj* CONST* objv)
	{
	    char* buf;
	    int   len;

	    if (objc != 3) {
		Tcl_WrongNumArgs (interp, 2, objv, "text");
		return TCL_ERROR;
	    }

	    buf = Tcl_GetStringFromObj (objv[2], &len);

	    rde_param_reset (p, NULL);
	    rde_param_data  (p, buf, len);
	    MAIN (p) ; /* Entrypoint for the generated code. */
	    return COMPLETE (p, interp);
	}

	static int COMPLETE (RDE_PARAM p, Tcl_Interp* interp)
	{
	    if (rde_param_query_st (p)) {
		long int  ac;
		Tcl_Obj** av;

		rde_param_query_ast (p, &ac, &av);

		if (ac > 1) {
		    long int  lsc;
		    long int* lsv;
		    Tcl_Obj** lv = NALLOC (3+ac, Tcl_Obj*);

		    rde_param_query_ls (p, &lsc, &lsv);

		    memcpy(lv + 3, av, ac * sizeof (Tcl_Obj*));
		    lv [0] = Tcl_NewObj ();
		    lv [1] = Tcl_NewIntObj (1 + lsv [lsc-1]);
		    lv [2] = Tcl_NewIntObj (rde_param_query_cl (p));

		    Tcl_SetObjResult (interp, Tcl_NewListObj (3, lv));
		    ckfree ((char*) lv);
		} else {
		    Tcl_SetObjResult (interp, av [0]);
		}

		return TCL_OK;
	    } else {
		Tcl_Obj* xv [1];
		const ERROR_STATE* er = rde_param_query_er (p);
		Tcl_Obj* res = rde_param_query_er_tcl (p, er);

		xv [0] = Tcl_NewStringObj ("pt::rde",-1);
		Tcl_ListObjReplace(interp, res, 0, 1, 1, xv);

		Tcl_SetObjResult (interp, res);
		return TCL_ERROR;
	    }
	}

    static int parser_objcmd (ClientData cd, Tcl_Interp* interp, int objc, Tcl_Obj* CONST* objv)
	{
	    RDE_PARAM p = (RDE_PARAM) cd;
	    int m, res;

	    static CONST char* methods [] = {
		"destroy", "parse", "parset", NULL
	    };
	    enum methods {
		M_DESTROY, M_PARSE, M_PARSET
	    };

	    if (objc < 2) {
		Tcl_WrongNumArgs (interp, objc, objv, "option ?arg arg ...?");
		return TCL_ERROR;
	    } else if (Tcl_GetIndexFromObj (interp, objv [1], methods, "option",
					    0, &m) != TCL_OK) {
		return TCL_ERROR;
	    }

	    /* Dispatch to methods. They check the #args in
	     * detail before performing the requested
	     * functionality
	     */

	    switch (m) {
		case M_DESTROY:
		    if (objc != 2) {
			Tcl_WrongNumArgs (interp, 2, objv, NULL);
			return TCL_ERROR;
		    }

		Tcl_DeleteCommandFromToken(interp, (Tcl_Command) rde_param_query_clientdata (p));
		return TCL_OK;

		case M_PARSE:	res = parser_PARSE  (p, interp, objc, objv); break;
		case M_PARSET:	res = parser_PARSET (p, interp, objc, objv); break;
		default:
		/* Not coming to this place */
		ASSERT (0,"Reached unreachable location");
	    }

	    return res;
	}
    
    int VMathParserCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
    {
	/*
	 * Syntax: No arguments beyond the name
	 */

	RDE_PARAM   parser;
	CONST char* name;
	Tcl_Obj*    fqn;
	Tcl_CmdInfo ci;
	Tcl_Command c;

#define USAGE "?name?"

	if ((objc != 2) && (objc != 1)) {
	    Tcl_WrongNumArgs (interp, 1, objv, USAGE);
	    return TCL_ERROR;
	}

	if (objc < 2) {
	    name = PARSERnewName (interp);
	} else {
	    name = Tcl_GetString (objv [1]);
	}

	if (!Tcl_StringMatch (name, "::*")) {
	    /* Relative name. Prefix with current namespace */

	    Tcl_Eval (interp, "namespace current");
	    fqn = Tcl_GetObjResult (interp);
	    fqn = Tcl_DuplicateObj (fqn);
	    Tcl_IncrRefCount (fqn);

	    if (!Tcl_StringMatch (Tcl_GetString (fqn), "::")) {
		Tcl_AppendToObj (fqn, "::", -1);
	    }
	    Tcl_AppendToObj (fqn, name, -1);
	} else {
	    fqn = Tcl_NewStringObj (name, -1);
	    Tcl_IncrRefCount (fqn);
	}
	Tcl_ResetResult (interp);

	if (Tcl_GetCommandInfo (interp,
				Tcl_GetString (fqn),
				&ci)) {
	    Tcl_Obj* err;

	    err = Tcl_NewObj ();
	    Tcl_AppendToObj    (err, "command \"", -1);
	    Tcl_AppendObjToObj (err, fqn);
	    Tcl_AppendToObj    (err, "\" already exists", -1);

	    Tcl_DecrRefCount (fqn);
	    Tcl_SetObjResult (interp, err);
	    return TCL_ERROR;
	}

	parser = rde_param_new (sizeof(p_string)/sizeof(char*), (char**) p_string);
	c = Tcl_CreateObjCommand (interp, Tcl_GetString (fqn),
				  parser_objcmd, (ClientData) parser,
				  PARSERdeleteCmd);
	rde_param_clientdata (parser, (ClientData) c);
	Tcl_SetObjResult (interp, fqn);
	Tcl_DecrRefCount (fqn);
	return TCL_OK;
    }

int Vmparse_Init(Tcl_Interp* interp) {
	if (interp == 0) return TCL_ERROR;

	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}

	Tcl_PkgProvide(interp, "vmparse", "0.1");
	
	Tcl_CreateObjCommand(interp, "VMParser", VMathParserCmd , NULL, NULL);
	return TCL_OK;
}


