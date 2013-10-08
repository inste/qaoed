#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>


#ifdef __FreeBSD__ 
#include <sys/disk.h>
int arch_getsize(int fd, void *mediasize)
{
   int error;

	error = ioctl(fd, DIOCGMEDIASIZE, mediasize);
	return(error);
 
        /* Break into 512byte blocks */
        *mediasize = (*mediasize >> 9); /* >>9 == /512 */

}
#endif

#ifdef __APPLE__
#include <sys/disk.h>
int arch_getsize(int fd, off_t *mediasize)
{
  u_int32_t blksize;
  long long int blkcnt;
  if(!ioctl(fd, DKIOCGETBLOCKSIZE, &blksize) &&
     !ioctl(fd, DKIOCGETBLOCKCOUNT, &blkcnt)) 
    {
      *mediasize = (blksize * blkcnt);

      /* Break into 512byte blocks */
      *mediasize = (*mediasize >> 9); /* >>9 == /512 */
      return(0);
    }
  else
    {
      *mediasize = 0;
      return(-1);
    }
}
#endif

