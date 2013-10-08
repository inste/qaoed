#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>

#include <stdarg.h>

#include "include/qaoed.h"
#include "include/logging.h"


struct level {
  char    *str;
  int     lvl;
};

struct level loglevel[] = {
  { "error",      4 },
  { "info",       3 },
  { "debg",       2 },
  { "emerg",      1 },
  { "none",       0 }
};

struct level sysloglevel[] = {
  { "LOG_EMERG",      LOG_EMERG   },
  { "LOG_ALERT",      LOG_ALERT   },
  { "LOG_CRIT",       LOG_CRIT    },
  { "LOG_ERROR",      LOG_ERR     },
  { "LOG_WARNING",    LOG_WARNING },
  { "LOG_NOTICE",     LOG_NOTICE  },
  { "LOG_INFO",       LOG_INFO    },
  { "LOG_DEBUG",      LOG_DEBUG   },
  { "LOG_AUTH",       LOG_AUTH    },
  { "LOG_DAEMON",     LOG_DAEMON  },
  { "LOG_USER",       LOG_USER    },
  { "LOG_LOCAL0",     LOG_LOCAL0  },
  { "LOG_LOCAL1",     LOG_LOCAL1  },
  { "LOG_LOCAL2",     LOG_LOCAL2  },
  { "LOG_LOCAL3",     LOG_LOCAL3  },
  { "LOG_LOCAL4",     LOG_LOCAL4  },
  { "LOG_LOCAL5",     LOG_LOCAL5  },
  { "LOG_LOCAL6",     LOG_LOCAL6  },
  { "LOG_LOCAL7",     LOG_LOCAL7  }
};

/* Resolv loglevel from string to integer */
int loglvresolv(char *str)
{
  int i;
  for( i = 0; i < (sizeof(loglevel)/sizeof(struct level)); i++)
    if(strcasecmp(str,loglevel[i].str) == 0)
      return(loglevel[i].lvl);

   printf("Unknown loglevel: %s\n",str);
   return(-1);
}


/* Resolv syslog facility and level from char * to int */
int syslgresolv(char *str)
{
  
  int i;
  for( i = 0; i < (sizeof(sysloglevel)/sizeof(struct level)); i++)
    if(strcasecmp(str,sysloglevel[i].str) == 0)
      return(sysloglevel[i].lvl);

  printf("Unknown syslog-level: %s\n",str);
  return(-1);

}

/* Open syslog for logging */
int logstart_syslog(struct logging *log)
{
   openlog("qaoed",LOG_NDELAY,log->syslog_facility);
   return(0);
}

/* Open file for logging */
int logstart_file(struct logging *log)
{
   if(log == NULL)
     {
	fprintf(stderr,"logstart_file() called with NULL argument\n");
	return(-1);
     }	
   
   log->fp = fopen(log->filename,"a");
   
   if(log->fp == NULL)
     {
	fprintf(stderr,"Failed to open logfile: %s: %s\n",
		log->filename,(char *) strerror(errno));
	log->logtype = LOGTYPE_SYSLOG; /* Switch logging target */
	return(-1);
     }

   fprintf(log->fp,"Log starting for target %s\n",log->name);
   
   return(0);
}


/* Call the correct logstart function */
int logstart(struct logging *log)
{
   if(log == NULL)
     {
	fprintf(stderr,"logstart() called with NULL argument\n");
	return(-1);
     }	
   
   switch(log->logtype)
     {
      case LOGTYPE_SYSLOG:
	return(logstart_syslog(log));
	break;
	
      case LOGTYPE_FILE:
	return(logstart_file(log));
	break;
	
      default:
	fprintf(stderr,"logstart()- Unknown logtype: %d\n",
		log->logtype);
	return(-1);
     }
   
   /* Ehh.. que ? .. foobar! */
   return(-1);
}

/* I really have to give credit to the guys that came up with 
 * vsyslog() and vfprintf(), this really wouldnt be possible 
 * without those functions :) */


int logfunc_syslog(struct logging *log, const char *format, va_list args )
{
   vsyslog(log->syslog_level|log->syslog_facility,format,args);
   return(0);
}

int logfunc_file(struct logging *log, const char *format, va_list args )
{
   if(vfprintf(log->fp,format,args) == -1)
     {
	/* We failed to log to the logfile, send the data to syslog and 
	 * also write it to stderr */
	syslog(LOG_ERR,"Failed to write to file %s for logging target %s: %s\n",
		log->filename,log->name,strerror(errno));
	vsyslog(LOG_ERR,format,args);
	fprintf(stderr,"Failed to write to file %s for logging target %s: %s\n",
		log->filename,log->name,strerror(errno));
	vfprintf(stderr,format,args);
     }    

   return(0);
}


/* Dispatch the log-message to the correct logging function */
int vlogfunc(struct logging *log, int level, const char *format, va_list args) 
{
   
   if(log == NULL)
     {
	/* We dont have any logging target :( */
	/* Send the output to syslog and stderr */
	vfprintf(stderr,format,args);
	vsyslog(LOG_ERR,format,args);
	return(-1);
     }
   
   
   switch(log->logtype)
     {
      case LOGTYPE_SYSLOG:
	return(logfunc_syslog(log,format,args));
	break;
	
      case LOGTYPE_FILE:
	return(logfunc_file(log,format,args));
	break;
	
      default:
	fprintf(stderr,"qaoed: Unknown logtype %d\n",log->logtype);
	syslog(LOG_ERR,"qaoed: Unknown logtype %d\n",log->logtype);
	vfprintf(stderr,format,args);
	return(-1);
     }
   
   /* Something went horribly wrong */
   return(-1);
   
}

/* Break out va_list */
int logfunc(struct logging *log, int level,  const char *format, ...)
{
   va_list args;
   va_start( args, format );

   return(vlogfunc(log,level,format,args));
}
