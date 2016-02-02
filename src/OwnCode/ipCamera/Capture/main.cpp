
#include <string>
extern "C"{
	#include <stdlib.h>
	#include <unistd.h>
	#include <stdio.h>
	#include <fcntl.h>
	#include <errno.h>
	#include <sys/ioctl.h>
}

#include "V4L2/V4L2.h"

#define DEV_NAME "/dev/video0"
#define DEFAULT_WIDTH    	640
#define DEFAULT_HEIFHT 	480

using namespace std;

int run (V4L2 v4l2);
int main(int argc, char** argv)
{
	std::string devName(DEV_NAME);
	V4L2 v4l2(devName);

	v4l2.OpenDevice();
	v4l2.InitDevice(DEFAULT_WIDTH, DEFAULT_HEIFHT);

	v4l2.StartCapturing();

	run(v4l2);

	v4l2.StopCapturing();

	v4l2.UninitDevice();
	v4l2.CloseDevice();
	return 0;
}

static int cnt = 0;
#define JPG_TEST         		"./images/image_test%d.jpg"
#define IMG_NAME_LEN	    	32
int time_in_sec_capture = 5;
int run (V4L2 v4l2)
{
	int frames;
	AVPicture Picture;
	int fd = v4l2.GetDeviceFd();

	FILE  *yuvFp;
	char yuvName[] = "sun.yuv";
	yuvFp = fopen(yuvName, "wa+");

	frames = 30 * time_in_sec_capture;

	while (frames-- > 0) {
		for ( ; ; ) {
			fd_set fds;
			struct timeval tv;
			int r;
			FD_ZERO (&fds);
			FD_SET (fd, &fds);

			/*Timeout*/
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select (fd + 1, &fds, NULL, NULL, &tv);
			if (-1 == r) {
				if (EINTR == errno)
					continue;
				perror("Fail to select");
               			exit(EXIT_FAILURE);
			}else if (0 == r) {
				fprintf (stderr, "select timeout\n");
				exit (EXIT_FAILURE);
			}

			#if 1
			if (v4l2.ReadFrame())
				break;
			#else	
			avpicture_alloc(&Picture, PIX_FMT_YUYV422, v4l2.getWidth(), v4l2.getHeight());
			if(v4l2.ReadFrame(Picture, PIX_FMT_YUYV422,  v4l2.getWidth(), v4l2.getHeight()) == 0){
				cnt++;
				printf("----v4l2.ReadFrame, cnt[%d]\n", cnt);
				int i = 0;

				//写yuv文件
				for(i = 0; i < 4; i++){
					printf("-------fwrite Picture.data[%d].\n", i);
					fwrite(Picture.data[i], Picture.linesize[i], 1, yuvFp);
				}

				FILE *fp;
				static int num = 0;
				char imageName[IMG_NAME_LEN];
				sprintf(imageName, JPG_TEST, num++);
				if ((fp = fopen(imageName, "w")) == NULL) {
					perror("Fail to fopen");
					exit(EXIT_FAILURE);
				}
				for(i = 0; i < 4; i++){
					printf("-------fwrite Picture.data[%d].\n", i);
					fwrite(Picture.data[i], Picture.linesize[i], 1, fp);
				}
				usleep(500);
				fclose(fp);
				printf("---v4l2.ReadFrame\n");
				break;
			}
			avpicture_free(&Picture);
			#endif
		}
	}
	fclose(yuvFp);
	return 0;
}

