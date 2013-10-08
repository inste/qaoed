#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>

#include "include/qaoed.h"
#include "include/acl.h"
#include "include/logging.h"

/* This function tries to match a mac-address to an access lists and
 *  * returns reject/accept if a match is found and the default policy 
 *  * for the access-list if no match is found. 
 *  */
int aclmatch(struct aclhdr *acl, unsigned char *h_addr)
{
   struct aclentry *aclrow = acl->acl;
   
   while(aclrow)
     {
	if(memcmp(h_addr,aclrow->h_dest,ETH_ALEN) == 0)
	  {
	     if(acl->log != NULL)
	       logfunc(acl->log,LOG_ERR,
		       "ACL %s %02X:%02X:%02X:%02X:%02X:%02X by %s\n",
		       (aclrow->rule) ? "allow" : "reject",
		       h_addr[0],
		       h_addr[1],
		       h_addr[2],
		       h_addr[3],
		       h_addr[4],
		       h_addr[5],
		       acl->name);	     

	     return(aclrow->rule);
	  }
	
	aclrow = aclrow->next;
     }
   
   if(acl->log != NULL)
     logfunc(acl->log,LOG_ERR,
	     "ACL default-%s %02X:%02X:%02X:%02X:%02X:%02X by %s\n",
	     (acl->defaultpolicy) ? "allow" : "reject",
	     h_addr[0],
	     h_addr[1],
	     h_addr[2],
	     h_addr[3],
	     h_addr[4],
	     h_addr[5],
	     acl->name);	     
   
   return(acl->defaultpolicy);
}


/* Resolv an access-list from its (char *) name to a pointer to an aclhdr */
struct aclhdr *referenceacl(char *name, struct qconfig *conf)
{
   struct aclhdr *acllist = conf->acllist;
   
   while(acllist)
     {
	if(strcmp(name,acllist->name) == 0)
	  return(acllist); 
	acllist = acllist->next;
     }
   
   logfunc(conf->log,LOG_ERR,
	   "Error: Unknown access-list named '%s' referenced in cfg.\n",name);
   return(NULL);
}


/* Resolv an access-list from its (int) number to a pointer to an aclhdr */
struct aclhdr *referenceaclint(int aclnum, struct qconfig *conf)
{
   struct aclhdr *acllist = conf->acllist;
   
   while(acllist)
     {
	if(aclnum == acllist->aclnum)
	  return(acllist); 
	acllist = acllist->next;
     }
   
   logfunc(conf->log,LOG_ERR,
	   "Error: Unknown access-list '%d' referenced.\n",aclnum);
   return(NULL);
}

void aclrefup(struct aclhdr *acl)
{
   if(acl == NULL)
     return;
      
      pthread_mutex_lock(&acl->lock);
      acl->refcnt++;
      pthread_mutex_unlock(&acl->lock);
}



void aclrefdown(struct aclhdr *acl)
{
   if(acl == NULL)
     return;
   
      pthread_mutex_lock(&acl->lock);
      acl->refcnt--;
      pthread_mutex_unlock(&acl->lock);
}


