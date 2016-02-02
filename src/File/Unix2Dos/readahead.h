
#ifndef H__READAHEAD
#define H__READAHEAD

#include <stdio.h>

class readahead
{
  public:
    readahead(const char *filespecs, unsigned size, bool binary = false);
    ~readahead();
    bool open() const {return fp != NULL;}
    int operator [](int i) const {return buffer[i];}
    void advance(unsigned count = 1);
    bool match(const char *s) const;
  private:
    int *buffer;
    unsigned psize;
    FILE *fp;
    bool eof;
    int get(void);
};



#endif