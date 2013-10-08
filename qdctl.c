#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "include/api.h"

#define SOCK_PATH "/tmp/qaoedsocket"

/* #define DEBUG 1 */

int processSTATUSreply(int sock,struct apihdr *api_hdr,void *arg)
{
  struct qaoed_status *status = (struct qaoed_status *) arg;
  
  if(api_hdr->cmd == API_CMD_STATUS && 
     api_hdr->arg_len == sizeof(struct qaoed_status))
    {
      printf("Qaoed status: \n");
      printf("   Running threads:   %d\n",status->num_threads);
      printf("   exported targets:  %d\n",status->num_targets);
      printf("   open interfaces:   %d\n",status->num_interfaces);
    }

  return(0);
}

int processTARGETADDreply(int sock,struct apihdr *api_hdr,void *arg)
{
  struct qaoed_target_info *target = (struct qaoed_target_info *) arg;
  
  if(api_hdr->cmd == API_CMD_TARGET_ADD && 
     api_hdr->arg_len == sizeof(struct qaoed_target_info))
    {
      if(api_hdr->error == API_ALLOK)
	printf("Successfully added new target\n");
      else
	printf("Failed to add new target\n");
	  printf("Target: %s\n",target->devicename);
	  printf("Shelf:  %d\n",target->shelf);
	  printf("Slot:   %d\n",target->slot);
	  printf("Interface: %s\n",target->ifname);
	  printf("Broadcast: %d\n",target->broadcast);
	  printf("Writecache: %d\n",target->writecache);
    }
  else
    printf("Failed to understand reply from server\n");
  return(0);
}

int processACLLISTreply(int sock,struct apihdr *api_hdr,void *arg)
{
   int cnt; 
   struct qaoed_acl_info *aclinfo = (struct qaoed_acl_info *) arg;
   
   cnt = api_hdr->arg_len / sizeof(struct qaoed_acl_info);
   
   while(cnt--)
     {
	printf("acl -- %s\n",aclinfo->name);
	aclinfo++;
     }
   
   return(0);
}

int processINTLISTreply(int sock,struct apihdr *api_hdr,void *arg)
{
   int cnt; 
   struct qaoed_if_info *ifinfo = (struct qaoed_if_info *) arg;
   
   cnt = api_hdr->arg_len / sizeof(struct qaoed_if_info);
   
   printf("INTERFACE                   HWADDR      MTU\n\n");

   while(cnt--)
     {
	printf("%-10s       %02X:%02X:%02X:%02X:%02X:%02X     %d\n",
	       ifinfo->name,
	       ifinfo->hwaddr[0],
	       ifinfo->hwaddr[1],
	       ifinfo->hwaddr[2],
	       ifinfo->hwaddr[3],
	       ifinfo->hwaddr[4],
	       ifinfo->hwaddr[5],
	       ifinfo->mtu);
	ifinfo++;
     }
   
   return(0);
}

int processTARGETLISTreply(int sock,struct apihdr *api_hdr,void *arg)
{
   int cnt; 
   struct qaoed_target_info *target = (struct qaoed_target_info *) arg;
   
   cnt = api_hdr->arg_len / sizeof(struct qaoed_target_info);

   printf("NAME                                 SHELF    SLOT     INTERFACE\n\n");
   
   while(cnt--)
     {
	printf("%-35s %6d  %6d    %10s\n",
	       target->devicename,
	       target->shelf,target->slot,
	       target->ifname);
	target++;
     }
   
   return(0);
}


int processAPIreply(int sock,struct apihdr *api_hdr,void *arg)
{

  switch(api_hdr->cmd)
    {
    case API_CMD_STATUS:
      processSTATUSreply(sock,api_hdr,arg);
      break;

    case API_CMD_TARGET_STATUS:
      printf("API_CMD_TARGET_STATUS - no function \n");
      break;

    case API_CMD_TARGET_ADD:
      processTARGETADDreply(sock,api_hdr,arg);
      break;

    case API_CMD_TARGET_DEL:
       if(api_hdr->error == API_ALLOK)
	 printf("Command completed successfully\n");
       else
	 printf("Command failed\n");
       break;
       
     default:
       printf("Unknown cmd: %d\n",api_hdr->cmd);
    }

  return(0);
}

struct apihdr * readreply(int sock)
{
   struct apihdr api_hdr;
   struct apihdr *arg;
   int n;
   
   /* Read the api_hdr */
   n = recv(sock, &api_hdr, sizeof(struct apihdr),0);
   
   if (n != sizeof(struct apihdr))
     {
	if(n > 0)     
	  printf("Wrong size for api_hdr: %d\n",n);
	else
	  printf("Error recieving api_hdr: %s\n",strerror(errno));
	
	close(sock);
	exit(-1);
     }
   
   if(api_hdr.arg_len < 2048)
     arg = (struct apihdr *) malloc(api_hdr.arg_len + sizeof(struct apihdr));
   else
     arg = (struct apihdr *) malloc(sizeof(struct apihdr));
   
   if(arg == NULL)
     {
	printf("Memory allocation error: %s\n",strerror(errno));
	close(sock);
	exit(-1);
     }

   /* Copy hdr */
   memcpy(arg,&api_hdr,sizeof(struct apihdr));
   
   if(api_hdr.arg_len > 2048)
     {
	printf("argument lenght has bogus value: %d\n",
	       api_hdr.arg_len);
	close(sock);
	exit(-1);
     }

   /* Read in argument data if any */
   if(api_hdr.arg_len > 0)
     n = recv(sock, arg->data, api_hdr.arg_len,0);
   else
     n = 0;
   
   /* Make sure we got data of the right size */
   if (n != api_hdr.arg_len)
     {
	printf("Bogus lenght recieved from socket: %d\n",n);
	close(sock);
	exit(-1);
     }
   
   return(arg);
   
}

  
int recvshowreply(int sock)
{
   struct apihdr *api_hdr;
   
   /* Read reply from socket */
   api_hdr = readreply( sock);
   
   if(api_hdr == NULL)
     {
	printf("Failed to read reply from socket!\n");
	fflush(stdout);
	exit(-1);
     }
      
   if(api_hdr->error != API_ALLOK)
     {
	printf("Command failed\n");
	fflush(stdout);
	free(api_hdr);
	return(-1);
     }

   switch(api_hdr->cmd)
     {
	
      case API_CMD_INTERFACES_LIST:
	processINTLISTreply(sock,api_hdr,api_hdr->data);
	break;
	
      case API_CMD_TARGET_LIST:
	processTARGETLISTreply(sock,api_hdr,api_hdr->data);
	break;
	
      case API_CMD_ACL_LIST:
	processACLLISTreply(sock,api_hdr,api_hdr->data);
	break;
	
      default:
	printf("Unknown reply from server!\n");
	break;
     }
   
   free(api_hdr);
   return(0);
}


int recvcmdreply(int sock)
{
   struct apihdr *api_hdr;
   
   /* Read reply from socket */
   api_hdr = readreply( sock);
   
   if(api_hdr == NULL)
     {
	printf("Failed to read reply from socket!\n");
	fflush(stdout);
	exit(-1);
     }
   
   if(api_hdr->error != API_ALLOK)
     {
	printf("Command failed!\n");
	fflush(stdout);
     }
   else
     processAPIreply(sock,api_hdr,api_hdr->data);
   
   free(api_hdr);   
   return 0;
}

int testAPIsocket(int sock)
{
  struct apihdr api_hdr;
  struct qaoed_status status;
  int n;

   api_hdr.cmd = API_CMD_STATUS;  /* We want status info */
   api_hdr.type = REQUEST;  /* This is a request */
   api_hdr.error = API_ALLOK;  /* Everything is ok */
   api_hdr.arg_len = 0; /* no arguments */
   
   send(sock,&api_hdr,sizeof(api_hdr),0);

   /* Read api_hdr */
   n = recv(sock, &api_hdr, sizeof(struct apihdr),0);
   
   if(n != sizeof(struct apihdr))
     {
       printf("Status request failed\n");
       return(-1);
     }

   /* Read status */
   if(api_hdr.arg_len > 0)
     n = recv(sock, &status, sizeof(struct qaoed_status),0);

   if(api_hdr.cmd   != API_CMD_STATUS ||
      api_hdr.error != API_ALLOK ||
      api_hdr.type  != REPLY)
     {
       printf("Status request failed\n");
       return(-1);
     }

   return(0);
}

/* Check if an interface exist */
int check_interface(int sock, char *interface)
{
   struct apihdr api_hdr;
   struct qaoed_interface_cmd qif;
   struct qaoed_if_info info;

   int n;

   api_hdr.cmd = API_CMD_INTERFACES_STATUS;  /* We want status info */
   api_hdr.type = REQUEST;  /* This is a request */
   api_hdr.error = API_ALLOK;  /* Everything is ok */
   api_hdr.arg_len = sizeof(struct qaoed_interface_cmd);
   
   qif.mtu = 0;
   strncpy(qif.ifname,interface,sizeof(qif.ifname));
      
   send(sock,&api_hdr,sizeof(api_hdr),0);
   send(sock,&qif,sizeof(qif),0);
   
   /* Read api_hdr */
   n = recv(sock, &api_hdr, sizeof(struct apihdr),0);
   
   if(n != sizeof(struct apihdr))
     {
	printf("Status request failed\n");
	return(-1);
     }

   /* Read status */
   if(api_hdr.arg_len > 0)
     n = recv(sock, &info, sizeof(struct qaoed_status),0);

   if(strcmp(info.name,interface) != 0)
     {
       printf("Status request failed\n");
       return(-1);
     }

   return(0);
}


int openapi()
{
  int sock;
  struct sockaddr_un rsock;
  socklen_t sockaddr_len;

  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
     {
       fprintf(stdout,"Failed to create socket for API: socket() - %s\n",
	       strerror(errno));
       fflush(stdout);
       exit(1);
     }
      
   /* Setup struct */
   rsock.sun_family = AF_UNIX;
   strcpy(rsock.sun_path, SOCK_PATH);
   sockaddr_len = strlen(rsock.sun_path) + sizeof(rsock.sun_family) + 1;

   if (connect(sock, (struct sockaddr *)&rsock, sockaddr_len) == -1) 
     {
       fprintf(stdout,"Failed to connect to API: connect() - %s\n",
	       strerror(errno));
       fflush(stdout);
       exit(1);
     }

   if(testAPIsocket(sock) == -1)
     {
       close(sock);
       return(-1);
     }

   return(sock);

}



int add_acl(int sock,int argc, char **argv)
{
   
   
   return(0);
}


int API_adddel_target(int sock,char *cmd,char *device, int shelf, 
		      int slot, char *interface)
{
  struct apihdr api_hdr;
  struct qaoed_target_info target;
  
  /* api header */
   if(strncmp(cmd,"remove",6) == 0)
     api_hdr.cmd     = API_CMD_TARGET_DEL;  /* We want to remove a target */
   else
     api_hdr.cmd     = API_CMD_TARGET_ADD;  /* We want to add a target */

  api_hdr.type    = REQUEST;              /* This is a request */
  api_hdr.error   = API_ALLOK;            /* Everything is ok */
  api_hdr.arg_len = sizeof(struct qaoed_target_info);
  
  /* Fill out target data */
  strcpy(target.devicename,device);
  target.shelf      = shelf;
  target.slot       = slot;
  if(interface != NULL)
    strcpy(target.ifname,interface);
  else
    target.ifname[0] = 0;
   
   /* Use default values */
  target.writecache = -1;
  target.broadcast = -1;

#ifdef DEBUG
  printf("Sending %s request\n",cmd);
#endif
  
  /* Send request */
  send(sock,&api_hdr,sizeof(api_hdr),0);  
  send(sock,&target,sizeof(target),0);  

  /* Wait for reply */
  recvcmdreply(sock);
  
  return(0);
}

void target_usage(char *cmd)
{
   printf("%s target <device> [shelf] [slot] [interface]\n",cmd);
   return;
}


int adddel_target(int sock,char *cmd,int argc, char **argv)
{
   int shelf = 0xffff; /* Auto assign */
   int slot = 0xff;  /* Auto assign */
   char *p;
   
   /* This option requires at least 4 argument
    * add     <device> [shelf] [slot] [interface]
    * remove  <device> <shelf> <slot> <interface>
    * The shelf, slot and interface options are optional for add */
   
   char *interface = NULL;
   
   if(argc < 2 || argc > 5)
     {
	target_usage(cmd);
	return(0);
     }
      
   /* User specified an interface */
   if(argc > 4) 
     interface = argv[4];
   
   /* User specified a slot */
   if(argc > 3) 
     {
	/* Make sure slot is all digits */
	p = argv[3];
	while(*p)
	  if(isdigit(*p) == 0) 
	    {
	       target_usage(cmd);
	       return(0);
	    }
	else
	  p++;
	
	slot = strtol(argv[3], (char **)NULL, 10);
	if(slot < 0 || slot > 255)
	  {
	     target_usage(cmd);
	     return(0);
	  }
     }
       
   
   /* User specified a shelf */
   if(argc > 2)
     {
	/* Make sure shelf is all digits */
	p = argv[2];
	while(*p)
	  if(isdigit(*p) == 0) 
	    {
	       target_usage(cmd);
	       return(0);
	    }
	else
	  p++;
	
	shelf = strtol(argv[2], (char **)NULL, 10);
	if(shelf < 0 || shelf > 65535)
	  {
	     target_usage(cmd);
	     return(0);
	  }
     }
          
   
   /* Add the requested target */
   API_adddel_target(sock,cmd,argv[1],shelf,slot,interface);
   
   return(0);
}

int list(int sock, int cmd)
{
  struct apihdr api_hdr;

  /* api header */
  api_hdr.cmd     = cmd;                  /* cmd to send */
  api_hdr.type    = REQUEST;              /* This is a request */
  api_hdr.error   = API_ALLOK;            /* Everything is ok */
  api_hdr.arg_len = 0;

   /* Send request */
   send(sock,&api_hdr,sizeof(api_hdr),0);
   
   /* Wait for reply */
   recvshowreply(sock);
   
   return(0);
}

void usage()
{
   printf("Usage: { add | remove | show | set } \n");
}

void set_usage()
{
   printf("Usage: set { interface } \n");
}

void set_interface_usage()
{
   printf("Usage: set interface { mtu | ... } \n");
}

void set_interface_mtu_usage()
{
   printf("Usage: set interface mtu { auto | <mtu size> } \n");
}

void show_usage()
{
   printf("Usage: show { status | targets | interfaces | access-lists } \n");
}

void add_usage()
{
   printf("Usage: add { target | interface | access-list } \n");
}

void del_usage()
{
   printf("Usage: remove { target | interface | access-list } \n");
}

int set_interface_mtu(int sock, int argc, char **argv)
{
  struct apihdr api_hdr;
  struct qaoed_interface_cmd ifcmd;
   
   char *interface;
   char *mtu;
   int i_mtu = 0;
   
   /* Make sure we got an argument of some kind */
   if(argc < 2)
     {
	set_interface_mtu_usage();
	return(-1);
     }

   interface = argv[1];
   mtu = argv[2];
   
   /* check if interface exists */
   if(check_interface(sock,interface) != 0)
     {
	printf("No such interface configured: %s\n",interface);
	return(-1);
     }
   
	
   if(mtu[0] == 'a') /* Auto */
     i_mtu = -1;
   else
     {
	i_mtu = strtol(mtu, (char **)NULL, 10);
	
	if(i_mtu <= 0 || i_mtu > 10000)
	  {
	     printf("Invalid MTU value: %d\n",i_mtu);
	     return(-1);
	  }
     }
   
   /* configure request */
   api_hdr.cmd = API_CMD_INTERFACES_SETMTU;  /* Set mtu of interface  */
   api_hdr.type = REQUEST;  /* This is a request */
   api_hdr.error = API_ALLOK;  /* Everything is ok */
   api_hdr.arg_len = sizeof(struct qaoed_interface_cmd);
   
   /* Name of interface */
   strncpy(ifcmd.ifname,interface,sizeof(ifcmd.ifname));
   
   /* New mtu value */
   ifcmd.mtu = i_mtu;
   
   /* Send command */
   send(sock,&api_hdr,sizeof(api_hdr),0);
   send(sock,&ifcmd,sizeof(ifcmd),0);
   
   return(0);
}
  
void set_interface(int sock, int argc, char **argv)
{
   
   /* Make sure we got an argument of some kind */
   if(argc < 2)
     {
	set_interface_usage();
	return;
     }

   /* del interface */
   if(strncmp(argv[1],"mtu",3) == 0)
     {
	set_interface_mtu(sock,(argc - 1),argv+1);
	return;
     }	 
   
   set_interface_usage();
   return;   
}

void set(int sock, int argc, char **argv)
{
   
   /* Make sure we got an argument of some kind */
   if(argc < 2)
     {
	set_usage();
	return;
     }

      /* del interface */
   if(strncmp(argv[1],"int",3) == 0)
     {
	set_interface(sock,(argc - 1),argv+1);
	return;
     }	 
   
   set_usage();
   return;   
}

void del(int sock, int argc, char **argv)
{
   
   /* Make sure we got an argument of some kind */
   if(argc < 2)
     {
	del_usage();
	return;
     }
   
   /* del target */
   if(strncmp(argv[1],"tar",3) == 0)
     {
	adddel_target(sock,"remove", (argc - 1),argv+1);
	return;
     }	 
   
   /* del interface */
   if(strncmp(argv[1],"int",3) == 0)
     {
	
	return;
     }	 
   
   /* del access-list */
   if((strncmp(argv[1],"acl",3) == 0) ||
      (strncmp(argv[1],"acc",3) == 0))
     {
  //	del_acl(sock,(argc - 1),argv+1);
	return;
     }	 

   del_usage();
   return;
}

void add(int sock, int argc, char **argv)
{
   /* Make sure we got an argument of some kind */
   if(argc < 2)
     {
	add_usage();
	return;
     }
   
   /* Add target */
   if(strncmp(argv[1],"tar",3) == 0)
     {
	adddel_target(sock,"add", (argc - 1),argv+1);
	return;
     }	 
   
   /* add interface */
   if(strncmp(argv[1],"int",3) == 0)
     {
	
	return;
     }	 
   
   /* add access-list */
   if(strncmp(argv[1],"acl",3) == 0)
     {
	add_acl(sock,(argc - 1),argv+1);
	return;
     }	 

   /* add access-list */
   if(strncmp(argv[1],"acc",3) == 0)
     {
	add_acl(sock,(argc - 1),argv+1);
	return;
     }	 
   
   /* Ehh */
   add_usage();
   return;
   
}

void status(int sock)
{

  struct apihdr api_hdr;
  struct apihdr *reply;

  /* api header */
  api_hdr.cmd     = API_CMD_STATUS;       /* Request qaoed status */
  api_hdr.type    = REQUEST;              /* This is a request */
  api_hdr.error   = API_ALLOK;            /* Everything is ok */
  api_hdr.arg_len = 0;

   /* Send request */
   send(sock,&api_hdr,sizeof(api_hdr),0);
   
   /* Read reply from socket */
   reply = readreply(sock);
   
   if(reply != NULL)
     {
       processSTATUSreply(sock,reply,reply->data);
       free(reply);
     }
   
   return;
}

void show(int sock, int argc, char **argv)
{
   
   /* Make sure we got an argument of some kind */
   if(argc < 2)
     {
	show_usage();
	return;
     }
   
   /* Show qaoed status */
   if(strncmp(argv[1],"sta",3) == 0)
     {
       status(sock);
       return;
     }	 

   /* List targets */
   if(strncmp(argv[1],"tar",3) == 0)
     {
	list(sock,API_CMD_TARGET_LIST);
	return;
     }	 
   
   /* List interfaces */
   if(strncmp(argv[1],"int",3) == 0)
     {
	list(sock,API_CMD_INTERFACES_LIST);
	return;
     }	 
   
   /* List access-lists */
   if((strncmp(argv[1],"acl",3) == 0) ||
      (strncmp(argv[1],"acc",3) == 0))
     {
	list(sock,API_CMD_ACL_LIST);
	return;
     }	 
   
   /* Ehh.. ? */
   show_usage();
   return;   
   
}


int main(int argc, char **argv)
{
   int sock;
   
   if(argc < 2)
     {
	usage();
	exit(-1);
     }
   
   sock = openapi();
      
   if(strncmp(argv[1],"add",3) == 0)
      {
	 add(sock,(argc - 1),argv+1);
	 return(0);
      }
      
      if(strncmp(argv[1],"remove",3) == 0)
      {
	 del(sock, (argc - 1),argv+1);
	 return(0);
      }
   
   if(strncmp(argv[1],"set",3) == 0)
      {
	 set(sock, (argc - 1),argv+1);
	 return(0);
      }
        
      if(strncmp(argv[1],"show",4) == 0)
      {
	 show(sock,(argc - 1),argv+1);
	 return(0);
      }	 
   
      usage();
      close(sock);
      return(0);
}
