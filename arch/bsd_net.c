#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

#include "../include/qaoed.h"
#include "../include/logging.h"

/* #define DEBUG 1 */

#ifdef __APPLE__
 #define USEBPF 1
#endif

#ifdef __FreeBSD__
 #define USEBPF 1
#endif

#ifdef USEBPF
#include <net/bpf.h>
#include <net/if.h>
#endif

#ifdef USEBPF
/* This is just a stub-function. Its not needed since the 
 * OS will set the correct ethernet-address on all outgoing packets.
 */
int getifhwaddr(struct ifst *ifentry)
{
 ifentry->hwaddr = NULL;
 return(0);
}

/* FIXME: This function allways returns 1500 .. no jumoframe support here :( */
int getifmtu(struct ifst *ifentry)
{
        ifentry->mtu = 1500;
	return(1500);
}

/* Read the bpf man-page if you want to know how this works */
int qaoed_opensock(struct ifst *ifentry)
{
 int i;
 struct bpf_program filter;
 struct ifreq ifr;
 int ioctlarg;
 struct bpf_insn insns[] = {
     /* Make sure this is an AOE-packet */
     BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
     BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETH_P_AOE, 0, 1),
     /* We want this packet :) */
     BPF_STMT(BPF_RET+BPF_K, (u_int)-1),
     /* else reject packet */
     BPF_STMT(BPF_RET+BPF_K, 0)
 };

 filter.bf_len = sizeof(insns)/sizeof(struct bpf_insn);
 filter.bf_insns = (struct bpf_insn *) insns;
 
/* find a free bpf-device */
  for(i = 0; i < 9; i++)
    { 
       char bpfdev[25];
       sprintf(bpfdev,"/dev/bpf%d",i);
       ifentry->sock  = open(bpfdev,O_RDWR);
       
       if(ifentry->sock > 0)
         break;
    }

 /* did we get a working bpf-device ? */
   if(ifentry->sock == -1)
     {
       logfunc(ifentry->log,LOG_ERR,"Failed to find a free bpf device\n");
       return(-1);
     }

   /* On *BSD we can ask the platform to fill in 
      the src address for us, nice nice :) */
     ioctlarg = 0;
     if(ioctl(ifentry->sock, BIOCSHDRCMPLT, &ioctlarg) == -1)
     {
       printf("Ekk.. que? BIOCSHDRCMPLT: %s\n",strerror(errno));
       return(-1);
     }

   /* We dont want to see outgoing packets */
     ioctlarg = 0;
     
     if(ioctl(ifentry->sock, BIOCSSEESENT, &ioctlarg) == -1)
     {
       logfunc(ifentry->log,LOG_ERR,"BIOCSSEESENT failed for %s : %s\n",
	       ifentry->ifname,strerror(errno));
       return(-1);
       } 
     
   /* We want the packets as they come in  */
     ioctlarg = 1;
     if(ioctl(ifentry->sock, BIOCIMMEDIATE, &ioctlarg) == -1)
     {
       logfunc(ifentry->log,LOG_ERR,"BIOCIMMEDIATE failed for %s : %s\n",
	       ifentry->ifname,strerror(errno));
       return(-1);
     }

   /* Set the size of the recive buffer to 16k */
   ifentry->buffsize = 16384;
   if(ioctl(ifentry->sock, BIOCSBLEN, &ifentry->buffsize) == -1)
     {
       logfunc(ifentry->log,LOG_ERR,"BIOCSBLEN failed for %s : %s\n",
	       ifentry->ifname,strerror(errno));
       return(-1);
     }

   /* zero out the ifreq struct */
   memset(&ifr,0,sizeof(struct ifreq));

   /* Set the interface we want to listen on */
   strcpy(ifr.ifr_name,ifentry->ifname);

   /* Select the interface */
   if(ioctl(ifentry->sock, BIOCSETIF, &ifr) == -1)
     {
       logfunc(ifentry->log,LOG_ERR,"BIOCSETIF failed for %s : %s\n",
	       ifentry->ifname,strerror(errno));
        return(-1);
     }   
 
   /* Attach the filter */
   if (ioctl(ifentry->sock, BIOCSETF, &filter) == -1) 
     {
       logfunc(ifentry->log,LOG_ERR,"BIOCSETF failed for %s : %s\n",
	       ifentry->ifname,strerror(errno));
       return(-1);
       }  
   
   return(ifentry->sock);
}
#endif


#ifdef USEBPF
/* This is the listener worker thread for Berkeley Packet Filter (bpf) */
int qaoed_listener(struct threadargs *args)
{
   struct ifst *ifentry = args->ifentry;
   struct qconfig *conf = args->conf; 
   struct bpf_hdr *bpfHeader;
   struct aoe_hdr *aoe;
   struct pkt *packet;
   char *pkt;

   free(args);
   
   if(ifentry->sock == -1)
     {
	logfunc(ifentry->log,LOG_ERR,
		"qaoed_listener: No socket for interface %s\n",
		ifentry->ifname);
	return(-1);
     }
   
   while(1)
     {
       packet = (void *) malloc(sizeof(struct pkt));

       if(packet == NULL)
	 continue; /* Ekk .. try again */

       /* Alloc size for actual data */
       packet->raw = (void *) malloc(ifentry->buffsize + 10);
       
       if(packet->raw != NULL)
	 {
	   int len;
	   
	   /* Init refcounter + lock */
	   pthread_mutex_init(&packet->pktlock,NULL);

	   packet->refcount = 1; /* We dont want it destroy from underneth 
                                    us now do we ? */

	   len = read(ifentry->sock, packet->raw, ifentry->buffsize);
	    
	   /* We dont want to be interrupted while we process the buffer */
	   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
	   
	   pkt = packet->raw;

	   /* Extracts each packet and pass it to processpacket() */
	   while(( (char *)pkt - (char *)packet->raw ) < len)
	     {
	   #ifdef DEBUG
		printf("Processing packet\n");
		fflush(stdout);
	#endif    
	       /* `Calculate` bpfHeader-start */
	       bpfHeader = (struct bpf_hdr *) pkt;
	       
	       /* The first packet */
	       aoe = (struct aoe_hdr *)
		 ((char *)pkt + (unsigned short)bpfHeader->bh_hdrlen);
	       
	       /* Add to the refcounter */
	       pthread_mutex_lock(&packet->pktlock);
	       packet->refcount++;
	       pthread_mutex_unlock(&packet->pktlock);

	       /* Sent the pkt on to processpacket */
	       processpacket(conf,ifentry,packet,aoe,bpfHeader->bh_caplen);
	       
	       /* Move on to the next packet */
	       pkt = (char *)pkt + 
		 BPF_WORDALIGN(bpfHeader->bh_hdrlen + bpfHeader->bh_caplen);

	     }
	   
	   /* Now we can destroy the packet if it aint in use any more */
	   qaoed_pktdestroy(packet);
	   
	   /* Okey, now we can be interrupted again */
	   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	 }
       else
	 free(packet);
     }
}
#endif
