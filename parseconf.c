
#define __USE_LARGEFILE64 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "include/qaoed.h"
#include "include/acl.h"
#include "include/logging.h"

/* #define DEBUG 1 */

/* This function parses the acl-sub-section to the device-section. 
 * It will find all the referenced access-lists and place a pointer 
 * to them in the aoedev-struct for each device 
 */
int parsedevacl(struct aoedev *dev, struct cfg *c, struct qconfig *conf)
{
  while(c)
    {
      if(c->type == ASSIGNMENT)
	switch(c->lvalue[0])
	  {
	    
	  case 'r' /* read */: 
	    if(strcmp(c->lvalue,"read") != 0)
	      goto error;
	    
	    /* Assign read acl */
	    dev->racl = referenceacl(c->rvalue,conf);
	    aclrefup(dev->racl);
	    break;
	    
	  case 'w' /* write */: 
	    if(strcmp(c->lvalue,"write") != 0)
	      goto error;
	    
	    /* Assign write acl */
	    dev->wacl = referenceacl(c->rvalue,conf);
	    aclrefup(dev->wacl);	     
	    break;
	     	     
	  case 'd' /* discover */: 
	  case 'c' /* cfgset / cfgread */: 
	    
	    if((strcmp(c->lvalue,"cfgread") == 0) ||
	       (strcmp(c->lvalue,"discover") == 0))
	      {
		dev->cfgracl = referenceacl(c->rvalue,conf);
		aclrefup(dev->cfgracl);
		break;
	      }
	    
	    if(strcmp(c->lvalue,"cfgset") == 0)
	      {
		dev->cfgsetacl = referenceacl(c->rvalue,conf);
		aclrefup(dev->cfgsetacl);
		break;
	      }
	    
	    /* else */
	    goto error;

	  default:
	  error:  
	     logfunc(conf->log,LOG_ERR,
		    "Unknown keyword in device->acl{ } section: %s\n",
		   c->lvalue);
	    return(-1);
	  }
      c = c->next;
    }
   
  return(0);
}

/* This function finds the ethernet interface named name in
 * the configuration conf and returns a pointer to it */
struct ifst *referenceint(char *name, struct qconfig *conf)
{
   struct ifst *iface = conf->intlist;
      
   if(conf->intlist == NULL)
     return(NULL);
        
   while(iface)
     {
	if(strcmp(name,iface->ifname) == 0)
	  return(iface);	 
	
	iface = iface->next;
     }
   
   return(NULL);
}

/* This function finds the ethernet interface named name in
 * the configuration conf and returns a pointer to it */
struct logging *referencelog(char *name, struct qconfig *conf)
{
   struct logging *log = conf->loglist;
   
   if(conf->loglist == NULL)
     return(NULL);
   
   while(log)
     {
	if(strcmp(name,log->name) == 0)
	  return(log);	 
	
	log = log->next;
     }

return(NULL);
}

/* Parses a device-block-section and fills out an aoedev-struct for 
 * each one. It will assign some default values and some invalid values
 * used to detect missing options. If the missing options can be found in 
 * the default section they will be assigned later on.
  */
struct aoedev * parseDEV(struct aoedev *devlist, struct cfg *c, 
			 struct qconfig *conf)
{
   struct aoedev *dev = devlist;
   
  if(dev == NULL)
     {
	/* First entry */
	devlist = dev = (struct aoedev *) malloc(sizeof(struct aoedev));
	dev->prev = NULL;;
     }
   else
     { 
	while(dev->next)
	  dev = dev->next;

	dev->next = (struct aoedev *) malloc(sizeof(struct aoedev));
	dev->next->prev = dev;
	dev->next->next = NULL;
	dev = dev->next;
     }
  
  /* default */
  dev->devicename = NULL;
  dev->interface  = NULL;
  dev->writecache = -1;    /* Invalid value */
  dev->broadcast  = -1;    /* Invalid valuer */
  dev->shelf      = 0xffff; /* Invalid value */
  dev->slot       = 0xff; /* Invalid value */
  dev->next       = NULL;
  dev->log       = NULL;
  dev->fp = NULL;
     
  /* Set default cfg lenght value */
  dev->cfg_len = 0;
  
  /* Set default ACL (none that is) */
  dev->wacl = NULL;
  dev->racl  = NULL;
  dev->cfgsetacl = NULL;
  dev->cfgracl   = NULL;
     
  while(c)
    {
      if(c->type == BLOCK && 
	 (strcmp(c->lvalue,"acl") == 0))
	if(parsedevacl(dev,c->block,conf) == -1)
	  return(NULL);

      if(c->type == ASSIGNMENT)
	switch(c->lvalue[0])
	  {
	  case 's' /* shelf / slot */: 
	    if(strcmp(c->lvalue,"shelf") == 0)
	      {
		dev->shelf = atoi(c->rvalue);
		break;
	      }
	    
	    if(strcmp(c->lvalue,"slot") == 0)
	      {
		dev->slot = atoi(c->rvalue);
		break;
	      }
	    
	    /* else */
	    goto error;
	    
	    break;
	     
	   case 'l' /* log / log-level */:
	     if((strcmp(c->lvalue,"log") != 0) &&
		(strcmp(c->lvalue,"log-level") != 0 ))
	       goto error;
	     
	     if(strcmp(c->lvalue,"log-level") == 0)
	       dev->log_level = loglvresolv(c->rvalue);
	     
	     /* Assign logging target */	     
	     if(strcmp(c->lvalue,"log") == 0)
	       {
		  
		  dev->log = referencelog(c->rvalue,conf);
		  
		  /* If the referenced logging target doesnt exist */
		  if(dev->log == NULL)
		    {
		       logfunc(conf->log,LOG_ERR,
			       "Failed to find logging target named %s\n",
			       c->rvalue);
		       
		       /* Fill out the struct with illegal values */
		       dev->log = 
			 (struct logging *) malloc(sizeof(struct logging));
		       
		       if(dev->log != NULL)
			 {
			    dev->log->name=strdup(c->rvalue);
			    dev->log->logtype = LOGTYPE_ERR; /* Invalid */
			 }
		    }
	       }
	     
	     
	     break;
	     
	  case 't' /* target */: 
	    if(strcmp(c->lvalue,"target") != 0)
	      goto error;
	    
	    dev->devicename = (char *) strdup(c->rvalue); 
	    break;
	    
	  case 'i' /* interface */: 
	    if(strcmp(c->lvalue,"interface") != 0)
	      goto error;
	     
	     dev->interface = referenceint(c->rvalue,conf);
	     break;
	    
	  case 'w' /* writecache */: 
	    if(strcmp(c->lvalue,"writecache") != 0)
	      goto error;
	    
	    if(strcmp(c->rvalue,"off") == 0)
	      dev->writecache = 0;
	    else
	      dev->writecache = 1; /* Defaults to on */

	    break;

	  case 'b' /* broadcast */: 
	    if(strcmp(c->lvalue,"broadcast") != 0)
	      goto error;
	    
	    if(strcmp(c->rvalue,"off") == 0)
	      dev->broadcast = 0;
	    else
	      dev->broadcast = 1; /* Defaults to on */

	    break;
	    
	  default:
	  error:
	     logfunc(conf->log,LOG_ERR,
		     "Unknown keyword in device{ } section: %s\n",c->lvalue);
	    return(NULL);
	  }
      
      c = c->next;
    }
  
  return(devlist);
}

/* Parse the default-section and return an aoedev-struct containing
 * the result. Default values for broadcast and writecache are set to on.
 */
struct aoedev * findDFLT(struct aoedev *devlist, struct cfg *c,
			 struct qconfig *conf)
{
  for(c = c->block;c;c = c->next)
    if(c->type == BLOCK && (strcmp(c->lvalue,"default") == 0))
      devlist = parseDEV(devlist,c->block,conf);
    else
      if(c->type == BLOCK)
	devlist = findDFLT(devlist,c,conf);

   if(devlist != NULL)
     {    
	if(devlist->writecache == -1)     /* Invalid value */
	  devlist->writecache = 1;
	if(devlist->broadcast      == -1)     /* Invalid valuer */
	  devlist->broadcast  = 1;
     }
   
  return(devlist);
}

/* Find all the device-blocks in the configuration file and send them 
 * to parseDEV() for processing
 */
struct aoedev * findDEV(struct aoedev *devlist, struct cfg *c, 
			struct qconfig *conf)
{
  for(c = c->block;c;c = c->next)
    if(c->type == BLOCK && (strcmp(c->lvalue,"device") == 0))
      devlist = parseDEV(devlist,c->block,conf);
    else
      if(c->type == BLOCK)
	devlist = findDEV(devlist,c,conf);
   
  return(devlist);
}

/* Parses a logging-block-section and fills out a logging-struct for
 * each one. 
 */
struct logging * parseLOG(struct logging *loglist, struct cfg *c, 
			  struct qconfig *conf)
{
  struct logging *log = loglist;

  if(log == NULL)
     {
	loglist = log = (struct logging *) malloc(sizeof(struct logging));
     }
  else
    { 
       while(log->next)
	 log = log->next;
       
       log = log->next = (struct logging *) malloc(sizeof(struct logging));
    }
  
  /* default */
  log->name              = NULL; /* Invalid value */
  log->logtype           = LOGTYPE_SYSLOG; /* default value */
  log->syslog_level      = LOG_INFO;
  log->syslog_facility   = LOG_DAEMON; 
  log->filename          = NULL; /* Invalid value */
  	  
  while(c)
    {
      if(c->type == ASSIGNMENT)
	switch(c->lvalue[0])
	  {
	   case 't': /* type */
	    if(strcmp(c->lvalue,"type") == 0)
	       {
		  if(strcmp(c->rvalue,"file") == 0)
		    log->logtype = LOGTYPE_FILE;
		  else
		    log->logtype = LOGTYPE_SYSLOG; /* Default */
	       }
	     else
	       goto error;
	     break;
	    
	   case 's':
	     if(((strcmp(c->lvalue,"syslog-level") != 0) &&
		 (strcmp(c->lvalue,"syslog-facility") != 0 )) ||
		(log->logtype != LOGTYPE_SYSLOG))
	       goto error;
       
	     if(strcmp(c->lvalue,"syslog-level") == 0)
	       log->syslog_level = syslgresolv(c->rvalue);
	     
	     if(strcmp(c->lvalue,"syslog-facility") == 0)
	       log->syslog_facility = syslgresolv(c->rvalue);
	     
	    break;
	     
	   case 'f': /* filename to log to */
	     if(strcmp(c->lvalue,"filename") == 0)
	       log->filename = strdup(c->rvalue);
	     else
	       goto error;
	       
	       break;
	     
	   case 'n': /* name of logging target*/
	     if(strcmp(c->lvalue,"name") == 0)
	       log->name = strdup(c->rvalue);
	     else
	       goto error;
	     
	     break;
	    
	  default:
	  error:
	     logfunc(conf->log,LOG_ERR,
		     "Unknown keyword in logging{ } section: %s\n",c->lvalue);
	    return(NULL);
	  }
      
      c = c->next;
    }
  
  return(loglist);
}



/* Find all logging targets defined in the config file and parse them */
struct logging * findLOG(struct logging *loglist, struct cfg *c,
			struct qconfig *conf)
{

  for(c = c->block;c;c = c->next)
    if(c->type == BLOCK && (strcmp(c->lvalue,"logging") == 0))
      loglist = parseLOG(loglist,c->block,conf);
    else
      if(c->type == BLOCK)
	loglist = findLOG(loglist,c,conf);
  
  return(loglist);
}

/* This function creates the default logging scheme if the users
 * didnt create one in the configuration file */
struct logging * createdefaultlog( struct logging *loglist)
{
   struct logging *log = loglist;

  if(log == NULL)
     {
	loglist = log = (struct logging *) malloc(sizeof(struct logging));
     }
  else
    { 
       while(log->next)
	 log = log->next;
       log = log->next = (struct logging *) malloc(sizeof(struct logging));
    }
  
  /* default */
  log->name              = strdup("default");
  log->logtype           = LOGTYPE_SYSLOG; 
  log->syslog_level      = LOG_INFO;
  log->syslog_facility   = LOG_DAEMON; 
  log->filename          = NULL; 
  log->next              = NULL;
   
   return(loglist);
}

struct logging * createLOG(struct cfg *c,
			   struct qconfig *conf)
{
   struct logging * loglist;
   struct logging * log;
   
   /* Read logging targets from configuration file */
   loglist = findLOG(NULL,c,conf);

   /* if no other logging scheme has been created we add the default one */
   if(loglist == NULL)
     loglist = createdefaultlog(loglist);
   
   /* Attach the loglist to the config, this is needed so that we can
    * use referencelog(). Its a bit ugly, I know... */
   conf->loglist = loglist;
   
   /* Figure out if there is a default logging scheme created */
   log = referencelog("default",conf);

   if( log == NULL) /* It not, attach a logging scheme named at the end */
     {   
	/* Find end of loglist */
	log = loglist;
	while(log->next)
	  log = log->next;
	
	/* Attach the default scheme */
	log->next = createdefaultlog(loglist);
     }
   
   /* Return the result */
   return(loglist);
}


/* Convert a mac address from char * to and array */
int aclmac(unsigned char *h_addr,char *mac,struct qconfig *conf)
{
  int ret = 0;
  unsigned int conv[6];
   
  /* Convert mac address */
  ret = sscanf(mac, "%X:%X:%X:%X:%X:%X",&conv[0], &conv[1], &conv[2], 
	       &conv[3], &conv[4], &conv[5]);
  
  if (ret != 6)
    {
       logfunc(conf->log,LOG_ERR,
	       "Failed to convert %s to h_addr[]-format: %d\n",mac,ret);
      return (-1);
    }

  while(ret--)
    h_addr[ret] = conv[ret];
  
  return(0);
}

/* Convert the access-list policy from char * to int */
unsigned char aclpolicy(char *p)
{
  if(strcmp(p,"accept") == 0)
    return(ACL_ACCEPT);
  
  if(strcmp(p,"reject") == 1)
    return(ACL_REJECT);
  
  return(ACL_ERROR);

}

/* Read an access-list block from the config file and turn it into 
 * a linked list of aclhdr and aclentry. */
struct aclhdr * parseACL(struct aclhdr *acllist, struct cfg *c,
			 struct qconfig *conf)
{
  struct aclhdr *acl = acllist;
  struct aclentry *row = NULL;

  if(acl == NULL)
    acllist = acl = (struct aclhdr *) malloc(sizeof(struct aclhdr));
  else
    { 
      while(acl->next)
	acl = acl->next;

      acl = acl->next = (struct aclhdr *) malloc(sizeof(struct aclhdr));
    }
  
  /* default */
  acl->next = NULL;
  acl->name = NULL;
  acl->acl  = NULL;
  acl->log  = NULL;
  acl->defaultpolicy = ACL_ERROR;
  acl->aclnum = conf->aclnumplan++;

  while(c)
     {
	switch(c->lvalue[0])
	  {
	   case 'n' /* name */: 
	     if(strcmp(c->lvalue,"name") != 0)
	       goto error;
	     
	     acl->name = strdup(c->rvalue);
	     break;
	     
	   case 'p' /* policy */: 
	     if(strcmp(c->lvalue,"policy") != 0)
	       goto error;
	     
	     acl->defaultpolicy = aclpolicy(c->rvalue);
	     break;
	     
	   case 'l' /* log */: 
	     if(strcmp(c->lvalue,"log") != 0)
	       goto error;
	     
	     /* Assign logging target */
	     acl->log = referencelog(c->rvalue,conf);
	     
	     /* If the referenced logging target doesnt exist */
	     if(acl->log == NULL)
	       {
		  logfunc(conf->log,LOG_ERR,
			  "Failed to find logging target named %s\n",
			  c->rvalue);
		  
		  /* Fill out the struct with illegal values */
		  acl->log = 
		    (struct logging *) malloc(sizeof(struct logging));
		  
		  if(acl->log != NULL)
		    {
		     acl->log->name=strdup(c->rvalue);
		       acl->log->logtype = LOGTYPE_ERR; /* Invalid */
		  }
	       }
	     
	     break;
	     
	   case 'a' /* accept */: 
	   case 'r' /* reject */: 
	     if((strcmp(c->lvalue,"accept") != 0) ||
		(strcmp(c->lvalue,"reject") != 0))
	       {	    	    
		  if(acl->acl == NULL)
		    acl->acl = row = (struct aclentry *) 
		      malloc(sizeof(struct aclhdr));
		  else
		    { 
		       row = acl->acl;
		       while(row->next)
			 row = row->next;
		       
		       row = row->next = (struct aclentry *)
		       malloc(sizeof(struct aclentry));
		    }
		  
		  row->next = NULL;
		  row->rule = aclpolicy(c->lvalue);
		  row->mask = 48;
		  
		  if(aclmac(row->h_dest,c->rvalue,conf) != 0)
		    return(NULL);
	       }
	     else
	       goto error;
	     
	     break;
	     
	   default:
	     error:
	     logfunc(conf->log,LOG_ERR,
		     "Unknown keyword in access-list: %s\n",c->lvalue);
	     return(NULL);
	  }
	
	c = c->next;
     }
   
   return(acllist);
}




/* Find all access-lists defined in the config file and parse it */
struct aclhdr * findACL(struct aclhdr *acllist, struct cfg *c,
			struct qconfig *conf)
{
   
   for(c = c->block;c;c = c->next)
     if(c->type == BLOCK && (strcmp(c->lvalue,"access-list") == 0))
       acllist = parseACL(acllist,c->block,conf);
   else
     if(c->type == BLOCK)
       acllist = findACL(acllist,c,conf);
   
   return(acllist);
}


/* Parses an interface-block-section and fills out an ifst-struct for 
 * each one. It will assign some default values.
 */
struct ifst *parseINT(struct ifst *iflist, struct cfg *c,
		      struct qconfig *conf)
{
  struct ifst *ifentry = iflist;

  if(ifentry == NULL)
    {
      /* First entry */
      iflist = ifentry = (struct ifst *) malloc(sizeof(struct ifst));
      ifentry->prev = NULL;
    }
  else
    { 
      while(ifentry->next)
	ifentry = ifentry->next;
      
      /* Add entry to linked list */
      ifentry->next = (struct ifst *) malloc(sizeof(struct ifst));
      ifentry->next->prev = ifentry;
      ifentry->next->next = NULL;
      ifentry = ifentry->next;
    }
  
   /* Set the default logging level */
   ifentry->log_level = conf->log_level;

   /* Set the default MTU to 1500 */
   ifentry->mtu = 1500;
   
   /* Set the default logging target */
   ifentry->log = referencelog("default",conf);
   
  while(c)
    {
      if(c->type == ASSIGNMENT)
	switch(c->lvalue[0])
	  {
	   case 'm': /* mtu */
	     if(strcmp(c->lvalue,"mtu") != 0)
	       goto error;
	     
	     if(strcmp(c->rvalue,"auto") == 0)
	       ifentry->mtu = 0; /* 0 == Autodetect MTU */
	     else
	       if(sscanf(c->rvalue,"%d",&ifentry->mtu) != 1)
		 {
		    logfunc(conf->log,LOG_ERR,
			    "Failed to understand interface mtu setting:"
			    "%s = %s;\n",c->lvalue,c->rvalue);
		    return(NULL);
		 }

	     if(ifentry->buffsize < ifentry->mtu)
	       ifentry->buffsize = ifentry->mtu + 14;

	     break;
	    
	  case 'i' /* interface */: 
	    if(strcmp(c->lvalue,"interface") != 0)
	      goto error;
	     
	     ifentry->ifname = strdup(c->rvalue);
	     break;

	   case 'l' /* log/loglevel */:
	     if((strcmp(c->lvalue,"log") != 0) &&
		(strcmp(c->lvalue,"log-level") != 0 ))
	       goto error;
	     
	     if(strcmp(c->lvalue,"log-level") == 0)
	       ifentry->log_level = loglvresolv(c->rvalue);
	     
	     if(strcmp(c->lvalue,"log") == 0)
	       {
		  ifentry->log = referencelog(c->rvalue,conf);
		  
		  /* If the referenced logging target doesnt exist */
		  if(ifentry->log == NULL)
		    {
		       logfunc(conf->log,LOG_ERR,
			       "Failed to find logging target named %s\n",
			       c->rvalue);
		       
		       /* Fill out the struct with illegal values */
		       ifentry->log = 
			 (struct logging *) malloc(sizeof(struct logging));
		       
		       if(ifentry->log != NULL)
			 {
			    ifentry->log->name=strdup(c->rvalue);
			    ifentry->log->logtype = LOGTYPE_ERR; /* Invalid */
			 }
		    }
	       }
	     
	     break;

	     
	  default:
	  error:
	     logfunc(conf->log,LOG_ERR,
		     "Unknown keyword in interface-section: %s\n",c->lvalue);
	    return(NULL);
	  }
      
      c = c->next;
    }
   
  return(iflist);
}


/* find all referenced interfaces. It will firt look for interface-sections
 * and then look for _any_ 'interface = reference' in the configuration file */
struct ifst * findINT(struct ifst *iflist, struct cfg *c,
		      struct qconfig *conf)
{
   struct ifst *ifent = iflist;
   struct cfg *cstart = c;
   
   /* Find interface sections */
   for(c = c->block;c;c = c->next)
     if(c->type == BLOCK && (strcmp(c->lvalue,"interface") == 0))
       iflist = parseINT(iflist,c->block,conf);
   
   /* Rewind c pointer and ifent */
   c = cstart;
   ifent = iflist; 
      
   /* Find any interface-reference anywhere else in the config file */
   for(c = c->block;c;c = c->next)
     if(c->type == ASSIGNMENT && (strcmp(c->lvalue,"interface") == 0))
       {
	  if(ifent == NULL)
	    {
	       iflist = ifent = (struct ifst *) malloc(sizeof(struct ifst));
	       ifent->next = NULL;
	    }
	  else
	    {
	       while(ifent->next)
		 {
		    if((strcmp(ifent->ifname,c->rvalue) == 0))
		      return(iflist); /* Duplicate entry */		    
		    
		    ifent = ifent->next;
		 }
	       
	       if((strcmp(ifent->ifname,c->rvalue) == 0))
		 return(iflist); /* Duplicate entry */		    
	       
	       
	       ifent = ifent->next =
		 (struct ifst *) malloc(sizeof(struct ifst));
	       ifent->next = NULL;
	    }
	  
	  /* Store interface name */
	  ifent->ifname = strdup(c->rvalue);
	  ifent->mtu = 1500; /* default */
	  ifent->log = referencelog("default",conf);
       }
   else
     if(c->type == BLOCK)
       iflist = findINT(iflist,c,conf);

   return(iflist);
}


/* This function reads and parses all the global configuration options 
 * in the configuration file. For instance the log-level. 
 */
struct qconfig *readglobal(struct cfg *c, struct logging *log)
{
   struct qconfig *conf;
   
   conf = (struct qconfig *) malloc(sizeof(struct qconfig));
   if(conf == NULL)
     {
	perror("malloc");
	exit(-1);
     }
   
   conf->log_level = 0; /* Default to none */
   conf->syslog_level = LOG_INFO;
   conf->syslog_facility = LOG_DAEMON;
   conf->log = log;
   conf->sockpath = NULL; /* No API-socket by default */
   conf->aclnumplan = 1; /* Start numbering access-lists from one (1) */
   
   /* Initialize the read-write-lock for the device-list */
   pthread_rwlock_init(&conf->devlistlock,NULL);
   
   /* Initialize the read-write-lock for the interface-list */
   pthread_rwlock_init(&conf->intlistlock,NULL);

  while(c)
    {
      if(c->type == ASSIGNMENT)
	switch(c->lvalue[0])
	  {
	  case 'l':
	    if(strcmp(c->lvalue,"log-level") != 0)
	      goto error;

	     conf->log_level = loglvresolv(c->rvalue);
	    break;
	   
          case 'i':
            if(strcmp(c->lvalue,"include") != 0)
              goto error;
            break;
	     
	   case 'a': /* Path to unix domain socket used by API */
	     if(strcmp(c->lvalue,"apisocket") != 0)
		goto error;
		
		conf->sockpath=strdup(c->rvalue);
		break;
	     
	  error:
	  default:
	     
	     if(log == NULL)
	       {
		  /* We dont have any log-target yet, so we sent the 
		   * error messages to stderr and syslogd */
		  fprintf(stderr,"Unknown statement: %s\n",c->lvalue);
		  syslog(LOG_DAEMON|LOG_ERR,"Unknown statement: %s\n",
			 c->lvalue);
	       }
	     else
	       logfunc(log,LOG_ERR, "Unknown statement: %s\n",c->lvalue);
	     
	    return(NULL);
	  }
      c = c->next;
    }


  return(conf);
}

/* Assign default values to a device */
void qaoed_devdefaults(struct qconfig *conf,struct aoedev *device)
{
  /* If not assigned we take the value from the default struct */
  if(device->shelf == 0xffff && conf->devdefault != NULL)
    device->shelf = conf->devdefault->shelf;
  
  /* If not assigned we take the value from the default struct */
  if(device->slot == 0xff && conf->devdefault != NULL)
    device->slot = conf->devdefault->slot++;
  
  if(device->writecache == -1 && conf->devdefault != NULL)
    device->writecache = conf->devdefault->writecache;
  
  if(device->broadcast == -1 && conf->devdefault != NULL)
    device->broadcast = conf->devdefault->broadcast;
  
  if(device->interface == NULL && conf->devdefault != NULL)
    device->interface = conf->devdefault->interface;
  
  /* Access lists */
  if(device->wacl == NULL && conf->devdefault != NULL )
    device->wacl = conf->devdefault->wacl;
  
  if(device->racl == NULL && conf->devdefault != NULL)
    device->racl = conf->devdefault->racl;
  
  if(device->cfgracl == NULL && conf->devdefault != NULL)
    device->cfgracl = conf->devdefault->cfgracl;
  
  if(device->cfgsetacl == NULL && conf->devdefault != NULL)
    device->cfgsetacl = conf->devdefault->cfgsetacl;
  
  /* Logging target */
  if(device->log == NULL && conf->devdefault != NULL)
    device->log = conf->devdefault->log;
  
  /* If its still NULL, we assign the default log target */
  if(device->log == NULL)
    device->log = referencelog("default",conf);
  
  /* If broadcast&writecache still doesnt have values we set them here */
  if(device->writecache == -1)
    device->writecache = 1;
  
  if(device->broadcast == -1)
    device->broadcast = 1;

  return;
}

/* This function will go through the device configuration and assign
 * default values where applicable */
int qaoed_defaultconfig(struct qconfig *conf)
{
   struct aoedev *device;
   
   for(device = conf->devices; device != NULL; device = device->next)
     qaoed_devdefaults(conf,device);
   
   return(0);
}


/* This will go through the configuration and open the socket for 
 * all the interfaces. It will also request the hw-address of each one, 
 * check that the MTU setting is valid and assign the MTU-value if needed */
int qaoed_openinterfaces(struct qconfig *conf)
{
   struct ifst *ifent = conf->intlist;
   
   if(ifent == NULL)
     {    
	logfunc(conf->log,LOG_ERR,
		"Error: No interface(s) defined in the configuration\n");
	return(-1);
     }
	
	
   while(ifent)
     {
	ifent->sock =  qaoed_opensock(ifent);
	
	if(ifent->sock == -1)
	  {
	     logfunc(conf->log,LOG_ERR,
		     "Failed to open network socket for interface %s\n",
		     ifent->ifname);
	     return(-1);
	  }
	
	/* Get the hwaddress of the interface */
	getifhwaddr(ifent);
	
	/* If the MTU setting is set to auto (0) we get the MTU from
	 * the interface */
	if(ifent->mtu == 0)
	  ifent->mtu = getifmtu(ifent);
	
	/* Recalibrate buffsize of needed */
	if(ifent->buffsize < ifent->mtu)
	  ifent->buffsize = ifent->mtu + 14;

	/* Make sure that the set MTU isnt higher then the MTU of the
	 * interface */
	if(ifent->mtu > getifmtu(ifent))
	  {
	     logfunc(conf->log,LOG_ERR,
		    "OS reports MTU=%d for interface %s and that is lower then"
		    "the value configured in the configuration file (%d)\n",
		    getifmtu(ifent),ifent->ifname,ifent->mtu);
	     return(-1);
	  }
	
	ifent = ifent->next;
     }
   return(0);
}

void destroyqcfg(struct qconfig *conf)
{
   struct aoedev *device = conf->devices;
   struct logging *log   = conf->log;
   struct ifst *ifent    = conf->intlist;
   struct aclhdr *acl    = conf->acllist;
   void *next;

   /* Close and destroy the logging targets */
   while(log)
     {
	free(log->name);
	
	if(log->logtype == LOGTYPE_FILE)
	  {
	     fclose(log->fp);
	     free(log->filename);
	  }

	{ next = log->next; free(log); log = (struct logging *) next; }
     }
   
   /* Close and destroy the interfaces */
   while(ifent)
     {
       if(ifent->ifname != NULL)
	 free(ifent->ifname);
       if(ifent->hwaddr != NULL)
	 free(ifent->hwaddr);
       if(ifent->sock > -1)
	 close(ifent->sock);
       ifent->sock = -1;
	
	{ next = ifent->next; free(ifent); ifent = (struct ifst *) next; }
     }
   
   /* Close and destroy the devices */
   while(device)
     {
	free(device->devicename);
	fclose(device->fp);
	pthread_mutex_destroy(&device->queuelock);

	{ next = device->next; free(device); device = (struct aoedev *) next; }
     }
   
   /* Destroy the default device */
   if(conf->devdefault != NULL)
     {
	free(conf->devdefault);
     }
   
   /* Destroy the access-lists */
   while(acl)
     {
	struct aclentry *aclr = acl->acl;
	free(acl->name);
	
	/* Destroy all the rows in each access-list */
	while(aclr)
	  { next = aclr->next; free(aclr); aclr = (struct aclentry *) next; }
	  
	{ next = acl->next; free(acl); acl = (struct aclhdr *) next; }
     }
   
   /* And finaly, destroy the qconfig */
   free(conf);
   
}



/* This function will go through the configuration to make sure 
 * that it is valid. It will make sure the target exists and that 
 * the slot/shelf-numbers are ok. 
 */
int qaoed_validateconfig(struct qconfig *conf)
{
   struct aoedev *device;
   struct aoedev *search;
   struct ifst *ifent;
   struct stat st;
   struct aclhdr *acl;
   
   /* Make all access-lists have valid logging targets*/
   for(acl = conf->acllist; acl != NULL; acl = acl->next)
     {
	if(acl->log == NULL)
	  continue; /* Its perfectly valid for an access-list not to have 
		     * a logging target attached. */
	
	if(acl->log->logtype == LOGTYPE_ERR)
	  return(-1); /* This in the other hand is an error :( */

     }
   
   /* Make all the interfaces have valid logging targets and mtu set*/
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     {
	if(ifent->log == NULL)
	  return(-1);
	
	if(ifent->log->logtype == LOGTYPE_ERR)
	  return(-1);
	
	if(ifent->mtu == 0)
	  return(-1);
     }
   
  /* Make sure target exists and is readable */
  for(device = conf->devices; device != NULL; device = device->next)
    {
      if(device->devicename == NULL)
	{
	   logfunc(conf->log,LOG_ERR,
		   "error: devicename is null for device in device-list\n");
	  return(-1);
	}
      
      if(stat(device->devicename,&st) == -1)
	{
	   logfunc(conf->log,LOG_ERR,
		   "Failed to access %s: %s\n",
		   device->devicename,strerror(errno));
	   return(-1);
	}
      
      /* Make sure that the file is a regular file or a block device */
      if(!(S_ISBLK(st.st_mode) ||
	  S_ISREG(st.st_mode)
#ifdef __FreeBSD__ 
       || S_ISCHR(st.st_mode)  /* On freebsd blkdevices are char devices (!?) */
#endif
))
	 {
#ifdef DEBUG	
	 printf("S_ISREG(st.st_mode) == %d\n",
	 S_ISREG(st.st_mode));

	 printf("S_ISBLK(st.st_mode) == %d\n",
	 S_ISBLK(st.st_mode));

	 printf("S_ISCHR(st.st_mode) == %d\n",
	 S_ISCHR(st.st_mode));
#endif
	    logfunc(conf->log,LOG_ERR,
		   "error: %s is not a regular file or block device!\n",
		    device->devicename);
	 }
    }

   /* Make sure each device has an ethernet interface attached */
   for(device = conf->devices; device != NULL; device = device->next)
     if(device->interface == NULL)
       {
	  logfunc(conf->log,LOG_ERR,
		  "error: device %d:%d has no interface attached!\n",
		  device->shelf,device->slot);
	  return(-1);
       }
   
   
   /* Make sure each device has a valid slot and shelf number */
   for(device = conf->devices; device != NULL; device = device->next)
     if(device->slot ==  0xff  ||
	device->shelf == 0xffff)
       {
	  logfunc(conf->log,LOG_ERR,
		  "Target %s does not have a valid slot/shelf configuration -  %d:%d\n",
		  device->devicename,device->shelf,device->slot);
	  return(-1);
       }       
   
   
   /* Make sure we dont set the same slot/shelf on multiple targets
    * unless they point to the same device and uses different interfaces */
   for(device = conf->devices; device != NULL; device = device->next)
     for(search = conf->devices; search != NULL; search = search->next)
       if((device->slot == search->slot  &&
	   device->shelf == search->shelf) && search != device)
	 if((device->interface == search->interface) ||
	    (strcmp(device->devicename,search->devicename) != 0))
	   {
	      /* Two devices can only share the same slot/shelf if 
	       * they both point to the same target and are attached 
	       * to different interfaces... */
	      
	      logfunc(conf->log,LOG_ERR,
		      "Two different devices share the same slot/shelf - %d:%d\n",
		      device->shelf,device->slot);
	      return(-1);
	   }
    
   return(0);
}


int openlogs(struct qconfig *conf)
{
   struct logging *log = conf->loglist;
   
   while(log)
     {
	if(logstart(log) == -1)
	  return(-1);
	
	log = log->next;
     }
   
   return(0);
}

void cfgerrorhandler(struct cfgerror *er, char *format,...)
{
   struct logging *log;
   va_list args;
   va_start( args, format );
      
   if(er->priv == NULL)
     {
	/* We write the errors to stderr untill we have anywhere else to
	 * write them to */
	vfprintf(stderr,format,args);
	
     }
   else
     {
	/* If we have a logging target defined, send the error-messages
	 * to that logging-target */
	log = (struct logging *) er->priv;
	vlogfunc(log,LOG_ERR,format,args);
     }
   
   return;
}


/* Read and verify the configuration file */
/* The function returns NULL if something is wrong */
struct qconfig *qaoed_loadconfig(char *filename)
{
   struct qconfig *conf;
   struct cfg *c;
   struct cfgerror er;

#ifdef DEBUG
   printf("qaoed_loadconfig(%s)\n",filename);
#endif
   
   /* Create an error handler for the rcfg-framework */
   er.logfunc = &cfgerrorhandler;
   er.priv = NULL; /* Logging target to use for err-messages from readconfig */

#ifdef DEBUG
   printf("readconfig(%s) - starting \n",filename);
#endif
   
   /* Convert the tokens to cfg-tree */
   c = readconfig(filename,&er);
   
#ifdef DEBUG
   printf("readconfig(%s) - complete \n",filename);
#endif
   
   if(c != NULL && c->block != NULL)
     {
	/* Read the global config options */
	conf = readglobal(c->block,NULL);
	if(conf == NULL)
	  return(conf);
	
	/* Read logging targets from disk 
	 * and create default if needed */
	conf->loglist = createLOG(c,conf);
	if(conf->loglist == NULL)
	  return(NULL);

	if(openlogs(conf) == -1)
	  return(NULL);
	
	/* Find all the interfaces */
	conf->intlist = findINT(NULL,c,conf);
	if(conf->intlist == NULL)
	  {
	     logfunc(conf->log,LOG_ERR,
		    "No valid interface definitions found in %s\n",
		     filename);
	     return(NULL);
	  }
	
	/* Find all the access-lists */
	conf->acllist = findACL(NULL,c,conf);
	
	/* Find the defaults for devices (if any)*/
	conf->devdefault = findDFLT(NULL,c,conf);
	
	/* Find all the exported devices */
	conf->devices = findDEV(NULL,c,conf);
	if(conf->devices == NULL)
	  return(NULL);
	
	/* Apply the default configuration where applicable */
	if(qaoed_defaultconfig(conf)  == -1)
	  return(NULL);
	
	/* Open the interfaces */
	if(qaoed_openinterfaces(conf) == -1)
	  return(NULL);
	
	/* Validate the configuration to make sure it will work */
	if(qaoed_validateconfig(conf) == -1)
	  return(NULL);

	/* At this point we switch log-target as well */
	conf->log = referencelog("default",conf);
	
	/* Okey, we have a valid qconf-struct :) */
	return(conf);
     }
   
   return(NULL);
}
