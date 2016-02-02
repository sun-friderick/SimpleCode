

// HTAB.H begins here

#ifndef H__HTAB
#define H__HTAB

typedef bool (*htab_match)(const void *p, const void *q, void *pointer);
typedef unsigned (*htab_code)(const void *p, void *pointer);

class htab
{
  private:
    struct item_block
    {
      item_block *next;
      void *item;
    } *queue[256];
    struct large_item_block
    {
      large_item_block *next;
      item_block buffer[256];
    } *large_block;
    unsigned blocks_in_large_block;
    htab_match pmatch;
    htab_code pcode;
    void *ppointer;
    item_block *insert_point;
    void *insert_item;
    unsigned insert_queue;
  public:
    htab(htab_match match, htab_code code, void *pointer);
    ~htab();
    static unsigned code(const char *name);
    static unsigned codei(const char *name);
    void *first(void *p);
    void *next(void);
    void insert(void);
    void *remove(void);
};

#endif

