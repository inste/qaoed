#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>

#include "include/qaoed.h"
#include "include/hdreg.h"
#include "include/logging.h"

/* #define DEBUG 1 */

struct qconfig *curcfg = NULL;

/* These are all the avaiable command line options, so far only two :) */
struct cmdline {
   int background;
   char *cfgfile;
}options;

int qaoed_startup(struct qconfig *conf)
{

#ifdef DEBUG
  printf("Starting devices!\n");
  fflush(stdout);
#endif

   /* Start devices */
   if(qaoed_devices(conf) == -1)
     return(-1); 

#ifdef DEBUG
  printf("Starting network listeners!\n");
  fflush(stdout);
#endif

   /* Start the network listeners */
   if(qaoed_network(conf) == -1)
     return(-1); 
   
   /* Start the API */
   qaoed_startapi(conf); 

   return(0);
}



void qaoed_shutdown(struct qconfig *conf)
{
   struct aoedev *device ;
   struct ifst *ifent;
   
   /* shutdown the network threads */
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     if(pthread_cancel(ifent->threadID) != 0)
       {      
	  logfunc(conf->log,LOG_ERR,"Failed to stop network thread for %s\n",
		  ifent->ifname);
	  exit(-1);
       }
   
   /* Wait for all network threads to finish */
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     pthread_join(ifent->threadID,NULL);
   
   /* Shutdown the devices */
   for(device = conf->devices; device != NULL; device = device->next)
     if(pthread_cancel(device->threadID) != 0)
       logfunc(conf->log,LOG_ERR,"Failed to stop device thread for %s\n",
	       device->devicename);
   
   /* Wait for all device threads to finish */
   for(ifent = conf->intlist; ifent != NULL; ifent = ifent->next)
     pthread_join(ifent->threadID,NULL);
}


void sighuphandler(int sig)
{
   struct qconfig *conf;
   
   /* We dont want to be interrupted again do we ? */
   signal(SIGHUP,SIG_IGN);
   
   if(curcfg != NULL && curcfg->log !=NULL)
     {  
	logfunc(curcfg->log,LOG_ERR,"Reloading configuration\n"); 
     }
   else
     fprintf(stderr,"Reloading configuration\n");
   
   /* load config from file */
   conf = qaoed_loadconfig(options.cfgfile);
   
   if(conf == NULL)
     {  
	fprintf(stderr,"Loading configuration failed!\n");
	logfunc(curcfg->log,LOG_ERR,"Loading configuration failed!\n");
	return;
     }
   
   /* Shutdown the old worker threads */
   qaoed_shutdown(curcfg);
   
   /* Start the new config */
   if(qaoed_startup(conf) == 0)
     {
	
	logfunc(curcfg->log,LOG_ERR,"New configuration successfully loaded!\n");
	logfunc(conf->log,LOG_ERR,"New configuration successfully loaded!\n");
	
	/* Destroy the old config */
	destroyqcfg(curcfg);
	
	/* Switch running config */
	curcfg = conf;
	
     }
   else
     {
	/* Something went wrong while trying to start the new
	 * configuration. Revoke the change and continue running
	 * the old config */
	
	logfunc(curcfg->log,LOG_ERR,"New configuration failed to load!\n");
	logfunc(conf->log,LOG_ERR,"New configuration failed to load!\n");
	
	/* Stop anything that managed to start */
	qaoed_shutdown(conf);
	
	/* Destroy the old config */
	destroyqcfg(conf);
	
	/* Restart the old configuration */
	if(qaoed_startup(curcfg) == 0)
	  logfunc(conf->log,LOG_ERR,"Revert to old configuration successfull!\n");
	else
	  {
	     /* Complete and utter failure :( */
	     /* The old configuration also failed to start... */
	     /* We dont have mutch of an option anymore, we have
	      * to continue with what we got ... */
	     
	     logfunc(curcfg->log,LOG_ERR,"Failed to revert to original configuration !\n");
	  }
     }
   
   /* Reregister signal handler for sighup */
   signal(SIGHUP,sighuphandler);
   
}

  
int qaoed_main()
{
   struct qconfig *conf;

   /* load config from file */
   conf = qaoed_loadconfig(options.cfgfile);

   if(conf == NULL)
     {
	printf("Errors while loading configuration... \n");
	exit(-1);
     }

   /* go into background if the user wants us to. We have to do this
    * before we start the threads or stuff gets really unpredictable */
   if(options.background == 1)
     if(fork()>0)
       exit(0);

   /* Startup the worker threads */
   qaoed_startup(conf);
   
   /* Update global curcfg pointer used by signalhandler for sighup */
   curcfg = conf;

   /* Register signal handler for sighup */
   signal(SIGHUP,sighuphandler);
         
   /* Our worker threads are running, we are in the background */
   while(1)
     sleep(100);
   
}

void usage(char *name)
{
   printf("usage: %s [-f -V -h] [-c <file>]\n",name);
}

int main(int argc, char **argv)
{
  int i;

#ifdef DEBUG
  printf("main!\n");
  fflush(stdout);
#endif

   options.cfgfile = strdup(CFGFILE); /* Default configuration file */
   options.background = 1; /* Daemonize */

#ifdef DEBUG
  printf("Parsing command line!\n");
  fflush(stdout);
#endif
   
   /* Parse the command line arguments */
   while ((i=getopt(argc,argv,"fhVc:"))>-1)
     switch(i)
       {      
	case 'f': /* Run in the foreground */
	  options.background = 0;
	  break;
   
	case 'c': /* Config file */
           options.cfgfile = (char *)strdup(optarg);
           break;
	  
	case 'h':
	  usage(argv[0]);
	  exit(-1);
   
	case 'V':
	  printf("qaoed version .. 0.beta.something.something\n");
	  exit(-1);
	  
	default:
	  printf("Unknown argument %c\n",i);
	  usage(argv[0]);
	  exit(-1);
       }	  
   
   qaoed_main();
   
   return(0);
}
