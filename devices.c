#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>

#ifdef linux
# include <linux/fs.h>
#endif

#include "include/qaoed.h"
#include "include/hdreg.h"
#include "include/byteorder.h"
#include "include/acl.h"
#include "include/logging.h"

/* This function creates an AOE-response packet using the information in the 
 * requests, it copies the tag and moves the source-address from the request 
 * into the destination field of the reply; as well as some other stuff.
 * On success it returns a pointer to the newly created reply-pkt
 */
void *qaoed_createpkt(struct aoedev *dev, struct workentry *work, int len)
{
   struct aoe_hdr *pkt;
   
   if(len < sizeof(struct aoe_hdr))
     len = sizeof(struct aoe_hdr);
   
   pkt = (struct aoe_hdr *) malloc(len);
   
   if(pkt == NULL)
     {
	perror("malloc");
	return(NULL);
     }
   
   /* Copy the entire header from the request into the reply */
   memcpy((void *)pkt, (void *)work->aoepkt,
	  sizeof(struct aoe_hdr));
     
   /* We send the reply to the source of the request */
   memcpy(pkt->eth.h_dest, work->aoepkt->eth.h_source, ETH_ALEN);
   
   /* Set src-addr to 00:00:00:00:00:00 for now  */
   memset(pkt->eth.h_source,00, ETH_ALEN);
   
   pkt->ver_flags |= 0x10;             /* AOE Version one */
   pkt->ver_flags |= AOE_FLAG_RSP;     /* response flag */
        
   /* Set the appropriate major and minor number */
   pkt->shelf = htons(dev->shelf);
   pkt->slot  = dev->slot;

   return(pkt);
}

/* Convert from LBA to block-offset */
off_t qaoed_LBA(struct aoe_atahdr *request)
{
   long long ppos = 0;
   unsigned char *p = request->lba;
   int i ;
   
   for (i = 0; i < 6; i++)
     ppos |= (long long)(*p++) << i * 8;
   
   if ((request->flags & AOE_ATAFLAG_LBA48) != 0)
     ppos = ppos & 0x0000ffffffffffffLL;     /* full 48 */
   else
     ppos = ppos & 0x0fffffff;
   
   return (ppos);
}

/* Read from file */
int qaoed_file_read(int fd, unsigned char *buff, int len, off_t pos)
{
   /* Seek to the right possition */
   if(lseek(fd,pos,SEEK_SET) == -1)
     {
      perror("lseek");
     printf("fseek failed!\n");
     return(-1);
     }

   /* Do the actual transaction */
   return(read(fd,buff,len));
}

/* Write to file */
int qaoed_file_write(int fd, unsigned char *buff, int len, off_t pos)
{
   int n;
   
   /* Seek to the right possition */
   if(lseek(fd,pos,SEEK_SET) == -1)
     {
	perror("fseek()");
	return(-1);
     }

   /* Write the data to disk */
   n=write(fd,buff,len);
   
   if(n < 1)
     perror("fwrite()\n");

   return(n);
}

/* this function moves data to or from the disk */
struct aoe_atahdr *qaoed_transfer(struct aoedev *dev,
				  struct workentry *work,
				  struct aoe_atahdr *request,
				  struct aoe_atahdr *reply,
				  int reply_len)
{
   off_t ppos = 0;
         
   /* Get offset in blocks */
   ppos = qaoed_LBA(request);
   
   /* Convert to byte offet */
   ppos = ppos * 512;
   
   switch (request->cmdstat) 
     {
      case WIN_READ:
      case WIN_READ_EXT:
	
	/* Make space for data in reply packet */
	reply_len = reply_len + request->nsect * 512;
	reply = realloc(reply, reply_len);
	
	if(qaoed_file_read(dev->fd, reply->data,
			  (request->nsect * 512), ppos) <= 0)
	  goto error_xmit;
	break;
	
      case WIN_WRITE:
      case WIN_WRITE_EXT:
	
	if(qaoed_file_write(dev->fd, request->data,
			   (request->nsect * 512), ppos) <= 0)
	  goto error_xmit;
	break;
       
      default:
	/* We didnt understand the command */
	reply->aoehdr.error |= AOE_ERR_BADARG;
	goto error_xmit;
	break;
     }
   
   /* Everything is OK */
   qaoed_xmit(work,(void *)reply,reply_len);
   return(reply);   
  
   /* If we get here, something went wrong :( */
   error_xmit:
   /* ata error fields */
   reply->aoehdr.ver_flags |= AOE_FLAG_ERR;
   reply->cmdstat |= ERR_STAT;
   reply->err_feature |= ABRT_ERR;
   
   qaoed_xmit(work,(void *)reply,reply_len);
   return(reply);  
}
  
/* The client sends an identify-request to figure out the geometry of 
 * the disk. The reply contains a 512-byte driveid binary blob */
struct aoe_atahdr *qaoed_identify(struct aoedev *dev,
				  struct workentry *work,
				  struct aoe_atahdr *request,
				  struct aoe_atahdr *reply,
				  int reply_len)
{
   struct hd_driveid *id;  /* hdreg.h */
   
   /* Make room for the driveid-struct */
   reply_len = sizeof(struct aoe_atahdr) + sizeof(struct hd_driveid);
   reply = realloc(reply, reply_len);
		          
   
   /* Create a pointer to the data-section of the reply */
   id = (struct hd_driveid *) reply->data;
   
   /* zero out everything */
   memset(id, 0, 512);
   
   strcpy((char *) id->model, "123456789");
   strncpy((char *) id->serial_no, dev->devicename, 19);
   
   /* Set up plain old CHS, its obsolete, but the aoe-client for 
    * Linux sometimes uses it, so we fill out the fields anyway.
    * This wont be all that accurate, but i thinks it will work */
   
   /*
    * The ATA spec tells large drives to return
    * C/H/S = 16383/16/63 independent of their size.
    */
   
   id->cur_cyls     = cpu_to_le16(((dev->size >> 8) >> 6));
   id->cur_heads    = cpu_to_le16(255);
   id->cur_sectors  = cpu_to_le16(64);
   
   /* We support LBA */
   id->capability |=  cpu_to_le16(2);   /* Seccond bit == supports LBA */
   if (dev->size > MAXATALBA)
     id->lba_capacity = cpu_to_le32(MAXATALBA);
   else
     id->lba_capacity = cpu_to_le32(dev->size);
   
   /* We support LBA 48 */
   id->command_set_2 |= cpu_to_le16(((1 << 10))); /* We support LBA48 */
   id->cfs_enable_2  |= cpu_to_le16(((1 << 10))); /* We use LBA48 */
   id->lba_capacity_2 = cpu_to_le64(dev->size);
   
   qaoed_xmit(work,(void *) reply,reply_len);
   return(reply);
}

/* This function takes care of incomming ata-requets. It does some basic
 * sanity checking and parses the ata-header to figure out if its a
 * read/write or a device identify-request before it dispatches the
 * request either to qaoed_tranfser for block io or to qaoed_identify
 * for identification requests. */

void qaoed_processATA(struct aoedev *dev,
		      struct workentry *work)
{
   int reply_len;
   struct aoe_atahdr *reply;
   struct aoe_atahdr *request; 
   
   request = (struct aoe_atahdr *) work->aoepkt;
   reply_len = sizeof(struct aoe_atahdr);
   reply   = (struct aoe_atahdr *) qaoed_createpkt(dev, work,reply_len);
	
   
   /* Copy the ata-header from the request to the reply */
   memcpy((char *) reply   + sizeof(struct aoe_hdr),
	  (char *) request + sizeof(struct aoe_hdr),
	  (sizeof(struct aoe_atahdr) - sizeof(struct aoe_hdr)));
   
   /* We start out by filling the reply with the OK flag */
   reply->err_feature = 0;
   reply->cmdstat = READY_STAT;
   
   /* We start out bye checking the access-lists before we do anything */
   switch (request->cmdstat) 
     {
      case WIN_READ:
      case WIN_READ_EXT:
      case WIN_IDENTIFY:
	
	/* Check the read access-list */
	if(dev->racl != NULL)
	  if(aclmatch(dev->racl,
		      request->aoehdr.eth.h_source) != ACL_ACCEPT)
	    goto error_xmit;
	break;
	
      case WIN_WRITE:
      case WIN_WRITE_EXT:
	
	/* Check the write access-list */
	if(dev->wacl != NULL)
	  if(aclmatch(dev->wacl,
		      request->aoehdr.eth.h_source) != ACL_ACCEPT)
	    goto error_xmit;
	break;
     }
   
	
   
   /* What did the sender want us to do ? */
   switch (request->cmdstat) 
     {
	
      case WIN_READ:
      case WIN_READ_EXT:
      case WIN_WRITE:
      case WIN_WRITE_EXT:
	
	reply = qaoed_transfer(dev,work,request,reply,reply_len);
	goto noxmit; /* xmit() was called in qaoed_identify() */
	break;
	
      case WIN_IDENTIFY:
	
	reply = qaoed_identify(dev,work,request,reply,reply_len);
	goto noxmit; /* xmit() was called in qaoed_identify() */
	break;
		
	/* We didnt understand the command :( */
	goto error_xmit;
	break;
	
     }
   
   
   error_xmit:
   reply->err_feature = ABRT_ERR;
   reply->cmdstat = ERR_STAT | READY_STAT;
   
   qaoed_xmit(work,(void *) reply,reply_len);
   
   noxmit: 
   qaoed_workdestroy(work);

   return;
   
}

/* This function takes care of AOE-CFG requests. */
void qaoed_processCFG(struct aoedev *dev, struct workentry *work)
{
   struct aoe_cfghdr *reply;
   struct aoe_cfghdr *request; 

   int reply_len = 0;
   
   request = (struct aoe_cfghdr *) work->aoepkt;

   /* Apply access-list */
   switch (request->aoever_cmd & 0x0f)
     {
	
      case AOE_CFG_READ:   
      case AOE_CFG_MATCH:   
      case AOE_CFG_MATCH_PARTIAL:

	/* Check the cfg read access-list */
	if(dev->cfgracl != NULL)
	  if(aclmatch(dev->cfgracl,
		      request->aoehdr.eth.h_source) != ACL_ACCEPT)
	    goto no_xmit;
		
	break;
	
      case AOE_CFG_SET_IF_EMPTY:   
      case AOE_CFG_SET:
	
	/* Check the cfg write access-list */
	if(dev->cfgsetacl != NULL)
	  if(aclmatch(dev->cfgsetacl,
		      request->aoehdr.eth.h_source) != ACL_ACCEPT)
	    goto no_xmit;
	
	break;
     }
  
   reply_len = sizeof(struct aoe_cfghdr);
   reply   = (struct aoe_cfghdr *) qaoed_createpkt(dev, work,reply_len);
  
   if(reply == NULL)
     goto no_xmit;
     
   /* The number of packets we can queue */
   reply->queuelen = htons(20);
   
   reply->firmware = htons(0x400a);
   reply->maxsect = (work->ifentry->mtu - sizeof (struct aoe_atahdr)) / 512;
   
   /* Copy cmd in reply */
   reply->aoever_cmd = 0;
   reply->aoever_cmd = request->aoever_cmd & 0x0f;
   reply->aoever_cmd |= 0x10;      /* AOE Version one */
   
   reply->cfg_len = 0;
   
   switch (request->aoever_cmd & 0x0f)
     {
	
      case AOE_CFG_READ:   /* read config data from device */
	if(dev->cfg_len > 0)
	  {
	     reply_len = sizeof(struct aoe_cfghdr) + dev->cfg_len;
	     reply = realloc(reply,reply_len);
	  }
	
	memcpy(reply->cfg, dev->cfg, dev->cfg_len);
	reply->cfg_len = htons(dev->cfg_len);
	break;   /* ---------- */
	
	
      case AOE_CFG_MATCH:   /* respond to exact match */
	if ((request->cfg_len == dev->cfg_len) && 
	    (memcmp(dev->cfg, request->cfg, dev->cfg_len)  == 0))
	  {
	     reply_len = sizeof(struct aoe_cfghdr) + dev->cfg_len;
	     reply = realloc(reply,reply_len);
	     
	     memcpy(reply->cfg, dev->cfg, dev->cfg_len);
	     reply->cfg_len = htons(dev->cfg_len);
	  }
	else  
	  goto no_xmit;
	break;     /* ---------- */
	
      case AOE_CFG_MATCH_PARTIAL:    /* respond on partial match */
	if( memcmp(dev->cfg, request->cfg, ntohs(request->cfg_len)) == 0) 
	  {
	     reply_len = sizeof(struct aoe_cfghdr) + dev->cfg_len;
	     reply = realloc(reply,reply_len);
	     memcpy(reply->cfg, dev->cfg, dev->cfg_len);
	     reply->cfg_len = htons(dev->cfg_len);
	  }
	else
	  goto no_xmit;
	break;     /* ---------- */
	
	
      case AOE_CFG_SET_IF_EMPTY:    /* Set config string if empty */
	
	if (dev->cfg_len == 0 &&
	    (ntohs(request->cfg_len) <= 1024)) 
	  {
	     memcpy(dev->cfg,request->cfg,ntohs(request->cfg_len));
	     dev->cfg_len = ntohs(request->cfg_len);
	  }
	else 
	  {
	     reply->aoehdr.ver_flags |= AOE_FLAG_ERR;
	     reply->aoehdr.error |= AOE_ERR_CFG_SET;
	  }
	break;    /* ---------- */
		
	
      case AOE_CFG_SET:    /* Set config string */
	if (ntohs(request->cfg_len) <= 1024)
	  {
	     memcpy(dev->cfg,request->cfg,ntohs(request->cfg_len));
	     dev->cfg_len = ntohs(request->cfg_len);
	  }
	else 
	  {
	     reply->aoehdr.ver_flags |= AOE_FLAG_ERR;
	     reply->aoehdr.error |= AOE_ERR_BADARG;
	  }
	break;
	
      default:
	reply->aoehdr.ver_flags |= AOE_FLAG_ERR;
	reply->aoehdr.error |= AOE_ERR_BADCMD;
	logfunc(dev->log,LOG_ERR,
		"qaoed_processCFG(): Unknown command: %d\n",
		(request->aoever_cmd & 0x0f));
	break;
     }

   qaoed_xmit(work,(void *) reply,reply_len);
   
   no_xmit:
   qaoed_workdestroy(work);

   return;
}


/* This is the entry-point for the worker, this function takes
 * care of newly recieved aoe-packets and dispatches them either to the
 * ata-handler or to the config-handler. As of now there are the only 
 * two commands specified in the ATA over Ethernet specification (ATA & CFG) */

void qaoed_processwork(struct aoedev *dev, struct workentry *work)
{
   
   if(work == NULL)
     return;
   
   if(work->aoepkt == NULL)
     {
	qaoed_workdestroy(work);
	return;
     }
  
   switch (work->aoepkt->cmd) 
     {
      case AOE_CMD_ATA:
	if(work->packet_len >= sizeof(struct aoe_atahdr))
	  qaoed_processATA(dev,work);
	else
	  {
	     logfunc(dev->log,LOG_ERR,
		     "Recieved short ATA packet from %X:%X:%X:%X:%X:%X on %s\n",
		     work->aoepkt->eth.h_source[0],
		     work->aoepkt->eth.h_source[1],
		     work->aoepkt->eth.h_source[2],
		     work->aoepkt->eth.h_source[3],
		     work->aoepkt->eth.h_source[4],
		     work->aoepkt->eth.h_source[5],
		     work->ifentry->ifname);
		     
	     qaoed_workdestroy(work);
	  }
	break;
	
      case AOE_CMD_CFG:
	if(work->packet_len >= sizeof(struct aoe_cfghdr))
	qaoed_processCFG(dev,work);
	else
	  {
	     logfunc(dev->log,LOG_ERR,
		     "Recieved short CFG packet from %X:%X:%X:%X:%X:%X on %s\n",
		     work->aoepkt->eth.h_source[0],
		     work->aoepkt->eth.h_source[1],
		     work->aoepkt->eth.h_source[2],
		     work->aoepkt->eth.h_source[3],
		     work->aoepkt->eth.h_source[4],
		     work->aoepkt->eth.h_source[5],
		     work->ifentry->ifname);
	     qaoed_workdestroy(work);
	  }
	break;
	
      default:
	  {
	     printf("processwork #error\n");
	     
	     logfunc(dev->log,LOG_ERR,
		     "Recieved unknown command (%d) from %X:%X:%X:%X:%X:%X on %s\n",
		     work->aoepkt->cmd,
		     work->aoepkt->eth.h_source[0],
		     work->aoepkt->eth.h_source[1],
		     work->aoepkt->eth.h_source[2],
		     work->aoepkt->eth.h_source[3],
		     work->aoepkt->eth.h_source[4],
		     work->aoepkt->eth.h_source[5],
		     work->ifentry->ifname);

	     qaoed_workdestroy(work);
	  }
	break;
     }
}

/* This function creats a fake cfg-read from h_source */
void qaoed_dobcast(struct aoedev *device, unsigned char *h_source)
{
   struct aoe_cfghdr *pkt;
   struct workentry *work;
   
   work = (struct workentry *) malloc(sizeof(struct workentry));
   if(work == NULL)
     return;
   
   work->rawbuf = malloc(sizeof(struct pkt));
   if(work->rawbuf == NULL)
     return;
   
   work->rawbuf->raw = malloc(sizeof(struct aoe_cfghdr));
   if(work->rawbuf->raw == NULL)
     return;
   
   pthread_mutex_init(&work->rawbuf->pktlock,NULL);
   work->rawbuf->refcount = 1;
   work->aoepkt = work->rawbuf->raw;
   pkt = work->rawbuf->raw;
   memset((void *)pkt,0,sizeof(struct aoe_cfghdr));
   
   /* Set source address h_source */
   memcpy(pkt->aoehdr.eth.h_source,h_source,ETH_ALEN);
   pkt->aoehdr.eth.h_proto = htons(ETH_P_AOE);
   pkt->aoehdr.ver_flags |= 0x10;             /* AOE Version one */
   pkt->aoehdr.ver_flags |= AOE_FLAG_RSP;      /* response flag */
   pkt->aoehdr.cmd = AOE_CMD_CFG;
   pkt->aoever_cmd = AOE_CFG_READ;
   pkt->aoever_cmd |= 0x10;      /* AOE Version one */
   work->type = WORKTYPE_PACKET;
   work->ifentry = device->interface;
   work->refcount = 1;
   work->packet_len = sizeof(struct aoe_cfghdr);
  
   /* Init the workentry mutex */
   if(pthread_mutex_init(&work->worklock,NULL) != 0)
     return;
  
   /* Init the packet mutex */ 
   if(pthread_mutex_init(&work->rawbuf->pktlock,NULL) != 0)
    return;
   
   /* This will force the device to broadcast its precense */
   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
     qaoed_processwork(device,work); 
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
      
}

/* This function will send device advertisement of a device to broadcast
 * if there is no readcfgacl attached, or send to those with an allow-statement
 * in the access-list if an access-list is attached */
void qaoed_bcastdevice(struct aoedev *device)
{
   struct aclhdr *acl = device->cfgracl;
   struct aclentry *aclrow;
   unsigned char h_bcast[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
   
   if(acl == NULL)
     qaoed_dobcast(device,h_bcast);
   else
     for(aclrow = acl->acl; aclrow; aclrow = aclrow->next)
       if(aclrow->rule == ACL_ACCEPT)
	 qaoed_dobcast(device,aclrow->h_dest);
}

/* This is the worker thread for each device */
void qaoed_worker(struct aoedev *device)
{
   struct workentry *work;
   struct queueentry *q;
   device->status = PTSTATUS_RUNNING;
   
   logfunc(device->log,LOG_DEBUG,"Thread for device %s starting\n",
	   device->devicename);
 
   /* Advertise this device */
   if(device->broadcast == 1)
     qaoed_bcastdevice(device); 
   
   while(1)
     {
	/* Aquire lock on our queue */
	pthread_mutex_lock(&device->queuelock);

	/* If queue is empty: Unlock queue and wait for condition */
	if(device->q == NULL)
	  pthread_cond_wait(&device->qcv,&device->queuelock);
	
	/* We dont want to be interrupted while we modify the queue 
	 * and process the work request */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
		
	/* Unlink the workentry from the queue */
	if(device->q != NULL) 
	  {  
	     work = device->q->work;
	     
	     if(device->q == device->qend)
	       device->qend = NULL;
	     
	     if(device->q == device->qstart)
	       device->qstart = device->q->next;

	     q = device->q;
	     device->q = device->q->next;

	     /* Deallocate queueentry */
	     free(q);
	  }
	
	/* Unlock queue */
	pthread_mutex_unlock(&device->queuelock);
	
	/* Handle work */
	switch(work->type)
	  {
	   case WORKTYPE_PACKET:  /* We recieved a packet */
	     qaoed_processwork(device,work);
	     break;
	     
	   default: /* Ehh.. WTF mate ? */
	     logfunc(device->log,LOG_ERR,"Unknown worktype recieved: %d\n",
		     work->type);
	     break;
	  }
	
	/* Allright, now we can be cancel if needed */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
     }
}

/* This function will initialize the queue and start a new worker thread */
int qaoed_startworker(struct aoedev *device)
{
   pthread_attr_t atr;
   
   if(device == NULL)
     return(-1);
   
   /* Initialize the queuelock */
   if(pthread_mutex_init(&device->queuelock,NULL) != 0)
     return(-1);
   
   /* Initialize the Queue Condition Variable (qcv) */
   if(pthread_cond_init (&device->qcv, NULL) != 0)
     {
	pthread_mutex_destroy(&device->queuelock);
	return(-1);
     }
   
   /* Initial values for the queue */
   device->q = NULL;
   device->qstart = NULL;
   device->qend = NULL;
   
   /* Initialize the attributes */
   pthread_attr_init(&atr);
   pthread_attr_setdetachstate(&atr, PTHREAD_CREATE_JOINABLE);
   
   /* Upp the refcounter for the interface */
   qaoed_intrefup(device->interface);
   
   /* Start the worker thread */
   if(pthread_create (&device->threadID,&atr,
		      (void *)&qaoed_worker,
		      (void *)device) != 0)
     {
	pthread_mutex_destroy(&device->queuelock);
	qaoed_intrefdown(device->interface);
	return(-1);
     }
   
   /* Success */
   return(0);
   
}


/* function called to start up a new device */
int qaoed_startdevice(struct aoedev *device)
{
   struct stat fstat;
   int fd;
   
   if(device == NULL)
     return(-1);
   
   if(device->devicename == NULL)
     return(-1);

   /* Try to open file if needed */
   if(device->fp == NULL)
     {
	if(device->writecache == 0)
	  fd = open(device->devicename,O_RDWR | O_SYNC);
	else
	  fd = open(device->devicename,O_RDWR);

	if(fd == -1)
	 { 
             perror("open");
	     logfunc(device->log,LOG_ERR,
		"Failed to open file: %s - %s\n",
		device->devicename,
		strerror(errno));
	      return(-1);
         }

	
	device->fp = fdopen(fd,"rw+");
        device->fd = fd;
     }
   
   if(device->fp == NULL)
     {
	logfunc(device->log,LOG_ERR,
		"Failed to open file: %s - %s\n",
		device->devicename,
		strerror(errno));
	return(-1);
     }
   
   if(stat(device->devicename,&fstat) == 0)
     {
	if(S_ISBLK(fstat.st_mode) ||
           S_ISCHR(fstat.st_mode))	
	  {	
	     off_t mediasize;
	     int error;
	     
	     error = arch_getsize(fd,&mediasize);
	    
	     if (error)
	       printf("ioctl(BLKGETSIZE64) failed, probably not a disk.");

	     device->size = mediasize;
	     
	  }	  
	else
	  {	  
	     /* Get the number of 512-byte blocks */
	     device->size = (fstat.st_size >> 9); /* >>9 == /512 */
	  }	  
     }
   else
     {
	logfunc(device->log,LOG_ERR,
		"Failed to stat file: %s\n",device->devicename);
	close(device->fd);
	return(-1);
     }
   
   if(qaoed_startworker(device) < 0)
     {
	close(device->fd);
	return(-1);
     }
   
   /* All is right */
   return(0);
   
}


/* Loop through the devices in the configuration and start
 * up a thread for each one. Return the number of successfully
 * started threads */
int qaoed_devices(struct qconfig *conf)
{
   struct aoedev *device ;
   
   if(conf == NULL)
     return(-1);
   
   if(conf->devices == NULL)
     return(-1);
      
   for(device = conf->devices; device != NULL; device = device->next)
     if(qaoed_startdevice(device) == -1)
       return(-1);
   
   /* Return success status */
   return(0);
      
}
