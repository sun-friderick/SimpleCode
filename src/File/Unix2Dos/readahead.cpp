


// File reading package with read-ahead

#include "readahead.h"

readahead::readahead(const char *filespecs, unsigned size, bool binary)
{
  psize = size;
  buffer = new int[psize];
  fp = fopen(filespecs, binary ? "rb" : "r");
  eof = fp == NULL;
  unsigned i;
  for (i = 0; i < psize; i++)
    buffer[i] = get();
}

readahead::~readahead()
{
  if (open())
  {
    delete [] buffer;
    fclose(fp);
    fp = NULL;
  }
}

void readahead::advance(unsigned count)
{
  while (count > 0)
  {
    unsigned i;
    for (i = 0; i < psize-1; i++)
      buffer[i] = buffer[i+1];
    buffer[i] = get();
    count--;
  }
}

int readahead::get(void)
{
  int c;
  if (eof)
    c = EOF;
  else
  {
    c = fgetc(fp);
    if (c == EOF)
      eof = true;
  }
  return c;
}

bool readahead::match(const char *s) const
{
  int i;
  for (i = 0; s[i] != 0; i++)
  {
    if (s[i] != (char) buffer[i])
      return false;
  }
  return true;
}
