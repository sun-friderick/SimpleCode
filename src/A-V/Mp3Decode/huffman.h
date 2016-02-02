/**********************************************************************
 * MPEG/audio ����/�������                                           *
 * VERSION 4.1                                                        *
 *********************************************************************/	
 
#define HUFFBITS unsigned long int
#define HTN	34
#define MXOFF	250
 
struct huffcodetab {
  char tablename[3];	/*�ַ�, ���� table_description	*/
  unsigned int xlen; 	
  unsigned int ylen;	
  unsigned int linbits; /* linbits	��Ŀ 		*/
  unsigned int linmax;	/*�洢��linbits�е������Ŀ 	*/
  int ref;		/*a positive value indicates a reference*/
  HUFFBITS *table;	/*ָ��array[xlen][ylen]��ָ��		*/
  unsigned char *hlen;	/*ָ�� array[xlen][ylen]��ָ��		*/
  unsigned char(*val)[2];/*������				*/ 
  unsigned int treelen;	/*����������		*/
};

extern struct huffcodetab ht[HTN];/* ȫ���ڴ��		*/
				/* ����huffcodtable headers������	*/
				/* 0..31 Huffman code table 0..31	*/
				/* 32,33 count1-tables			*/
extern int read_decoder_table(FILE *);
extern int huffman_decoder(struct huffcodetab *, int *, int*, int*, int*);
