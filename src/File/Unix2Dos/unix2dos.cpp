


// converts Unix text files to DOS format by replacing \n with \r\n

#include <stdio.h>
//#include <dir.h>
#include "readahead.h"

void unix2dos(int argc, char **argv)
{
  for (int i = 1; i < argc; i++)
  {
    readahead *f = new readahead(argv[i], 3);
    printf("%s:\n", argv[i]);
    if (!f->open())
    {
      fputs("Can't open file\n", stderr);
      continue;
    }
    FILE *t = tmpfile();
    while ((*f)[0] != EOF)
    {
      if ((*f)[0] == '\r' && (*f)[1] == '\n')
      {
        fputc('\n', t);
        f->advance(2);
      }
      else if ((*f)[0] == '\n')
      {
        fputc('\n', t);
        f->advance(1);
      }
      else
      {
        fputc((*f)[0], t);
        f->advance(1);
      }
    }
    delete f;
    rewind(t);
    FILE *ff = fopen(argv[i], "w");
    if (ff == NULL)
    {
      fputs("Can't modify file\n", stderr);
      continue;
    }
    int c;
    while ((c = fgetc(t)) != EOF)
      fputc(c, ff);
    fclose(t);
    fclose(ff);
  }
}



