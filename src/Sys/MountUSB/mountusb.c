#include "mountusb.h"

static pthread_mutex_t mount_mutex;
static char devlist[8][MAX_PART];
static int deamon_mode = 0;

static int load_procfile(char* filename, char* buf);

//�õ����е�U�֣��������豸�б��Էֺŷָ�
static int get_udisk_dev_list()
{
	int major, minor;
	long long blocks;
	char name[256], line[256];
	int len, fd, partnum=0;
	FILE *f;
	char *frc, *iobuf = NULL;
	
	fd = open("/proc/partitions", O_RDONLY, 0);
	if (fd < 0)
	{
		//fprintf(stderr, "Failed to open /proc/partitions: %s\n", strerror(errno));
		return -1;
	}
	iobuf = (char *)malloc(16384);
	if (!iobuf)
	{
		//fprintf(stderr, "out of memory.\n");
		return -1;
	}
	len = read(fd, iobuf, 16384);
	close(fd);
	if (len < 0)
	{
		fprintf(stderr, "failed to read /proc/partitions: %s\n", strerror(errno));
		return -1;
	}
	memset(devlist, 0, 256);
	f = (FILE*)fmemopen((void*)iobuf, len, "r");
	
	frc = fgets(line, sizeof(line), f);
	if (frc)
		frc = fgets(line, sizeof(line), f);
	if (frc)
		frc = fgets(line, sizeof(line), f);
	while(frc)
	{
		if (sscanf(line, "%d %d %lld %s", &major, &minor, &blocks, name) != 4)
		{
			fprintf(stderr, "invalid line in /proc/partitions: '%s'\n", line);
			if (iobuf)
			free(iobuf);
			fclose(f);
			return -1;
		}
		if ((blocks > 10) && (major==8))
		{
			char buf[16];
			sprintf(buf, "/dev/%s", name);
			if (access(buf, R_OK)==0)
			strncpy(devlist[partnum++], name, 8);
				//sprintf(devlist, "%s;%s", devlist, name);
			PRINTF("Part %d: %s found\n", partnum, name);
		}
		frc = fgets(line, sizeof(line), f);
	}
	if (iobuf)
	free(iobuf);
	fclose(f);
	return partnum;
}

//���ָ����U�̣���������Ŀ��Ŀ¼�Ƿ��Ѿ�mount���ˡ�
static int check_mount (char *device_name, char *mnt_dir)
{
	FILE *f;
	struct mntent *mnt;
	
	if((f = setmntent("/proc/mounts", "r")) == NULL)
		return 0;

	while((mnt = getmntent(f)) != NULL) {
		//PRINTF("device: %s mount on: %s\n", mnt->mnt_fsname, mnt->mnt_dir);
		if (device_name && (strcmp(device_name, mnt->mnt_fsname) == 0))
			goto quit;

		if (mnt_dir && (strcmp(mnt_dir, mnt->mnt_dir) == 0))
			goto quit;
	}
	endmntent( f);
	return 0;

quit:
	PRINTF("Checking mount: %s or %s have been mounted\n", device_name, mnt_dir);
	return 1;
}

static int get_mounted_dir(char *device_name, char* dir_name)
{
	FILE *f;
	struct mntent *mnt;

	if ((f = setmntent("/proc/mounts", "r")) == NULL)
		return -1;

	while ((mnt = getmntent(f)) != NULL) {
		if (strcmp(device_name, mnt->mnt_fsname) == 0) {
			strcpy(dir_name, mnt->mnt_dir);
			endmntent(f);
			return 0;
		}
	}
	endmntent(f);

	return -1;	
}

//��ָ����U�̣��������豸mount��Ŀ��Ŀ¼�ϡ�
static int mountusb2dir(char *dev, char *target)
{
#ifdef USE_CMD
	char *argv[] = { "/bin/mount", "-t", "vfat", NULL, NULL, NULL};
	pid_t pid;
	int status;
	int errors = 0;
	
	if ((!dev) || (!target))
		return 1;
	argv[3] = dev;
	argv[4] = target;
	if (!(pid = vfork()))
	{
		execv(argv[0], argv);
		_exit(1);
	}
	waitpid(pid, &status, 0);
	if (!((((__extension__ ({ union { __typeof(status) __in; int __i; } __u; __u.__in = (status); __u.__i; }))) & 0x7f) == 0) || ((((__extension__ ({ union { __typeof(status) __in; int __i; } __u; __u.__in = (status); __u.__i; }))) & 0xff00) >> 8))
	{
		fprintf(stderr, "/bin/mount failed\n");
		errors = -1;
	}
	
	return errors;
#else
	char buf[512];
	int ret = 1;
	if (!load_procfile("/proc/filesystems", buf))
		return 1;
	if (strstr(buf, "vfat")) {
		PRINTF("Try to mount %s to %s as VFAT\n", dev, target);
		ret = mount(dev, target, "vfat", MS_NOEXEC, "iocharset=utf8,shortname=mixed");
	}
	if ((ret) && (strstr(buf, "ext3"))) {
		PRINTF("Try to mount %s to %s as EXT3\n", dev, target);
		ret = mount(dev, target, "ext3", MS_NOEXEC, 0);
	}
	if ((ret) && (!access("/home/yx5532/bin/ntfs-3g.elf", R_OK))) {
		int pid;
		PRINTF("Try to mount %s as NTFS\n", dev);
		if (!(pid = vfork())) {
			PRINTF("Run ntfs-3g.elf %s %s\n", dev, target);
			ret = execl("/home/yx5532/bin/ntfs-3g.elf", "/home/yx5532/bin/ntfs-3g.elf", dev, target, NULL);
			PRINTF("ntfs-3g.elf return %d\n", ret);
			_exit(ret);
		}
		waitpid(pid, &ret, 0);
		//ret = mount(dev, target, "ntfs", 0, "iocharset=gb2312");
	}
	PRINTF("%s end\n", __FUNCTION__);
	return ret;
#endif	
}

/* Mount���е�U�̣���������*/
static int mount_all_usb()
{
	char buf[12], tmp[12];
	int i, rc;
//	int cur_dir_no = 0;
	int partnum = get_udisk_dev_list();

#ifdef MS_DIRSYNC
	int flags = MS_SYNCHRONOUS | MS_NOATIME | MS_NODIRATIME | MS_NOSUID; /* sync mode is better for removeable */
	flags |= MS_DIRSYNC; 
#endif

	i = 0;
	while (i < partnum)
	{
		/* Skip duplicat partition and disk device. For example, skip sda if sda1 found */
		int j, k;
		k = 1;
		PRINTF("Checking %s: ", devlist[i]);
		for (j = i + 1; j < partnum; j++) {
			k = strncmp(devlist[i], devlist[j], strlen(devlist[i]));
			PRINTF("%s=%d ", devlist[j], k);
			if (k == 0)
				break;
		}
		PRINTF("\n");
		if (k) {
			sprintf(tmp, "/dev/%s", devlist[i]);
			if (check_mount(tmp, NULL)) {
				fprintf(stderr,"Device %s has been mounted\n", tmp);
				i++;
				continue;
			}
			
			//��һ��û��mount�豸��Ŀ¼��
			for (j = 1; j < MAX_PART; j++) {
				printf(">>  j = %d\n", j);
				sprintf(buf, "/mnt/usb%d", j);
				if (access(buf, R_OK) == 0)
					if (!check_mount(NULL, buf))
						break;
			}
			if (j < MAX_PART) {
				fprintf(stderr, "Mounting %s to %s ", tmp, buf);
					
				rc = mountusb2dir(tmp, buf);
				if (rc != 0) {
					fprintf(stderr, "failed: %s\n", strerror(errno));
				} else {
					fprintf(stderr, "ok\n");
					usleep(100000);
				}
			}
		}
		i++;
	}
	return 0;
}

//ж��mount��ָ��Ŀ¼��U�̣���������
static int umount_usb(char *mdir)
{
	if(mdir != NULL)
	{
		printf("umount %s", mdir);
		if(umount2(mdir, MNT_FORCE) != 0)
		{
			printf(" fail\n");
			return -1;
		}
		else
			printf(" ok\n");
	}
	else
		return -1;
	return 0;
}

//�������ڽ���hotplug��Ϣ��socket��
int mountusb_socket_init(void)
{
	int hotplug_sock = 0;
    struct sockaddr_nl snl;
//    const int buffersize = 16 * 1024 * 1024;
    int retval;
	setlinebuf(stdout);
    memset(&snl, 0x00, sizeof(struct sockaddr_nl));
    snl.nl_family = 16;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;

    hotplug_sock = socket(16, SOCK_DGRAM, 15);
    if (hotplug_sock == -1)
    {
        PRINTF("error getting socket: %s", strerror(errno));
        return -1;
    }

    retval = bind(hotplug_sock, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl));
    if (retval < 0) {
        PRINTF("bind failed: %s", strerror(errno));
        close(hotplug_sock);
        hotplug_sock = -1;
        return -1;
    }
    pthread_mutex_init (&mount_mutex, NULL);
    return hotplug_sock;

}

//�ر����ڽ���hotplug��Ϣ��socket��
void mountusb_socket_uninit(int sockfd)
{
	close(sockfd);
}

//ȡ���豸��β�������֡�
static int get_digit_suffix(char* devName)
{
	char c;
	int i;

	for(i=0; i<strlen(devName); i++)
	{
		c = *(devName+i);
		if( isdigit(c) )
		{
			int no;
			no = atoi(devName+i);
			//sprintf(dev, "/dev/%s", devName);
			return no;
		}
	}
	return -1;
}

//��hotplug��Ϣ���������豸�š�
static int extract_dev_no(char* info, char* dev)
{
	char *p1 = basename(info);
	if (p1 && dev) {
		char* devName = p1;
		sprintf(dev, "/dev/%s", devName);
		return get_digit_suffix(devName);
	}
	return -1;
}

static int load_procfile(char* filename, char* buf)
{
	FILE* fp = fopen(filename, "rb");
	if (fp) {
		int ret = fread(buf, 1, 512, fp);
		fclose(fp);
		return ret;
	}
	return 0;
}

static int extract_dev_product(char* dir, char* product, char* vendor)
{
	char *p, filename[256];

	if (strncmp(dir, "add@/devices", 11))
		return 1;

	p = (char*)(dir + 4);
	sprintf(filename, "/sys%s/product", p);
	if (!load_procfile(filename, product))
		return 1;

	sprintf(filename, "/sys%s/manufacturer", p);
	if (!load_procfile(filename, vendor))
		return 1;

	return 0;
}

int mountmsb_process_data(int sockfd)
{
	char buf[4096] = {0};
	char devName[20];
	char dirName[20];

	recv(sockfd, &buf, sizeof(buf), 0);

	if (strstr(buf, "add@/block/")) {
		PRINTF("Received: %s\n", buf);
		mount_all_usb();	
	} else 
	if (strstr(buf, "remove@/block/")) {
		extract_dev_no(buf, devName);
		PRINTF("umount %s\n", devName);
		
		if (get_mounted_dir(devName, dirName))
			return -1;
		
		if (umount_usb(dirName)) {
			int i = 10;
			PRINTF("umount %s: %s!\n", dirName, strerror(errno));
			while (i--) {
				sleep(1);
				if (umount_usb(dirName)) {
					PRINTF("umount %s retry%d: %s!\n", dirName, i, strerror(errno));
					PRINTF("================================\n\n");
				} else {
					PRINTF("Umount %s ok\n", dirName);
					break;
				}
			}
		}
	} else
	if (0 == extract_dev_product(buf, devName, dirName)) {
		if (strstr(devName, "802.11") && strstr(devName, "WLAN")) {
			PRINTF("WLAN device found!\n");
			sleep(2);
			system("ifconfig rausb0 up");
		}
	}
	return 0;
}

static int log_pid(void) 
{
	FILE *f;

	if((f = fopen(PID_FILE, "r")) != NULL) {
		fclose(f);
		return 1;
	}
	
	if((f = fopen(PID_FILE, "w")) == NULL) {
		printe("fopen %s mode=w Err.\n", PID_FILE);
		return -1;
	}
	fprintf(f, "%d\n", getpid());
	fclose(f);
	
	return 0;
}

int main(int argc,char *argv[])
{
	int rc, socket_usb;	
	int fd,fdtablesize;
	int fp;   
	
	rc = log_pid();
	switch (rc) {
		case 1: 
			fprintf(stderr, "%s is runing, kill it and remove %s ...\n", argv[0], PID_FILE);
			exit(1);
		break;
		case -1: 
			fprintf(stderr, "Cannot save %s\n", PID_FILE);
			exit(-1);
		break;
	}
	
	if ((argc < 2) || strncmp(argv[1], "-d", 2)) {
		deamon_mode = 1;
	// Run in deamon mode
	//����һЩ�йؿ����ն˲������źš���ֹ���ػ�����û��������ת����ʱ�������ն��ܵ�
	//���� �˳�����𣬴˴��������ն�I/O�źš�STOP�ź�
		 signal(SIGTTOU,SIG_IGN);
		 signal(SIGTTIN,SIG_IGN);
		 signal(SIGTSTP,SIG_IGN);
		 signal(SIGHUP,SIG_IGN);
	//�����ӽ��̻�̳и����̵�ĳЩ���ԣ�������նˡ���¼�Ự��������ȣ����ػ�����
	//����Ҫ��������ն˵���̨ȥ���У����Ա���Ѹ�����ɱ������ȷ���ӽ��̲��ǽ����鳤��
	//��Ҳ�ǳɹ�����setsid()��Ҫ��ġ�
		 if (fork() != 0)
		 {
/*			syslog(LOG_USER|LOG_INFO,"Run  %d\n", __LINE__); */
			exit(1);
		 }
	//�����˿����ն˻�Ҫ�����¼�Ự�ͽ����飬������Ե���setsid()���������óɹ���
	//���̳�Ϊ�µĻỰ�鳤���µĽ����鳤������ԭ���ĵ�¼�Ự�ͽ��������룬���ڻỰ����
	//�Կ����ն˵Ķ�ռ�ԣ�����ͬʱ������ն����롣
	
		 //if 
		 setsid();
		 {
/*			syslog(LOG_USER|LOG_INFO,"Run to Line %d\n", __LINE__); */
//			exit(1);
		 }

		 //Ҫ�ﵽsetsid()�����Ĺ���Ҳ���������´�������"/dev/tty"��һ�����豸��Ҳ���ն�ӳ�䣬����close()�������ն˹رա�
		 if ((fp = open("/dev/tty", O_RDWR)) >= 0)
		 {
			 ioctl(fp, TIOCNOTTY, NULL);
			 close(fp);
		 }

		/*�Ӹ����̼̳й����ĵ�ǰ����Ŀ¼������һ��װ����ļ�ϵͳ�С���Ϊ�ػ�����ͨ����ϵͳ����֮ǰ��һֱ���ڵģ�
		��������ػ����̵ĵ�ǰ����Ŀ¼��һ��װ���ļ�ϵͳ�У���ô���ļ�ϵͳ�Ͳ��ܱ�ж�ء�
		����˵�Ӹ����̼̳еĵ�ǰĿ¼��/mnt�����һ�������ص�Ŀ¼��
		*/
		 if (chdir("/tmp") == -1) {
			syslog(LOG_USER|LOG_INFO, "Run to Line %d\n", __LINE__);
			exit(1);
		 }
		 /*�رմ򿪵��ļ������������ض����׼���롢��׼����ͱ�׼����������ļ���������
		 ���̴Ӵ������ĸ���������̳��˴򿪵��ļ���������������رգ������˷�ϵͳ��Դ��
		 �����޷�Ԥ�ϵĴ���getdtablesize()����ĳ���������ܴ򿪵������ļ�����*/
		for (fd=0, fdtablesize = getdtablesize(); fd < fdtablesize; fd++)
			close(fd);

		 //�еĳ�����Щ��������󣬻���Ҫ�����������¶���
	/*
		 error=open("/tmp/error",O_WRONLY|O_CREAT,0600);
		 dup2(error,2);
		 close(error);
		 in=open("/tmp/in",O_RDONLY|O_CREAT,0600);
		 if(dup2(in,0)==-1) perror("in");
		 close(in);
		 out=open("/tmp/out",O_WRONLY|O_CREAT,0600);
		 if(dup2(out,1)==-1) perror("out");
		 close(out);
	*/
		 /* �ɼ̳е������ļ���ʽ�����������ֿ��ܻ�ܾ�����ĳЩȨ�ޣ�����Ҫ���¸�������Ȩ�ޡ� */
		 umask(0);

		/*��������̲��ȴ��ӽ��̽������ӽ��̽���Ϊ��ʬ���̣�zombie���Ӷ�ռ��ϵͳ��Դ��
		��������̵ȴ��ӽ��̽����������Ӹ����̵ĸ�����Ӱ����������̵Ĳ������ܡ������Ҫ��SIGCHLD�ź���������
		���ս�ʬ���̵���Դ��������ɲ���Ҫ����Դ�˷ѡ� */
		signal(SIGCHLD,SIG_IGN);
	}

	mount_all_usb();
	socket_usb = mountusb_socket_init();

	while (1) {
		mountmsb_process_data(socket_usb);
	}
	mountusb_socket_uninit(socket_usb);
	unlink(PID_FILE);
	return 0;
}
