/* ZSORT.H begins here */

#ifndef H__ZSORT
#define H__ZSORT

typedef int (*ZSORTCOMPARE)(void *, void *, void *);

typedef struct ZSORT_handle
{
  int dummy;
} *ZSORT;

#ifdef __cplusplus
extern "C" {
#endif

extern ZSORT ZsortOpen(unsigned, ZSORTCOMPARE, unsigned, void *);
extern int   ZsortSubmit(ZSORT, const void *p);
extern int   ZsortRetrieve(ZSORT, void *p);
extern void  ZsortClose(ZSORT, int);

#ifdef __cplusplus
}
#endif

#define ZSORTEND        1
#define ZSORTABORT      2
#define ZSORTMEMFAIL    3
#define ZSORTFILEERR    4

/* These values must be in the range of type "int" and outside the range */
/* of numbers oridnarily returned by the comparison function.            */

#define ZCOMPABORT    0x80000000
#define ZCOMPDUPP     0x80000001
#define ZCOMPDUPQ     0x80000002

#endif
