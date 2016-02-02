/* Merge Sort
   by Philip J. Erdelsky
   pje@efgh.com
   http://www.alumni.caltech.edu/~pje/
*/
#ifndef __MERGE_SORT_H__
#define __MERGE_SORT_H__


#define OK                   0
#define INSUFFICIENT_MEMORY  1
#define FILE_CREATION_ERROR  2
#define FILE_WRITE_ERROR     3



int merge_sort(FILE *unsorted_file, FILE *sorted_file,
              int (*read)(FILE *, void *, void *),   int (*write)(FILE *, void *, void *),
              int (*compare)(void *, void *, void *), void *pointer,
              unsigned max_record_size, unsigned long block_size, unsigned long *pcount );

  
  #endif //__MERGE_SORT_H__