#include <stdarg.h>

#define LOGTYPE_ERR -1
#define LOGTYPE_SYSLOG 0
#define LOGTYPE_FILE 1

struct logging
{
   char *name; /* Name of this logging target */
   int log_level; /* Log-level for this entity */

   int logtype; /* Type of function (file, syslog, ...) */

	/* If type == syslog */
	int syslog_facility;
	int syslog_level;
	
	/* if type == file */
	char *filename;
        FILE *fp;
         
   struct logging *next; /* A pointer to the next logging target */
};


int loglvresolv(char *str);
int syslgresolv(char *str);

int logstart(struct logging *log);
int logfunc(struct logging *log, int level, const char *format, ...);
int vlogfunc(struct logging *log, int level, const char *format, va_list args);
  

struct logging *referencelog(char *name, struct qconfig *conf);
