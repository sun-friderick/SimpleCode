
/* Test program begins here */

/*---------------------------------------------------------------------------
The test program reads all words from standard input, where a word is a
series of letters, apostrophes and hyphens, beginning with a letter,
preceded by a non-letter and followed by a character that is not a
letter, apostrophe or hyphen. Each word is converted to all uppercase
and is truncted to 15 characters if it is more than 15 characters long.

Each word is accompanied by a count, which is initialized to 1.

The words are sorted into alphabetical order, removing duplicates. When
duplicates are removed, the count of the one removed is added to the
count of the one retained. This produces an alphabetical list of words
and the number of times each one appeared in the standard input.

Finally, the list is again sorted into order of descending count. Words with
equal counts are kept in alphabetical order.

The result is a list of word counts, with the most frequently used words
at the top of the list.
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <ctype.h>
#include "ZSort.h"


#define WORD_SIZE 15
#define MEMORY    10

struct item_tag
{
  long count;
  char word[WORD_SIZE+1];
};

int compare_words(struct item_tag *p, struct item_tag *q, void *pointer)
{
  int n = strcmp(p->word, q->word);
  if (n == 0)
  {
    p->count += q->count;
    return ZCOMPDUPP;
  }
  return n;
}

int compare_counts(struct item_tag *p, struct item_tag *q, void *pointer)
{
  long n = q->count - p->count;
  return n == 0 ? strcmp(p->word, q->word) : n;
}

void main(void)
{
  ZSORT h1, h2;
  int c;
  struct item_tag item;
  h1 = ZsortOpen(sizeof(item), (ZSORTCOMPARE) compare_words, MEMORY, NULL);
  c = getchar();
  while (1)
  {
    int i = 0;
    while (c != EOF && !isalpha(c))
      c = getchar();
    if (c == EOF)
      break;
    while (isalpha(c) || c == '\'' || c == '-')
    {
      if (i < WORD_SIZE)
        item.word[i++] = toupper(c);
      c = getchar();
    }
    item.word[i] = 0;
    item.count = 1;
    ZsortSubmit(h1, &item);
  }
  h2 = ZsortOpen(sizeof(item), (ZSORTCOMPARE) compare_counts,
    MEMORY, NULL);
  while (ZsortRetrieve(h1, &item) == 0)
    ZsortSubmit(h2, &item);
  ZsortClose(h1, 1);
  while (ZsortRetrieve(h2, &item) == 0)
    printf("%10d %s\n", item.count, item.word);
  ZsortClose(h2, 1);
}

