#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#ifdef linux
# include <linux/fs.h>

int arch_getsize(int fd, void *mediasize)
{
  int error; 
	error = ioctl(fd, BLKGETSIZE, mediasize);

	return(error);
}

#endif

