
/**
* 将数组文件 拆分为 单个的gif文件；
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FILE_CONTENT_SIZE (2048*1000*2)
#define BUF_SIZE (8192*1000*2)


int n = 1;
char name_surffix[3][5] = { 
              {'.', 'g', 'i', 'f', '\0'}
            , {'.', 'p', 'n', 'g', '\0'}
            , {'.', 'j', 'p', 'g', '\0'} 
            
            , {'.', 'b', 'm', 'p', '\0'} 
            , {'.', 'r', 'a', 'w', '\0'} 
            };

int main(int argc, char* argv[])
{
	FILE* fp1 = NULL;
	FILE* fp2 = NULL;
	int fd = 0;
	char file_name[64] = {};
	char temp_buf[256] = {}; 
	char *p1 = NULL, *p2 = NULL, *p3 = NULL;
	int ret = 0, key = 0;
	char* pret = NULL;
	int	img_type = 0; //0,gif;  1,png;  2,jpg;
		
		char *p_f1 = NULL, *p = NULL, *p0 = NULL;
		char a[5] = {};
		int a2i = 0;
		unsigned char c_a[2] = {};
	int k = 0;
	
    char f_name[32] = {};
    if(argc == 0) {
        strcpy(f_name, "test.txt");
    } else {
        strcpy(f_name, argv[1]);
    }
    
	char* file_content = malloc(FILE_CONTENT_SIZE);
	if(file_content == NULL)
		printf("malloc file_content err\n");
	char* buffer = malloc(BUF_SIZE);
	if(buffer == NULL)
		printf("malloc buffer err\n");
	memset(buffer, 0, BUF_SIZE);
	memset(file_content, 0, FILE_CONTENT_SIZE);
	
	//read source file
	fp1 = fopen("test.txt", "rb");
	if (ret == -1)
		printf("  fopen err! \n");
	fseek(fp1, 0, SEEK_SET);
	fread( buffer, 1, BUF_SIZE, fp1);
	fclose(fp1);
	fp1 = NULL;
	
	//split file 
	p3 = buffer;
	while(p3){
		//get name for new file
		memset(file_name, 0, sizeof(file_name));
		memset(file_content, 0, FILE_CONTENT_SIZE);
		p1 = strstr(p3, "app_gif_");
		if(p1 == NULL){
			p3 = p1;
			printf(" strstr app_gif_ err\n");
			break;
		}
		p2 = strstr(p3, "[");
		if(p2 == NULL)
			printf("strstr [ err\n");
		pret = strncpy(file_name, p1, (p2 - p1));
		if(pret == NULL)
			printf(" strncpy file name err\n");
		
		//get file type
		p3 = p2;
		strncpy(temp_buf, p3, 255);
		temp_buf[255] = '\0';
		printf("p1=%p, p2=%p,  p3=%p\n", p1, p2, p3);
		p1 = strstr(temp_buf, "0x47, 0x49, 0x46, 0x38,");  //gif
		if(p1 == NULL){
			p1 = strstr(temp_buf, "0x89, 0x50, 0x4e, 0x47,"); //png
			if(p1 == NULL){
				p1 = strstr(temp_buf, "0xff, 0xd8, 0xff, 0xe1,"); //jpg(jfif)
				if(p1 == NULL){
					p1 = strstr(temp_buf, "0xff, 0xd8, 0xff, 0xe0,"); //jpg(jfif)
					if(p1 == NULL){
						printf(" strstr 0x47, 0x49, 0x46, 0x38, err\n");
					} else{
						img_type = 2;
						printf("this is a jpg image \n");
					}
				} else{
					img_type = 2;
					printf("this is a jpg image \n");
				}
			} else{
				img_type = 1;
				printf("this is a png image \n");
			}
		} else{
			img_type = 0;
			printf("this is a gif image \n\n");
		}
		pret = strcpy(file_name + strlen(file_name), name_surffix[img_type]);
		if(pret == NULL)
			printf(" strncpy file name err\n");
		printf("file name is [%s] \n", file_name);
		
		//get file contents
		p1 = &p3[p1-temp_buf]; //image file header location
		p2 = strstr(p3, "}");
		if(p2 == NULL)
			printf("strstr } err\n");
		pret = strncpy(file_content, p1, (p2 - p1));
		if(pret == NULL)
			printf(" strncpy file content err\n");
		p3 = p2;
		printf(" file content is [%s]\n", file_content);

		//to solute duplicated file name
		fd = open(file_name, O_RDONLY);
		if (fd != -1){
			printf("file name [%s] is exist\n", file_name);
			ret = sprintf(file_name, "%s%d", file_name, n);
			if(ret == -1)
				printf(" sprintf file name err\n");
			n = n + 1;
		}
		close(fd);
		
		//get real contents and write
		p_f1 = file_content;
		p = strstr(p_f1, "0x");
		p0 = strstr(p_f1, ",");
		printf("  while p \n");
		
		fp2 = fopen(file_name, "wb+");  //, O_RDWR | O_APPEND | O_CREAT
		while(p){
			memset(a, 0, 5);
			memset(c_a, 0, 2);
			pret = strncpy(a, p, 4);
			a[4] = '\0';
			if (pret == NULL)
				printf(" strncpy  err\n");
			printf("str hex is [%s]\n", a);

			//convert to hex
			a2i = strtol(a, NULL, 16);
			c_a[0] = (unsigned char)a2i;
			c_a[1] = '\0';
			printf(" hex is [%02x] [%c] -[%c] \n", a2i, a2i, c_a[0]);
			//printf("%c",c_a[0]);
			fwrite( c_a, sizeof(unsigned char), 1, fp2);
			k =  k + 1;
			if ((k % 16) == 0)    
				;
				//printf("\n\n\n");
			printf("fwrite over\n");
			
			// get game over flags
			if(img_type == 0){  //gif
				if(strcmp(a, "0x3b") == 0)
					if(p[4] == '}' || p[4] == '\n' || p[5] == '\n')
						break;
			}else if(img_type == 1){ //png
				if(strcmp(a, "0x82") == 0)
					if(p[4] == '}' || p[4] == '\n' || p[5] == '\n')
						break;
			}else if(img_type == 2){  //jpg
				if(strcmp(a, "0xd9") == 0)
					if(p[4] == '}' || p[4] == '\n' || p[5] == '\n')
						break;
			}else 
				printf(" img_type err \n");
			printf("get over flag\n");
			
			p_f1 = p0 + 1;
			p = strstr(p_f1, "0x");
			p0 = strstr(p_f1, ",");
		} //end while
		fclose(fp2);
		a2i = 0;
		key = key + 1;
		
		printf("\n this is the [%d]th file [%s] create complate \n\n\n\n\n", key, file_name);
	} //end while
	printf("\n game over\n");
	
	return 0;
}






