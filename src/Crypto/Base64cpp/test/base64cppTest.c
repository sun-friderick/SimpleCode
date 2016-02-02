/*
c-example1.c - libb64 example code

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64

This is a short example of how to use libb64's C interface to encode
and decode a string directly.
The main work is done between the START/STOP ENCODING and DECODING lines.

Note that this is extremely simple; you will almost never have all the data
in a string ready to encode/decode!
Because we all the data to encode/decode in a string, and know its length,
we can get away with a single en/decode_block call.
For a more realistic example, see c-example2.c
*/
/*
c-example2.c - libb64 example code

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64

This is a short example of how to use libb64's C interface to encode
and decode a file directly.
The main work is done between the START/STOP ENCODING and DECODING lines.
The main difference between this code and c-example1.c is that we do not
know the size of the input file before hand, and so we use to iterate over
encoding and decoding the data.
*/

#include "../includes/cencode.h"
#include "../includes/cdecode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

/* arbitrary buffer size */
#define SIZE 100


#define TEST1

#ifdef TEST1


/**************************************************************************************/
/******                 c-example1.c - libb64 example code                    *********/
/**************************************************************************************/
/*
    c-example1.c: shows how to encode/decode a single string
*/
char* encode1(const char* input)
{
	/* set up a destination buffer large enough to hold the encoded data */
	char* output = (char*)malloc(SIZE);
	/* keep track of our encoded position */
	char* c = output;
	/* store the number of bytes encoded by a single call */
	int cnt = 0;
	/* we need an encoder state */
	base64_encodestate s;
	
	/*---------- START ENCODING ----------*/
	/* initialise the encoder state */
	base64_init_encodestate(&s);
	/* gather data from the input and send it to the output */
	cnt = base64_encode_block(input, strlen(input), c, &s);
	c += cnt;
	/* since we have encoded the entire input string, we know that 
	   there is no more input data; finalise the encoding */
	cnt = base64_encode_blockend(c, &s);
	c += cnt;
	/*---------- STOP ENCODING  ----------*/
	
	/* we want to print the encoded data, so null-terminate it: */
	*c = 0;
	
	return output;
}

char* decode1(const char* input)
{
	/* set up a destination buffer large enough to hold the encoded data */
	char* output = (char*)malloc(SIZE);
	/* keep track of our decoded position */
	char* c = output;
	/* store the number of bytes decoded by a single call */
	int cnt = 0;
	/* we need a decoder state */
	base64_decodestate s;
	
	/*---------- START DECODING ----------*/
	/* initialise the decoder state */
	base64_init_decodestate(&s);
	/* decode the input data */
	cnt = base64_decode_block(input, strlen(input), c, &s);
	c += cnt;
	/* note: there is no base64_decode_blockend! */
	/*---------- STOP DECODING  ----------*/
	
	/* we want to print the decoded data, so null-terminate it: */
	*c = 0;
	
	return output;
}

int main_test1()
{
	const char* input = "hello world";
	char* encoded;
	char* decoded;
	
    printf("c-example1.c: shows how to encode/decode a single string \n");
    
	/* encode the data */
	encoded = encode1(input);
	printf("encoded: %s", encoded); /* encoded data has a trailing newline */

	/* decode the data */
	decoded = decode1(encoded);
	printf("decoded: %s\n", decoded);
	
	/* compare the original and decoded data */
	assert(strcmp(input, decoded) == 0);
	
	free(encoded);
	free(decoded);
	return 0;
}





/**************************************************************************************/
/********                  c-example2.c - libb64 example code                  ********/
/**************************************************************************************/
/*
    c-example2.c: shows how to directly encode/decode a file
*/
void encode2(FILE* inputFile, FILE* outputFile)
{
	/* set up a destination buffer large enough to hold the encoded data */
	int size = SIZE;
	char* input = (char*)malloc(size);
	char* encoded = (char*)malloc(2*size); /* ~4/3 x input */
	/* we need an encoder and decoder state */
	base64_encodestate es;
	/* store the number of bytes encoded by a single call */
	int cnt = 0;
	
	/*---------- START ENCODING ----------*/
	/* initialise the encoder state */
	base64_init_encodestate(&es);
	/* gather data from the input and send it to the output */
	while (1)
	{
		cnt = fread(input, sizeof(char), size, inputFile);
		if (cnt == 0) break;
		cnt = base64_encode_block(input, cnt, encoded, &es);
		/* output the encoded bytes to the output file */
		fwrite(encoded, sizeof(char), cnt, outputFile);
	}
	/* since we have reached the end of the input file, we know that 
	   there is no more input data; finalise the encoding */
	cnt = base64_encode_blockend(encoded, &es);
	/* write the last bytes to the output file */
	fwrite(encoded, sizeof(char), cnt, outputFile);
	/*---------- STOP ENCODING  ----------*/
	
	free(encoded);
	free(input);
}

void decode2(FILE* inputFile, FILE* outputFile)
{
	/* set up a destination buffer large enough to hold the decoded data */
	int size = SIZE;
	char* encoded = (char*)malloc(size);
	char* decoded = (char*)malloc(1*size); /* ~3/4 x encoded */
	/* we need an encoder and decoder state */
	base64_decodestate ds;
	/* store the number of bytes encoded by a single call */
	int cnt = 0;
	
	/*---------- START DECODING ----------*/
	/* initialise the encoder state */
	base64_init_decodestate(&ds);
	/* gather data from the input and send it to the output */
	while (1)
	{
		cnt = fread(encoded, sizeof(char), size, inputFile);
		if (cnt == 0) break;
		cnt = base64_decode_block(encoded, cnt, decoded, &ds);
		/* output the encoded bytes to the output file */
		fwrite(decoded, sizeof(char), cnt, outputFile);
	}
	/*---------- START DECODING  ----------*/
	
	free(encoded);
	free(decoded);
}

int main_test2(int argc, char** argv)
{
	FILE* inputFile;
	FILE* encodedFile;
	FILE* decodedFile;
	
    printf("c-example2.c: shows how to directly encode/decode a file.\n");
    
	if (argc < 4)
	{
		printf("please supply three filenames: input, encoded & decoded\n");
		exit(-1);
	}
	
	/* encode the input file */
	inputFile   = fopen(argv[1], "r");
	encodedFile = fopen(argv[2], "w");
	
	encode2(inputFile, encodedFile);
	
	fclose(inputFile);
	fclose(encodedFile);

	/* decode the encoded file */
	encodedFile = fopen(argv[2], "r");
	decodedFile = fopen(argv[3], "w");
	
	decode2(encodedFile, decodedFile);
	
	fclose(encodedFile);
	fclose(decodedFile);

	/* we leave the original vs. decoded data comparison to 
		diff in the Makefile */
		
	return 0;
}

#endif



/**********************************************************************************/
/***************                     main                       *******************/
/**********************************************************************************/
/*
    test-c-example1: c-example1
        ./c-example1

    test-c-example2: c-example2
        ./c-example2 loremgibson.txt encoded.txt decoded.txt
        diff -q loremgibson.txt decoded.txt
        
        loremgibson.txt?
            ----------------
            The loremgibson.txt file is a plain text containing content from 
            here: http://loremgibson.com/
            It is used in c-example2.c as the input data file.
        
    test: test-c-example1 test-c-example2
*/
int showUseage()
{
    printf("Useage: \n\n");
    printf("\t 1.no args, ./test : ");
    printf("\t\t\t shows how to encode/decode a single string, will create encoded.txt && decoded.txt files.\n\n");
    
    printf("\t 2.three args, ./test loremgibson.txt encoded.txt decoded.txt\n");
    printf("\t\t\t shows how to directly encode/decode a file, and then run \" diff -q loremgibson.txt decoded.txt\"\n\n");
    
    printf("\t loremgibson.txt?\n");
    printf("\t\t\t The loremgibson.txt file is a plain text containing content from here: http://loremgibson.com/\n");
    printf("\t\t\t It is used in c-example2.c as the input data file.\n\n\n");
    
    return 0;
}





int main(int argc, char** argv)
{
    showUseage();
    
	if (argc == 1) {
		printf("test  c-example1.c - libb64 example code  \n");
        //main_test1();
		exit(-1);
	}else if (argc > 1 && argc < 4){
		printf("please supply three filenames: input, encoded & decoded \n");
		exit(-1);
	} else{
        printf("test c-example2.c - libb64 example code  \n");
        //main_test2(argc, argv);
    }
			
	return 0;
}

