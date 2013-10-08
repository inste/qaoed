#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#ifdef linux
# include <linux/if_packet.h>
# include <net/if.h>
#endif

#include "../include/qaoed.h"
#include "../include/logging.h"


#ifdef linux
int getifhwaddr(struct ifst *ifentry)
{
   struct ifreq ifr;
   
   strcpy(ifr.ifr_name,ifentry->ifname);
   
   if(ioctl(ifentry->sock,SIOCGIFHWADDR,&ifr) == -1)
     {
	logfunc(ifentry->log,LOG_ERR,
		"Failed to get hardware address of interface");
	return(-1);
     }
   else
     {
	ifentry->hwaddr = malloc(6);

	if(ifentry->hwaddr == NULL)
	  {
	     logfunc(ifentry->log,LOG_ERR,"malloc: %s",strerror(errno));
	     exit(-1);
	  }
	memcpy(ifentry->hwaddr,ifr.ifr_hwaddr.sa_data,6);
     }
   return(0);
}
#endif

#ifdef linux
int getifmtu(struct ifst *ifentry)
{
   struct ifreq ifr;
   
   strcpy(ifr.ifr_name,ifentry->ifname);
   
   if(ioctl(ifentry->sock,SIOCGIFMTU,&ifr) == 0)
     return(ifr.ifr_mtu);
   
   /* else */
   
   logfunc(ifentry->log,LOG_ERR,
	   "Failed to get MTU for interface %s",ifentry->ifname);
   return(-1);
   
}
#endif

  
#ifdef linux
int getifindex(int sock,char *ifname)
{
   struct ifreq ifr;
   
   strcpy(ifr.ifr_name,ifname);
   
   if(ioctl(sock,SIOCGIFINDEX,&ifr) == -1)
     {
	perror("getifindex");
	return(-1);
     }
   else
	return(ifr.ifr_ifindex);
   
}
#endif


#ifdef linux 
int qaoed_opensock(struct ifst *ifentry)
{
  struct sockaddr_ll sa;  
  
  ifentry->sock = (int) socket(PF_PACKET,SOCK_RAW,htons(ETH_P_AOE)); 

  if(ifentry->sock == -1)
     {
	logfunc(ifentry->log,LOG_ERR,
		"Failed to open network socket for interface %s!\n",
		ifentry->ifname);
	return(-1);
     }
   
   /* In order to send packets we must bind the socket 
    * to a specific device */
   
   sa.sll_family = AF_PACKET;
   sa.sll_protocol = htons(ETH_P_AOE);
   sa.sll_ifindex = getifindex(ifentry->sock,ifentry->ifname);
   
   if(bind(ifentry->sock,(struct sockaddr *)&sa, sizeof(sa)) != 0)
     {
	logfunc(ifentry->log,LOG_ERR,"bind failed for interface %s - %s",
		ifentry->ifname,strerror(errno));
	return(-1);
     }

   /* Initial value */
   ifentry->buffsize = 1518;

   return(ifentry->sock);
}
#endif

#ifdef linux
/* This is the listener worker thread for Linux */
int qaoed_listener(struct threadargs *args)
{
   struct ifst *ifentry = args->ifentry;
   struct qconfig *conf = args->conf; 
   struct pkt *packet;

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
	   
	   /* Init refcounter lock */
	   pthread_mutex_init(&packet->pktlock,NULL);

	   /* Linux allways returns one packet per read() .. (i think?) */
	   packet->refcount = 1;

	   len = read(ifentry->sock, packet->raw, ifentry->buffsize);
	    
	    /* We dont want to be interrupted while we process the packet */
	   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
	    	    
	   if(len >= sizeof(struct aoe_hdr))
	     processpacket(conf,ifentry,packet,packet->raw,len);
	   else
	     {
	       logfunc(ifentry->log,LOG_ERR,
		       "short read from socket for interface %s\n",
		       ifentry->ifname);
	     }
	    
	    /* Okey, now we can be interrupted again */
	    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	 }
       else
	 free(packet);
     }
}
#endif
