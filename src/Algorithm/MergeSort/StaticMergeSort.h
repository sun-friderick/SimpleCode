/* Stable Merge Sort
   by Philip J. Erdelsky
   pje@efgh.com
   http://www.alumni.caltech.edu/~pje/
*/
#ifndef __STATIC_MERGE_SORT_H__
#define __STATIC_MERGE_SORT_H__

int stable_merge_sort(FILE *unsorted_file, FILE *sorted_file,
                      int (*read)(FILE *, void *, void *),  int (*write)(FILE *, void *, void *),
                      int (*compare)(void *, void *, void *), void *pointer,
                      unsigned max_record_size, unsigned long block_size,  unsigned long *record_count);
                      
                      
                      
#endif //__STATIC_MERGE_SORT_H__        