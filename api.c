#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

/* #define DEBUG 1 */

#include "include/qaoed.h"
#include "include/logging.h"
#include "include/api.h"
#include "include/acl.h"

/* Return some statistics about qaoed */
int processSTATUSrequest(struct qconfig *conf, int conn,
			 struct apihdr *api_hdr,void *arg)
{
   struct qaoed_status status;
   struct aoedev *device;
   struct ifst *ifent;
   
   status.num_targets    = 0;
   status.num_interfaces = 0;
   status.num_threads    = 0;
   
   /* Place a readlock on the device-list before counting */
   pthread_rwlock_rdlock(&conf->devlistlock);
   /* Count the number of device threads */
   for(device = conf->devices; device != NULL; device = device->next)
     status.num_targets++;
   /* unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
      
   /* Place a readlock on the int-list before counting */
   pthread_rwlock_rdlock(&conf->devlistlock);
   /* Count the number of interfaces */
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     status.num_interfaces++;
   /* unlock */
   pthread_rwlock_unlock(&conf->intlistlock);
   
   /* Sum up the information */
   status.num_threads = status.num_interfaces + status.num_targets + 2;
   
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = sizeof(struct qaoed_status);
   
   send(conn,api_hdr,sizeof(struct apihdr),MSG_NOSIGNAL);
   send(conn,&status,sizeof(struct qaoed_status),MSG_NOSIGNAL);
   
   return(0); 
}

/* Return information about targets */
int processTARGET_LIST(struct qconfig *conf, int conn,
		       struct apihdr *api_hdr)
{
   struct aoedev *device;
   struct qaoed_target_info *tglist;
   struct qaoed_target_info *tg;
   int repsize = 0;
   int cnt = 0; 
   
   /* Place a readlock on the device-list before counting */
   pthread_rwlock_rdlock(&conf->devlistlock);
   /* Count the number of device threads */
   for(device = conf->devices; device != NULL; device = device->next)
     cnt++;
   
   repsize = cnt * sizeof(struct qaoed_target_info);
   
   tg = tglist = (struct qaoed_target_info *) malloc(repsize);
   
   if(tglist == NULL)
     {
	/* unlock */
	pthread_rwlock_unlock(&conf->devlistlock);
	return(-1);
     }
   
   for(device = conf->devices; device != NULL; device = device->next)
     {
	/* Make sure we dont send more data then we have room for */
	if(cnt-- <= 0)
	  break;
	
	tg->shelf = device->shelf;
	tg->slot = device->slot;
	tg->broadcast = device->broadcast;
	tg->writecache = device->writecache;
	strcpy(tg->devicename, device->devicename);
	strcpy(tg->ifname, device->interface->ifname);
	
	/* Move to the next */
	tg++;
     }
   
   
   /* unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = repsize;
   
   send(conn,api_hdr,sizeof(struct apihdr),MSG_NOSIGNAL);
   send(conn,tglist, repsize,              MSG_NOSIGNAL);
   
   return(0); 
}

/* Return detailed info about a specific target */
int processTARGET_DETAILS(struct qconfig *conf, int conn,
			  struct apihdr *api_hdr, 
			  struct qaoed_target_info *target)
{
   struct aoedev *device;
   struct qaoed_target_info_detailed *tg;
   
   /* Place a readlock on the device-list before counting */
   pthread_rwlock_rdlock(&conf->devlistlock);

   /* Allocate memory to hold target information */
   tg = (struct qaoed_target_info_detailed *) 
     malloc(sizeof(struct qaoed_target_info_detailed));
   
   if(tg == NULL)
     {
       /* unlock */
       pthread_rwlock_unlock(&conf->devlistlock);
       return(-1);
     }
   

   /* Search for matching device */
   for(device = conf->devices; device != NULL; device = device->next)
     if((device->slot == target->slot  &&
	 device->shelf == target->shelf))
       if(target->ifname[0] == 0 ||
	  device->interface == referenceint(target->ifname,conf))
	 {
	   /* Zero struct */
	   memset(tg,0,sizeof(struct qaoed_target_info_detailed));

	   tg->info.shelf = device->shelf;
	   tg->info.slot = device->slot;
	   tg->info.broadcast = device->broadcast;
	   tg->info.writecache = device->writecache;
	   strcpy(tg->info.devicename, device->devicename);
	   strcpy(tg->info.ifname, device->interface->ifname);

	   
	   /* Access list used for write operations    */
	   if(device->wacl != NULL)
	     strncpy(tg->wacl,device->wacl->name,
		     sizeof(tg->wacl) - 1);

	   /* Access list used for read operations     */
	   if(device->racl != NULL)
	     strncpy(tg->racl,device->racl->name,
		     sizeof(tg->racl) - 1);

	   /* Access list used for cfg set             */
	   if(device->cfgsetacl != NULL)
	     strncpy(tg->cfgsetacl,device->cfgsetacl->name,
		     sizeof(tg->cfgsetacl) - 1);
	   
	   /* Access list used for cfg read / discover */
	   if(device->cfgracl != NULL)
	     strncpy(tg->cfgracl,device->cfgracl->name,
		     sizeof(tg->cfgracl) - 1);

	   /* Accounting data */
	   /* ... */

	   /* Logging target */
	   if(device->log != NULL)
	     strncpy(tg->log,device->log->name,
		     sizeof(tg->log) - 1);

	   /* Loglevel for this device */
	   tg->log_level = device->log_level;

	   /* The cfg-data lenght */
	   tg->cfg_len = device->cfg_len;

	   /* The cfg-data for this device */
	   memcpy(tg->cfg,device->cfg,sizeof(tg->cfg));

	 }
   
   /* unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = sizeof(struct qaoed_target_info_detailed);
   
   send(conn,api_hdr, sizeof(struct apihdr),                     MSG_NOSIGNAL);
   send(conn,tg,      sizeof(struct qaoed_target_info_detailed), MSG_NOSIGNAL);
   
   return(0); 
}


/* Return a list of access-lists */
int processACL_LIST(struct qconfig *conf, int conn,
		    struct apihdr *api_hdr)
{
   struct aclhdr *acllist;
   struct qaoed_acl_info *aclinfo;
   struct qaoed_acl_info *anfo;
   int repsize = 0;
   int cnt = 0; 
   
   /* Count the number access-lists */
   for(acllist = conf->acllist; acllist != NULL; acllist = acllist->next)
     cnt++;
   
   /* Calc size and allocate memory */
   repsize = cnt * sizeof(struct qaoed_acl_info);
   anfo = aclinfo = (struct qaoed_acl_info *) malloc(repsize);
   
   if(aclinfo == NULL)
	return(-1);
   
   /* Extract info */
   for(acllist = conf->acllist; acllist != NULL; acllist = acllist->next)
     {
	/* Make sure we dont send more data then we have room for */
	if(cnt-- <= 0)
	  break;
	
	anfo->aclnumber = acllist->aclnum;
	anfo->defaultpolicy = acllist->defaultpolicy;
	strcpy(anfo->name, acllist->name);
	
	/* Move to the next */
	anfo++;
     }
   
   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = repsize;
   
   /* Send it */
   send(conn,api_hdr, sizeof(struct apihdr),  MSG_NOSIGNAL);
   if(repsize > 0) /* Send list if any */
     send(conn,aclinfo, repsize,              MSG_NOSIGNAL);
   
   return(0); 
}

/* Return information on a specific access-list */
int processACL_STATUS(struct qconfig *conf, int conn,
		      struct apihdr *api_hdr,
		      struct qaoed_acl_info *reqacl)
{
  
  struct aclhdr *acl;
  struct aclentry *entry;
  struct qaoed_acl_entry *aclinfo;
  struct qaoed_acl_entry *anfo;
  int repsize = 0;
  int cnt = 0; 

  /* Encode reply */
  api_hdr->type = REPLY;
  api_hdr->error = API_ALLOK;
  api_hdr->arg_len = 0;  
  
  /* Find the correct access-list */
  for(acl = conf->acllist; acl != NULL; acl = acl->next)
    if(strcmp(acl->name,reqacl->name))
      break;
  
   if(acl == NULL)
     {
       /* Error */
       api_hdr->error = API_FAILURE;
       send(conn,api_hdr, sizeof(struct apihdr),MSG_NOSIGNAL);
       return(0);
     }

   /* Count the number of rows  */
   for(entry = acl->acl; entry != NULL; entry = entry->next)
     cnt++;

   /* Calc size and allocate memory */
   repsize = cnt * sizeof(struct qaoed_acl_entry);
   anfo = aclinfo = (struct qaoed_acl_entry *) malloc(repsize);
   
   if(aclinfo == NULL)
	return(-1);
   
   /* Extract info */
   for(entry = acl->acl; entry != NULL; entry = entry->next)
     {
	/* Make sure we dont send more data then we have room for */
	if(cnt-- <= 0)
	  break;
	
	anfo->rule = entry->rule;
	memcpy(anfo->h_dest, entry->h_dest,6);
	
	/* Move to the next */
	anfo++;
     }
   
   /* Update argument size */
   api_hdr->arg_len = repsize;
   
   /* Send it */
   send(conn,api_hdr, sizeof(struct apihdr),MSG_NOSIGNAL);
   send(conn,aclinfo, repsize,              MSG_NOSIGNAL);
   
   return(0); 
}


/* Return a list of interfaces */
int processINT_LIST(struct qconfig *conf, int conn,
		    struct apihdr *api_hdr)
{
   struct ifst *iflist;
   struct qaoed_if_info *ifinfo;
   struct qaoed_if_info *ifinfolist;
   int repsize = 0;
   int cnt = 0; 
   
   /* Place a readlock on the device-list before counting */
   pthread_rwlock_rdlock(&conf->intlistlock);

   /* Count the number of interfaces */
   for(iflist = conf->intlist; iflist != NULL; iflist = iflist->next)
     cnt++;

   /* Calc size and allocate memory */
   repsize = cnt * sizeof(struct qaoed_if_info);
   ifinfo = ifinfolist = (struct qaoed_if_info *) malloc(repsize);
   
   if(ifinfo == NULL)
     {
       /* FIXME: add logging of this error */
       pthread_rwlock_unlock(&conf->intlistlock);
       return(-1);
     }   

   /* Extract info */
   for(iflist = conf->intlist; iflist != NULL; iflist = iflist->next)
     {
	/* Make sure we dont send more data then we have room for */
	if(cnt-- <= 0)
	  break;
	
	ifinfo->mtu = iflist->mtu;
	
	if(iflist->ifname)
	  strcpy(ifinfo->name, iflist->ifname);
	else
	  ifinfo->name[0] = 0; /* zt */
	
		
	if(iflist->hwaddr)
	  memcpy(ifinfo->hwaddr, iflist->hwaddr,6);
	else
	  memset(ifinfo->hwaddr,0,6);
	
	/* Move to the next */
	ifinfo++;
     }
   
   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = repsize;
   
   /* Send it */
   send(conn,api_hdr, sizeof(struct apihdr),MSG_NOSIGNAL);
   send(conn,ifinfolist, repsize,           MSG_NOSIGNAL);
   
   /* Unlock interface list */
   pthread_rwlock_unlock(&conf->intlistlock);

   return(0); 
}

int processINTERFACE_ADD(struct qconfig *conf, int conn,
			 struct apihdr *api_hdr,
			 struct qaoed_interface_cmd *ifcmd)
{
   struct ifst *newif;
   struct ifst *ifent;
   int cnt = 0;
   
   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_FAILURE; /* Default is failure */
   api_hdr->arg_len = 0;
   
   /* Place a readlock on the device-list before accessing */
   pthread_rwlock_rdlock(&conf->devlistlock);
   
   /* Make sure that the interface doesnt already exist */
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     if(strcmp(ifent->ifname,ifcmd->ifname))
       cnt++;
  
   /* Unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   /* If interface already exists we cant add it again ... */
   if(cnt > 0)
     {
	/* Command failed */
	send(conn,api_hdr, sizeof(struct apihdr),              MSG_NOSIGNAL);
	return(-1);
     }
   
   
   /* Allocate memory */
   newif  = (struct ifst *) malloc(sizeof(struct ifst));
   
   if(newif == NULL)
     {
	/* Memory allocation failed */
	send(conn,api_hdr, sizeof(struct apihdr),              MSG_NOSIGNAL);
	return(-1);
     }
      
   /* Default values */
   newif->prev = NULL;
   newif->next = NULL;
   
   /* Set the default logging level */
   newif->log_level = conf->log_level;
   
   /* Set the default MTU to 1500 */
   /* FIXME - We should use MTU-value from ifcmd */
   newif->mtu = 1500;
   
   /* Set the default logging target */
   newif->log = referencelog("default",conf);
   
   /* Add the interface name */
   newif->ifname = strdup(ifcmd->ifname);
   
   if(qaoed_opensock(newif) != 0)
     {
	/* close it, just in case */
	if(newif->sock != -1)
	  close(newif->sock);

	/* Failed to open interface */
	send(conn,api_hdr, sizeof(struct apihdr),              MSG_NOSIGNAL);
	return(-1);
     }
      
   if(qaoed_startlistener(conf,newif) != 0)
     {
	if(newif->sock != -1)
	  close(newif->sock);

	/* Failed to start network thread  */
	send(conn,api_hdr, sizeof(struct apihdr),              MSG_NOSIGNAL);
	return(-1);
     }

   
   /* Network thread successfully started */
   
   /* Find end of interface list and add our new interface */
   if(conf->intlist != NULL)
     {
	for(ifent=conf->intlist; ifent->next!=NULL; ifent=ifent->next);;
	
	/* Add our device */
	ifent->next  = newif;
	newif->prev = ifent;
     }
   else
     conf->intlist = newif;
     
   /* Update reply header */
   api_hdr->error = API_ALLOK; /* Success */
   api_hdr->arg_len = sizeof(struct qaoed_interface_cmd);   
   
   /* Transfer interface info to ifcmd-reply */
   strcpy(ifcmd->ifname,newif->ifname);
   ifcmd->mtu = newif->mtu;
   
   /* Command completed successfully */
   send(conn,api_hdr, sizeof(struct apihdr),              MSG_NOSIGNAL);
   
   /* Success! :) */
   return(0);
}

int processINTERFACE_DEL(struct qconfig *conf, int conn,
			 struct apihdr *api_hdr,
			 struct qaoed_interface_cmd *ifcmd)
{
   struct ifst *ifent;

   if(conf->intlist == NULL)
     return(-1);
   
   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_FAILURE; /* Default is failure */
   api_hdr->arg_len = sizeof(struct qaoed_interface_cmd);   
   
   /* Place a writelock on the device-list before modifying */
   pthread_rwlock_wrlock(&conf->devlistlock);
   
   /* Make sure that the interface we are going to delete exists 
    * and that it isnt referenced */
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     if(strcmp(ifent->ifname,ifcmd->ifname))
       {
	  pthread_mutex_lock(&ifent->iflock);
	  
	  if(ifent->refcnt < 1)
	    {
	      /* Try to shutdown interface && 
		* Wait for thread to finish */
	       if(pthread_cancel(ifent->threadID) == 0 &&
		  pthread_join(ifent->threadID,NULL) == 0)
		 {
		   /* Unlink from interface list */
		   if(ifent->prev != NULL)
		     ifent->prev->next = ifent->next;
		   else
		     conf->intlist = ifent->next; /* First device */
		   
		   if(ifent->next != NULL)
		     ifent->next->prev = ifent->prev;
		   
		   /* Interface successfully removed! */
		   api_hdr->error = API_ALLOK; /* Success */
		 }
	       else
		 api_hdr->error = API_FAILURE; /* Failure */
	    }
	  pthread_mutex_unlock(&ifent->iflock);
       }

   /* Unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   /* Send reply */
   send(conn,api_hdr, sizeof(struct apihdr),              MSG_NOSIGNAL);
   send(conn,ifcmd,   sizeof(struct qaoed_interface_cmd), MSG_NOSIGNAL);   
   
   return(0);
}

int processINTERFACE_SETMTU(struct qconfig *conf, int conn,
			    struct apihdr *api_hdr,
			    struct qaoed_interface_cmd *ifcmd)
{
   struct ifst *interface;

   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = sizeof(struct qaoed_interface_cmd);   
   
   /* Try to lookup interface */
   interface = referenceint(ifcmd->ifname,conf);
   
   if(interface == NULL)
     api_hdr->error = API_FAILURE;
   else
     {
	/* Set MTU */

	/* mtu == -1 is a request to autoconfigure / reset to default */
	if(ifcmd->mtu == -1)
	  {
	     ifcmd->mtu = getifmtu(interface);
#ifdef DEBUG
	     printf("Autoconfiguring interface mtu to %d\n",ifcmd->mtu);
#endif 
	  }
	     
	/* Make sure mtu value isnt higher then the MTU-value of the int */
	if(ifcmd->mtu > getifmtu(interface))
	  api_hdr->error = API_FAILURE;
	else
	  interface->mtu = ifcmd->mtu; /* actually set the MTU value */
	
	/* FIXME: Should we rebroadcast all devices attached to this
	 * interface to update the maxsect value? */
		
	/* Transfer interface info to ifcmd-reply */
	strcpy(ifcmd->ifname,interface->ifname);
	ifcmd->mtu = interface->mtu;
     }

#ifdef DEBUG
   printf("interface mtu reconfigured to %d\n",interface->mtu);
#endif
      
   send(conn,api_hdr, sizeof(struct apihdr), MSG_NOSIGNAL);
   send(conn,ifcmd,   sizeof(struct qaoed_interface_cmd), MSG_NOSIGNAL);
      
   return(0);
}


int processINTERFACE_STATUS(struct qconfig *conf, int conn,
			    struct apihdr *api_hdr,
			    struct qaoed_interface_cmd *ifcmd)
{
   struct ifst *interface;
   struct qaoed_if_info ifinfo;

   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK;
   api_hdr->arg_len = sizeof(struct qaoed_if_info);
   
   /* Zero struct */
   memset(&ifinfo,0,sizeof(struct qaoed_if_info));
   
   /* Try to lookup interface */
   interface = referenceint(ifcmd->ifname,conf);
   
   if(interface == NULL)
     api_hdr->error = API_FAILURE;
   else
     {
	ifinfo.mtu = interface->mtu;
	
	if(interface->ifname)
	  strcpy(ifinfo.name, interface->ifname);
	else
	  ifinfo.name[0] = 0; /* zt */
		
	if(interface->hwaddr)
	  memcpy(ifinfo.hwaddr, interface->hwaddr,6);
	else
	  memset(ifinfo.hwaddr,0,6);
     }
   
   /* Send reply */
   send(conn,api_hdr, sizeof(struct apihdr),              MSG_NOSIGNAL);
   send(conn,&ifinfo, sizeof(struct qaoed_interface_cmd), MSG_NOSIGNAL);   
      
   return(0);
}


int processTARGET_SETACL(struct qconfig *conf, int conn,
			 struct apihdr *api_hdr,
			 struct qaoed_target_info_detailed *target)
{
   struct aoedev *device;
   
   /* Encode reply */
   api_hdr->type = REPLY;
   api_hdr->error = API_ALLOK; /* Default to ok */
   api_hdr->arg_len = sizeof(struct qaoed_target_info_detailed);

   /* Search for matching device */
   for(device = conf->devices; device != NULL; device = device->next)
     if((device->slot == target->info.slot  &&
	 device->shelf == target->info.shelf))
       if(target->info.ifname[0] == 0 ||
	  device->interface == referenceint(target->info.ifname,conf))
	 {
	    /* wacl */
	    if(target->wacl[0] == 0)
	      {
		 /* Remove ACL */
		 if(device->wacl != NULL)
		   {
		      aclrefdown(device->wacl);
		      device->wacl = NULL;
		   }
	      }
	      else
	      {
		 /* Add / change ACL */
		 if(device->wacl != NULL)
		   {
		      aclrefdown(device->wacl);
		      device->wacl = NULL;
		   }
		 
		 device->wacl = referenceacl(target->wacl,conf);
		 
		 if(device->wacl == NULL)
		   api_hdr->error = API_FAILURE; 
		 else
		   aclrefup(device->wacl);
	      }
	    
	    
	    /* racl */
	    if(target->racl[0] == 0)
	      {
		 /* Remove ACL */
		 if(device->racl != NULL)
		   {
		      aclrefdown(device->racl);
		      device->racl = NULL;
		   }
	      }
	      else
	      {
		 /* Add / change ACL */
		 if(device->racl != NULL)
		   {
		      aclrefdown(device->racl);
		      device->racl = NULL;
		   }
		 
		 device->racl = referenceacl(target->racl,conf);
		 
		 if(device->racl == NULL)
		   api_hdr->error = API_FAILURE; 
		 else
		   aclrefup(device->racl);
	      }
	    
	    
	    
	    /* cfgsetacl */
	    if(target->cfgsetacl[0] == 0)
	      {
		 /* Remove ACL */
		 if(device->cfgsetacl != NULL)
		   {
		      aclrefdown(device->cfgsetacl);
		      device->cfgsetacl = NULL;
		   }
	      }
	      else
	      {
		 /* Add / change ACL */
		 if(device->cfgsetacl != NULL)
		   {
		      aclrefdown(device->cfgsetacl);
		      device->cfgsetacl = NULL;
		   }
		 
		 device->cfgsetacl = referenceacl(target->cfgsetacl,conf);
		 
		 if(device->cfgsetacl == NULL)
		   api_hdr->error = API_FAILURE; 
		 else
		   aclrefup(device->cfgsetacl);
	      }

	    
	    
	    /* cfgracl */
	    if(target->cfgracl[0] == 0)
	      {
		 /* Remove ACL */
		 if(device->cfgracl != NULL)
		   {
		      aclrefdown(device->cfgracl);
		      device->cfgracl = NULL;
		   }
	      }
	      else
	      {
		 /* Add / change ACL */
		 if(device->cfgracl != NULL)
		   {
		      aclrefdown(device->cfgracl);
		      device->cfgracl = NULL;
		   }
		 
		 device->cfgracl = referenceacl(target->cfgracl,conf);
		 
		 if(device->cfgracl == NULL)
		   api_hdr->error = API_FAILURE; 
		 else
		   aclrefup(device->cfgracl);
	      }
	    
	 }
      
   /* Send reply */
   send(conn,api_hdr, sizeof(struct apihdr),                     MSG_NOSIGNAL);
   send(conn,target,  sizeof(struct qaoed_target_info_detailed), MSG_NOSIGNAL);
      
   return(0);
}


int processTARGET_DEL(struct qconfig *conf, int conn,
		      struct apihdr *api_hdr, 
		      struct qaoed_target_info *target)
{
   struct aoedev *device;
   int ret = 0;
   int cnt = 0;
   
   api_hdr->error = API_ALLOK;
   api_hdr->type = REPLY;
   api_hdr->arg_len = 0;
   
   /* Place a writelock on the device-list before modifying */
   pthread_rwlock_wrlock(&conf->devlistlock);
   
   /* Make sure that we wont get more then one hit in the search */
   for(device = conf->devices; device != NULL; device = device->next)
     if((device->slot == target->slot  &&
	 device->shelf == target->shelf))
       if(target->ifname[0] == 0 ||
	  device->interface == referenceint(target->ifname,conf))
	 cnt++;
   
   /* unlock the device-list */
   if(cnt > 1)
     {
	/* More then one device matched the delete request */
	pthread_rwlock_unlock(&conf->devlistlock);
	api_hdr->error = API_FAILURE;
	api_hdr->arg_len = 0;
	send(conn, api_hdr, sizeof(struct apihdr)          , MSG_NOSIGNAL);
	return(-1);
     }
      
   if(cnt == 0)
     {
       /* No matching device */
	pthread_rwlock_unlock(&conf->devlistlock);
	api_hdr->error = API_FAILURE;
	api_hdr->arg_len = 0;
	send(conn, api_hdr, sizeof(struct apihdr)          , MSG_NOSIGNAL);
	return(-1);
     }

   /* Search for matching device */
   for(device = conf->devices; device != NULL; device = device->next)
     if((device->slot == target->slot  &&
	 device->shelf == target->shelf))
       if(target->ifname[0] == 0 ||
	  device->interface == referenceint(target->ifname,conf))
	 {
	    /* Unlink device from device-list */
	    if(device->prev != NULL)
	      {
		device->prev->next = device->next;
	      }
	    else
	      {
		/* First device */
		conf->devices = device->next;
	      }

	    if(device->next != NULL)
	      {
		device->next->prev = device->prev;
	      }
	  
#ifdef DEBUG
	    printf("Stopping thread for device  %s\n",
		   device->devicename);
#endif
  
	    /* Try to stop thread */
	    ret = pthread_cancel(device->threadID);
	    
	    if(ret == -1)
	      {
#ifdef DEBUG
		printf("Failed to stop device thread for %s\n",
		       device->devicename);
#endif
		
		logfunc(conf->log,
			LOG_ERR,
			"Failed to stop device thread for %s\n",
			device->devicename);
	      }
	 }
   
   /* Unlock write lock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
   if(ret == -1)
     api_hdr->error = API_FAILURE;
   else
     {
	/* Command succeeded */
	api_hdr->error = API_ALLOK;
	
	/* Remove reference from interface */
	qaoed_intrefdown(device->interface);
     }
   
   /* Send the reply to the client */
   send(conn, api_hdr, sizeof(struct apihdr)          , MSG_NOSIGNAL);
   
   return(ret);
}


int processTARGET_ADD(struct qconfig *conf, int conn, 
		      struct apihdr *api_hdr, 
		      struct qaoed_target_info *target)
{
  struct aoedev *device;
  struct aoedev *newdevice;

   api_hdr->error = API_ALLOK;
   api_hdr->type = REPLY;
   
   newdevice = (struct aoedev *) malloc(sizeof(struct aoedev));
   
   if(newdevice == NULL)
     {
	api_hdr->error = API_FAILURE;
	api_hdr->arg_len = 0;
	send(conn, api_hdr, sizeof(struct apihdr)          , MSG_NOSIGNAL);
	return(-1);
     }

#ifdef DEBUG
  printf("Processing new device!\n");
  fflush(stdout);
#endif

  newdevice->devicename = strdup(target->devicename);
  newdevice->shelf = target->shelf;
  newdevice->slot = target->slot;
  newdevice->broadcast = target->broadcast;
  newdevice->writecache = target->writecache;
  if(strlen(target->ifname) > 0)
    newdevice->interface = referenceint(target->ifname,conf);
  else
    newdevice->interface = NULL;
  newdevice->next       = NULL;
  newdevice->prev       = NULL;
  newdevice->log        = NULL;
  newdevice->fp         = NULL;

  /* Set default cfg lenght value */
  newdevice->cfg_len = 0;
  
  /* Set default ACL (none that is) */
  newdevice->wacl      = NULL;
  newdevice->racl      = NULL;
  newdevice->cfgsetacl = NULL;
  newdevice->cfgracl   = NULL;
  
  /* Assign default values from the default target */
  qaoed_devdefaults(conf,newdevice);

   /* Read-lock */
   pthread_rwlock_rdlock(&conf->devlistlock);
   
  /* Make sure we dont set the same slot/shelf on multiple targets
   * unless they point to the same device and uses different interfaces */
   if(conf->devices != NULL)
      for(device = conf->devices; device != NULL; device = device->next)
      if((device->slot == newdevice->slot  &&
	  device->shelf == newdevice->shelf))
      if((device->interface == newdevice->interface) ||
	 (strcmp(device->devicename,newdevice->devicename) != 0))
       {
	 /* Two devices can only share the same slot/shelf if 
	  * they both point to the same target and are attached 
	  * to different interfaces... */
	 free(newdevice);
	 /* Unlock before return */
	 pthread_rwlock_unlock(&conf->devlistlock);
	 return(-1);
       }

   /* Unlock */
   pthread_rwlock_unlock(&conf->devlistlock);
   
  /* Try to start our device */
  if(qaoed_startdevice(newdevice) == 0)
    {
       /* Place a writelock on the device-list before modifying */
       pthread_rwlock_wrlock(&conf->devlistlock);
       
       /* Find end of device list */
       if(conf->devices != NULL)
	 {
	   for(device=conf->devices; device->next!=NULL; device=device->next);;
	   
	   /* Add our device */
	   device->next  = newdevice;
	   newdevice->prev = device;
	 }
       else
	 conf->devices = newdevice;
       
       /* Unlock */
       pthread_rwlock_unlock(&conf->devlistlock);
       
#ifdef DEBUG
      printf("New device sucessfully started!\n");
      fflush(stdout);
#endif
      
      target->shelf = newdevice->shelf;
      target->slot = newdevice->slot;
      target->broadcast = newdevice->broadcast;
      target->writecache = newdevice->writecache;
      strcpy(target->devicename, newdevice->devicename);
      strcpy(target->ifname, newdevice->interface->ifname);
    }
  else
    {
#ifdef DEBUG
  printf("New device failed to start!\n");
  fflush(stdout);
#endif
       api_hdr->error = API_FAILURE;
    }
   
   /* Send back results */
   api_hdr->arg_len = sizeof(struct qaoed_target_info);
   send(conn, api_hdr, sizeof(struct apihdr)          ,  MSG_NOSIGNAL);
   send(conn, target,  sizeof(struct qaoed_target_info), MSG_NOSIGNAL);   
   
   return(0);
}


int processAPIrequest(struct qconfig *conf, int conn,
		      struct apihdr *api_hdr,void *arg)
{
   switch(api_hdr->cmd)
     {
      case API_CMD_STATUS:
	processSTATUSrequest(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_INTERFACES_LIST:
	processINT_LIST(conf, conn,api_hdr);
	break; 
	
      case API_CMD_INTERFACES_STATUS:
	processINTERFACE_STATUS(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_INTERFACES_ADD:
	printf("API_CMD_INTERFACES_ADD - not implemented yet\n");
	break;
	
      case API_CMD_INTERFACES_DEL:
	printf("API_CMD_INTERFACES_DEL - not implemented yet\n");
	break;
	
      case API_CMD_INTERFACES_SETMTU:
	processINTERFACE_SETMTU(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGET_LIST:
	processTARGET_LIST(conf,conn,api_hdr);
	break;
	
      case API_CMD_TARGET_STATUS:
	printf("API_CMD_TARGET_STATUS - not implemented yet\n");
	break;
	
      case API_CMD_TARGET_ADD:
#ifdef DEBUG
	printf("processTARGET_ADD(conf, conn,api_hdr,arg);\n");
#endif
	processTARGET_ADD(conf, conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGET_DEL:
#ifdef DEBUG
	printf("processTARGET_DEL(conf, conn,api_hdr,arg)\n");
#endif
	processTARGET_DEL(conf, conn,api_hdr,arg);
	break;
	
      case API_CMD_TARGET_SETOPTION:
	printf("API_CMD_TARGET_SETOPTION - not implemented yet\n");
	break;
	
      case API_CMD_TARGET_SETACL:
	processTARGET_SETACL(conf,conn,api_hdr,arg);
	break;
	
      case API_CMD_ACL_LIST:
	processACL_LIST(conf, conn,api_hdr);
	break;
	
      case API_CMD_ACL_STATUS:
	printf("API_CMD_ACL_STATUS - not implemented yet\n");
	break;
      case API_CMD_ACL_ADD:
	printf("API_CMD_ACL_ADD - not implemented yet\n");
	break;
      case API_CMD_ACL_DEL:
	printf("API_CMD_ACL_DEL - not implemented yet\n");
	break;
	
      default:
	printf("UNKNOWN!\n");
	break;
     }

   return(0);
}


int recvAPIrequest(int conn, struct qconfig *conf)
{
   struct apihdr api_hdr;
   char *arg = NULL;
   int n;

#ifdef DEBUG
   printf("recvAPIrequest()\n");
   fflush(stdout);
#endif

   while(1)
     {
	arg = NULL;
	
	/* Read api_hdr */
	n = recv(conn, &api_hdr, sizeof(struct apihdr),0);
	
	/* Make sure we got data of the correct size */
	if (n != sizeof(struct apihdr))
	  {
#ifdef DEBUG
	    if(n > 0)
	      printf("Wrong size for api_hdr: %d\n",n);
#endif

	     close(conn);
	     return(-1);
	  }
	
	/* Is there more data ? */
	if(api_hdr.arg_len > 0)
	  {
	     /* If its way out of proportions we close the socket */
	     if(api_hdr.arg_len > 2048)
	       {
#ifdef DEBUG
		 printf("argument lenght has bogus value: %d\n",api_hdr.arg_len);
#endif
		  close(conn);
		  return(-1);
	       }
	     
	     /* Allocate memory to hold options */
	     arg = (char *) malloc(api_hdr.arg_len);
	     if(arg == NULL)
	       {
#ifdef DEBUG
		 printf("malloc failed!\n");
#endif
		  close(conn);
		  return(-1);
	       }
	     
	     /* Read option arguments */
	     n = recv(conn, arg, api_hdr.arg_len,0);
	     
	     /* Make sure we got data of the right size */
	     if (n != api_hdr.arg_len)
	       {
#ifdef DEBUG
		 printf("Bogus lenght recieved from socket: %d\n",n);
#endif 
		  close(conn);
		  return(-1);
	       }
	  
	  }

	/* Process the request */
	processAPIrequest(conf, conn,&api_hdr,arg);
	
	/* Free memory used by args */
	if(arg != NULL)
	  free(arg);
	
     }
}



/* This is the API thread for qaoed */
int qaoed_api(struct qconfig *conf)
{
   int sock,conn;
   struct sockaddr_un lsock, rsock;
   socklen_t sockaddr_len;
   
   if(conf->sockpath == NULL)
     return(-1);

#ifdef DEBUG
   printf("qaoed_api() starting\n");
#endif
   
   /* Remove old socket file */
   unlink(conf->sockpath);
   
   /* Create a socket */
   if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
     {
#ifdef DEBUG
       fprintf(stdout,"Failed to create socket for API interface: socket() - %s",
		strerror(errno));
       fflush(stdout);

#endif
	logfunc(conf->log,LOG_ERR,
		"Failed to create socket for API interface: socket() - %s",
		strerror(errno));
	return(-1);
     }
   
   /* Fill out the sockaddr_un struct */
   lsock.sun_family = AF_UNIX;
   strcpy(lsock.sun_path, conf->sockpath);
   sockaddr_len = strlen(conf->sockpath) + sizeof(lsock.sun_family) + 1;

#ifdef DEBUG
   printf("Binding the socket\n");
#endif

   /* Bind the socket */
   if (bind(sock, (struct sockaddr *)&lsock, sockaddr_len) == -1)
     {
#ifdef DEBUG
       fprintf(stdout,"Failed to bind socket for API interface: bind() - %s\n",
	       strerror(errno));
       fflush(stdout);
#endif
	logfunc(conf->log,LOG_ERR,
		"Failed to bind socket for API interface: bind() - %s",
		strerror(errno));
	return(-1);
     }

#ifdef DEBUG
   printf("Listening for connections\n");
#endif

   if (listen(sock, 5) == -1) 
     {
#ifdef DEBUG
       fprintf(stdout,"Failed to listen on socket for API interface: listen()"
	       "- %s\n",
	       strerror(errno));
       fflush(stdout);
#endif
       logfunc(conf->log,LOG_ERR,
	       "Failed to listen on socket for API interface: listen() - %s",
	       strerror(errno));
       return(-1);
     }
   
#ifdef DEBUG
   printf("accepting connections\n");
#endif

   sockaddr_len =  sizeof(lsock.sun_family);
   while((conn = accept(sock, (struct sockaddr *)&rsock, &sockaddr_len)) >= 0)
     recvAPIrequest(conn,conf);
   
   if(conn < 0)
     {

#ifdef DEBUG
       fprintf(stdout,
	       "Failed to accept() on socket for API interface- %s\n",
	       strerror(errno));
       fflush(stdout);
#endif
	logfunc(conf->log,LOG_ERR,
		"Failed to accept() on socket for API interface: accept() - %s",
		strerror(errno));
	return(-1);
     }
   
#ifdef DEBUG
   printf("This should never happen!\n");
#endif

   /* Ehh? */
   return(0);
}

   

int qaoed_startapi(struct qconfig *conf)
{
   pthread_attr_t atr;
   
   if(conf == NULL)
     return(-1);
   
#ifdef DEBUG
   printf("qaoed_startapi()\n");
#endif

   /* Initialize the attributes */
   pthread_attr_init(&atr);
   pthread_attr_setdetachstate(&atr, PTHREAD_CREATE_JOINABLE);
	
   /* Start the API thread */
   if(pthread_create (&conf->APIthreadID,
		      &atr,
		      (void *)&qaoed_api,(void *)conf) != 0)
     {
	logfunc(conf->log,LOG_ERR,"Failed to start API interface for qaoed\n");
	return(-1);
     }	
		
   
   /* Return success status */
   return(0);
   
}


