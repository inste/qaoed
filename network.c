#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#ifdef linux
# include <linux/if_packet.h>
# include <net/if.h>
#endif

#include "include/qaoed.h"
#include "include/logging.h"

void insertwork(struct aoedev *device, struct workentry *work)
{
   struct queueentry *qe;
   
   qe = (struct queueentry *) malloc(sizeof(struct queueentry));
   
   if(qe == NULL)
     {
	perror("malloc failed");
	return;
     }

   qe->work = work;
   qe->next = NULL;
   
   
   /* Find the next free slot */
   if(device->q == NULL)
     {
	device->q = qe;
	device->qstart = qe;
	device->qend = qe;
     }
   else
     if(device->qend != NULL)
       {
	  device->qend->next = qe;
	  device->qend = qe;
       }
}
  
int qaoed_pktdestroy(struct pkt *packet)
{
  /* Start by locking */
  pthread_mutex_lock(&packet->pktlock);

  /* We dec the recounter */
  packet->refcount--;
  
   /* If this was the last reference it gets destroyed */
   if(packet->refcount < 1)
     {
	if(packet->raw != NULL)
	  free(packet->raw);
	
	/* Pointless, but for good measure .. */
	pthread_mutex_unlock(&packet->pktlock);
	pthread_mutex_destroy(&packet->pktlock);
	
	/* Thats it .. we are done */
	free(packet);
	
	return(0);
     }
   else
     {
       pthread_mutex_unlock(&packet->pktlock);
       return(1);
     }
}

void qaoed_workdestroy(struct workentry *work)
{
   if(work == NULL)
     return;
   
   /* Try to destroy the packet */
   if(work->rawbuf != NULL)
     if(qaoed_pktdestroy(work->rawbuf) == 0)
       work->rawbuf = NULL;

   /* Start by locking the workentry */
   pthread_mutex_lock(&work->worklock);

   /* We dec the recounter */
   work->refcount--;
   
   /* If this was the last reference it gets destroyed */
   if(work->refcount < 1)
     {
	
	/* Pointless, but for good measure .. */
	pthread_mutex_unlock(&work->worklock);
	pthread_mutex_destroy(&work->worklock);
	
	/* Thats it .. we are done */
	free(work);
	
	return;
     }
   else
     {
	/* We have decreased the refcounter, not mutch more to
	 * do then to unlock and return */
	pthread_mutex_unlock(&work->worklock);
	return;
     }
   
   return;
}

void processpacket(struct qconfig *conf, struct ifst *ifentry,
		   struct pkt *Packet, struct aoe_hdr *aoepkt, int len)
{
   struct aoedev *device;
   struct workentry *work;
   
   unsigned short shelf;   
   unsigned char slot;     

   /* The packet has to be at least sizeof(struct aoe_hdr) long */
   if((len < sizeof(struct aoe_hdr)))
     {
	logfunc(ifentry->log,LOG_ERR,
		"Short packet recieved on interface %s, packet len == %d\n",
		ifentry->ifname,len);
	
	qaoed_pktdestroy(Packet);
	return;
     }

   /* Make sure its the right kind */
   if((ntohs(aoepkt->eth.h_proto) != ETH_P_AOE))
     {
	logfunc(ifentry->log,LOG_ERR,
		"Wrong ethernet type recieved on interface %s, proto == %X\n",
		ifentry->ifname,aoepkt->eth.h_proto);
	qaoed_pktdestroy(Packet);
	return;
     }
   
   /* Is this a response or a request  ? */
   if ((aoepkt->ver_flags & htons(AOE_FLAG_RSP)) > 0)
     {
	/* If its a response we simply drop it */
#ifdef DEBUG
	logfunc(conf->log,LOG_ERR,"RSP packet\n");
#endif
	qaoed_pktdestroy(Packet);
	return;
     }

   /* Retrieve major and minor */
   shelf = ntohs(aoepkt->shelf); /* ntohs needed since its a short (2 bytes) */
   slot  = aoepkt->slot; 
   
   /* Create a workentry to add to the queue(s) */
   work = (struct workentry *) malloc(sizeof(struct workentry));
   
   if(work == NULL)
     {
	logfunc(conf->log,LOG_ERR,"malloc() failed in processpacket for "
		"interface %s - %s\n",ifentry->ifname,strerror(errno));
	free(Packet);
	return;
     }
   
   /* place the packet in the workorder */
   work->rawbuf = Packet;
   work->aoepkt = aoepkt;
   work->packet_len = len;

   /* workorder type */
   work->type = WORKTYPE_PACKET;
   
   /* The interface where the reply is supposed to go */
   work->ifentry = ifentry;
  
   /* Init the lock */
   pthread_mutex_init(&work->worklock,NULL);
 
   /* We start with a refcount of 1 so that it wont 
    * get destroyed untill we are finished queueing it */
   work->refcount = 1;
   
   /* Place a readlock on the device-list */
   pthread_rwlock_rdlock(&conf->devlistlock);
   
   /* Find the correct device for this packet */
   for(device = conf->devices; device != NULL; device = device->next)
     if(device->shelf == shelf && device->slot == slot)
     {
	/* Up the refcount for this item */
	pthread_mutex_lock(&work->worklock);
	work->refcount++;
	pthread_mutex_unlock(&work->worklock);
	
	/* Up the refcount for the packet */
	pthread_mutex_lock(&work->rawbuf->pktlock);
	work->rawbuf->refcount++;
	pthread_mutex_unlock(&work->rawbuf->pktlock);
	
	/* Insert it in the queue */
	pthread_mutex_lock(&device->queuelock);
	insertwork(device,work);
	
	/* Wake the worker */
	pthread_cond_signal( &device->qcv);
		
	/* Unlock queue */
	pthread_mutex_unlock(&device->queuelock);
     }
   
   /* If this was a broadcast we send it to all queues */
   if((shelf == 0xffff) && (slot == 0x00ff))
     for(device = conf->devices; device != NULL; device = device->next)
       {
	  
	  /* Up the refcount for this item */
	  pthread_mutex_lock(&work->worklock);
	  work->refcount++;
	  pthread_mutex_unlock(&work->worklock);
	  
	  /* Up the refcount for the packet */
	  pthread_mutex_lock(&work->rawbuf->pktlock);
	  work->rawbuf->refcount++;
	  pthread_mutex_unlock(&work->rawbuf->pktlock);
	  
	  /* Insert it in the queue */
	  pthread_mutex_lock(&device->queuelock);
	  insertwork(device,work);
	  
	  /* Wake the worker */
	  pthread_cond_signal( &device->qcv);
	  
	  /* Unlock queue */
	  pthread_mutex_unlock(&device->queuelock);
       } 

   /* release readlock on the device-list */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   /* We are done with the work-struct so we try to destroy it */
   qaoed_workdestroy(work);
      
   return;
}

int qaoed_startlistener(struct qconfig *conf, struct ifst *ifent)
{
   struct threadargs *args;
   pthread_attr_t atr;
   
   args = (struct threadargs *) malloc(sizeof(struct threadargs));
   
   if(args == NULL)
     {
	logfunc(conf->log,LOG_ERR,
		"malloc: %s\n", strerror(errno));
	perror("malloc");
	return(-1);
     }
   
   args->conf = conf;
   args->ifentry = ifent;
   
   /* Initialize the attributes */
   pthread_attr_init(&atr);
   pthread_attr_setdetachstate(&atr, PTHREAD_CREATE_JOINABLE);
   
   /* Start the network listener thread */
   if(pthread_create (&ifent->threadID,
		      &atr,
		      (void *)&qaoed_listener,(void *)args) != 0)
     {
	logfunc(conf->log,LOG_ERR,
		"Failed to start network listener for interface %s\n",
		ifent->ifname);
	return(-1);
     }	   
 
   /* Success :) */
   return(0);
}


int qaoed_network(struct qconfig *conf)
{

   struct ifst *ifent = conf->intlist;

   
   if(conf == NULL)
     return(-1);
   
   while(ifent)
     {
	/* Try to start network listener thread */
	if(qaoed_startlistener(conf,ifent) != 0)
	  return(-1);
	
	ifent = ifent->next;
     }
   
   /* Return succes status */
   return(0);
   
}


int qaoed_xmit(struct workentry *work, void *pkt, int pkt_len )
{
   int ret; 
   struct ethhdr *eth = (struct ethhdr *)pkt;
   

   /* Set the correct src-addr if needed */
   if(work->ifentry->hwaddr != NULL)
     memcpy(eth->h_source,work->ifentry->hwaddr,6);
   
   ret = write(work->ifentry->sock, pkt, pkt_len); 

   /* 
   printf("Sending packet to %X:%X:%X:%X:%X:%X from %X:%X:%X:%X:%X:%X\n",
	  eth->h_dest[0],
	  eth->h_dest[1],
	  eth->h_dest[2],
	  eth->h_dest[3],
	  eth->h_dest[4],
	  eth->h_dest[5],
	  
	  eth->h_source[0],
	  eth->h_source[1],
	  eth->h_source[2],
	  eth->h_source[3],
	  eth->h_source[4],
	  eth->h_source[5]);
*/
   
   if(ret != pkt_len)
     {
	perror("write to sock");

	logfunc(work->ifentry->log,LOG_ERR,
		"Failed to write packet to socket for interface %s: %s\n",
		work->ifentry->ifname,strerror(errno));
     }
   
   free(pkt);
   return (ret);
}

void qaoed_intrefup(struct ifst *interface)
{
   pthread_mutex_lock(&interface->iflock);
   interface->refcnt++;
   pthread_mutex_unlock(&interface->iflock);
}


void qaoed_intrefdown(struct ifst *interface)
{
   pthread_mutex_lock(&interface->iflock);
   interface->refcnt--;
   pthread_mutex_unlock(&interface->iflock);
}



