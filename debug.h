/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#ifndef __DEBUG_H__
#define __DEBUG_H__

// Compiler specific differences...
#ifdef DEBUG_MODULE

#undef _D_
#ifndef __GNUC__
#define _D_	printf(">>>>" __FILE__ "(%d)\n", __LINE__);
#ifndef __FUNCTION__
#define __FUNCTION__ ""
#endif
#else
#define _D_	printf(">>>>" __FILE__ ":%s(%ld)\n", __FUNCTION__,(int)__LINE__);
#endif

#define DBG0MSG0(x) printf("%s: " x,__FUNCTION__)
#define DBG0MSG1(x,a) printf("%s: " x,__FUNCTION__,a)

#define FDBG0MSG0(fnc,x) printf("" #fnc ": " x)
#define FDBG0MSG1(fnc,x,a) printf("" #fnc ": " x,a)

#define _D1(fa,a) #a "=" fa
#define _D2(fa,a) "," #a "=" fa
#define _D3(a) ,a
#define DBG2ENTRY0() printf("%s(" ") called\n",__FUNCTION__)
#define DBG2ENTRY1(fa,a) \
    printf("%s(" \
    _D1(fa,a) \
    ") called\n",__FUNCTION__ \
    _D3(a) \
    )
#define DBG2ENTRY2(fa,a,fb,b) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) \
    )
#define DBG2ENTRY3(fa,a,fb,b,fc,c) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) _D3(c) \
    )
#define DBG2ENTRY4(fa,a,fb,b,fc,c,fd,d) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) _D3(c) _D3(d) \
    )
#define DBG2ENTRY5(fa,a,fb,b,fc,c,fd,d,fe,e) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) \
    )
#define DBG2ENTRY6(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) \
    )
#define DBG2ENTRY7(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) _D2(fg,g) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) _D3(g) \
    )
#define DBG2ENTRY8(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) _D2(fg,g) _D2(fh,h) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) _D3(g) _D3(h) \
    )
#define DBG2ENTRY9(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h,fi,i) \
    printf("%s(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) _D2(fg,g) _D2(fh,h) _D2(fi,i) \
    ") called\n",__FUNCTION__ \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) _D3(g) _D3(h) _D3(i) \
    )

#define FDBG2ENTRY0(fnc) printf("" #fnc "(" ") called\n" )
#define FDBG2ENTRY1(fnc,fa,a) \
    printf(#fnc "(" \
    _D1(fa,a) \
    ") called\n" \
    _D3(a) \
    )
#define FDBG2ENTRY2(fnc,fa,a,fb,b) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) \
    ") called\n" \
    _D3(a) _D3(b) \
    )
#define FDBG2ENTRY3(fnc,fa,a,fb,b,fc,c) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) \
    ") called\n" \
    _D3(a) _D3(b) _D3(c) \
    )
#define FDBG2ENTRY4(fnc,fa,a,fb,b,fc,c,fd,d) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) \
    ") called\n" \
    _D3(a) _D3(b) _D3(c) _D3(d) \
    )
#define FDBG2ENTRY5(fnc,fa,a,fb,b,fc,c,fd,d,fe,e) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) \
    ") called\n" \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) \
    )
#define FDBG2ENTRY6(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) \
    ") called\n" \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) \
    )
#define FDBG2ENTRY7(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) _D2(fg,g) \
    ") called\n" \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) _D3(g) \
    )
#define FDBG2ENTRY8(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) _D2(fg,g) _D2(fh,h) \
    ") called\n" \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) _D3(g) _D3(h) \
    )
#define FDBG2ENTRY9(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h,fi,i) \
    printf(#fnc "(" \
    _D1(fa,a) _D2(fb,b) _D2(fc,c) _D2(fd,d) _D2(fe,e) _D2(ff,f) _D2(fg,g) _D2(fh,h) _D2(fi,i) \
    ") called\n" \
    _D3(a) _D3(b) _D3(c) _D3(d) _D3(e) _D3(f) _D3(g) _D3(h) _D3(i) \
    )

#define NUM "%d"
#define UNUM "%u"
#define LNM "%ld"
#define LUNM "%lu"
#define FLT "%f"
#define DBL "%lf"
#define BYT "%02X"
#define WRD "%04X"
#define LNG "%08X"
#define STR "\"%s\""
#define PTR LNG
#define FLG NUM

#define DBG2EXITNULL() printf("%s() return\n",__FUNCTION__)
#define DBG2EXITVALUE(fa,a) printf("%s() return(" fa ")\n",__FUNCTION__,a)
#define DBG2EXITVALUE1(fa,a) printf("%s() return(" _D1(fa,a) ")\n",__FUNCTION__,a)
#define DBG2EXITVALUE2(a) printf("%s() return(" #a ")\n",__FUNCTION__)

#define FDBG2EXITNULL(f) printf("" #f "() return\n")
#define FDBG2EXITVALUE(f,fa,a) printf("" #f "() return(" fa ")\n",a)
#define FDBG2EXITVALUE1(f,fa,a) printf("" #f "() return(" _D1(fa,a) ")\n",a)
#define FDBG2EXITVALUE2(f,a) printf("" #f "() return(" #a ")\n")

#define DBG3MSG0(x) printf("%s: " x,__FUNCTION__)
#define DBG3MSG1(x,a) printf("%s: " x,__FUNCTION__,a)
#define DBG3MSG2(x,a,b) printf("%s: " x,__FUNCTION__,a,b)
#define DBG3MSG3(x,a,b,c) printf("%s: " x,__FUNCTION__,a,b,c)
#define DBG3MSG4(x,a,b,c,d) printf("%s: " x,__FUNCTION__,a,b,c,d)
#define DBG3MSG5(x,a,b,c,d,e) printf("%s: " x,__FUNCTION__,a,b,c,d,e)

#define FDBG3MSG0(fnc,x) printf("" #fnc ": " x)
#define FDBG3MSG1(fnc,x,a) printf("" #fnc ": " x,a)
#define FDBG3MSG2(fnc,x,a,b) printf("" #fnc ": " x,a,b)
#define FDBG3MSG3(fnc,x,a,b,c) printf("" #fnc ": " x,a,b,c)
#define FDBG3MSG4(fnc,x,a,b,c,d) printf("" #fnc ": " x,a,b,c,d)
#define FDBG3MSG5(fnc,x,a,b,c,d,e) printf("" #fnc ": " x,a,b,c,d,e)

#else

#define _D_
#define DEBUG_TARGET
#define DBG0MSG0(x)
#define DBG0MSG1(x,a)
#define TDBG0MSG0(x)
#define TDBG0MSG1(x,a)
#define FDBG0MSG0(fnc,x)
#define FDBG0MSG1(fnc,x,a)
#define TFDBG0MSG0(fnc,x)
#define TFDBG0MSG1(fnc,x,a)
#define _D1(fa,a)
#define _D2(fa,a)
#define _D3(a)
#define DBG2ENTRY0()
#define DBG2ENTRY1(fa,a)
#define DBG2ENTRY2(fa,a,fb,b)
#define DBG2ENTRY3(fa,a,fb,b,fc,c)
#define DBG2ENTRY4(fa,a,fb,b,fc,c,fd,d)
#define DBG2ENTRY5(fa,a,fb,b,fc,c,fd,d,fe,e)
#define DBG2ENTRY6(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f)
#define DBG2ENTRY7(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g)
#define DBG2ENTRY8(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h)
#define DBG2ENTRY9(fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h,fi,i)
#define FDBG2ENTRY0(fnc)
#define FDBG2ENTRY1(fnc,fa,a)
#define FDBG2ENTRY2(fnc,fa,a,fb,b)
#define FDBG2ENTRY3(fnc,fa,a,fb,b,fc,c)
#define FDBG2ENTRY4(fnc,fa,a,fb,b,fc,c,fd,d)
#define FDBG2ENTRY5(fnc,fa,a,fb,b,fc,c,fd,d,fe,e)
#define FDBG2ENTRY6(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f)
#define FDBG2ENTRY7(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g)
#define FDBG2ENTRY8(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h)
#define FDBG2ENTRY9(fnc,fa,a,fb,b,fc,c,fd,d,fe,e,ff,f,fg,g,fh,h,fi,i)

#define NUM "%d"
#define UNUM "%u"
#define LNM "%ld"
#define LUNM "%lu"
#define FLT "%f"
#define DBL "%lf"
#define BYT "%02X"
#define WRD "%04X"
#define LNG "%08X"
#define STR "\"%s\""
#define PTR LNG
#define FLG NUM
#define DBG2EXITNULL()
#define DBG2EXITVALUE(fa,a)
#define DBG2EXITVALUE1(fa,a)
#define DBG2EXITVALUE2(a)
#define FDBG2EXITNULL(fnc)
#define FDBG2EXITVALUE(fnc,fa,a)
#define FDBG2EXITVALUE1(fnc,fa,a)
#define FDBG2EXITVALUE2(fnc,a)
#define DBG3MSG0(x)
#define DBG3MSG1(x,a)
#define DBG3MSG2(x,a,b)
#define DBG3MSG3(x,a,b,c)
#define DBG3MSG4(x,a,b,c,d)
#define DBG3MSG5(x,a,b,c,d,e)
#define FDBG3MSG0(fnc,x)
#define FDBG3MSG1(fnc,x,a)
#define FDBG3MSG2(fnc,x,a,b)
#define FDBG3MSG3(fnc,x,a,b,c)
#define FDBG3MSG4(fnc,x,a,b,c,d)
#define FDBG3MSG5(fnc,x,a,b,c,d,e)

#endif /* DEBUG_MODULE */

#endif /* __DEBUG_H__ */
