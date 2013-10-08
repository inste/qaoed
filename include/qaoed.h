#include <pthread.h>
#include "aoe.h"
#include "../rcfg/rcfg.h"

#define CFGFILE "/etc/qaoed.conf"

#define  ETH_P_AOE 0x88a2

/* The different types of work requests */
#define WORKTYPE_PACKET 1  /* Work request contains a packet */


/* Status of device/interface threads */
#define PTSTATUS_IDLE 0
#define PTSTATUS_RUNNING 1

struct threadargs{
  struct qconfig *conf;
  struct ifst *ifentry;
};

struct pkt 
{
  void *raw; /* The real start of the packet, used for free and malloc */
  int refcount; /* Used to indicate if this buffer is still needed */
  pthread_mutex_t pktlock; /* Lock for this buffer, only used to atomic
                            * access to the recounter  */
};

struct workentry
{
   unsigned char type;
   struct pkt *rawbuf;
   struct aoe_hdr *aoepkt;
   int packet_len;
   struct ifst *ifentry; /* Interface that the packet came in on */
   pthread_mutex_t worklock; /* Lock for this workentry */
   int refcount; /* Refcount is used to indicate if this 
		  * workentry can be destroyed or not */
};

struct queueentry 
{
   struct workentry *work;
   struct queueentry *next;
};


/* Used to describe an interface used for communication */
struct ifst
{
   char *ifname;           /* Name of interface */
   unsigned char status;   /* Status of interface thread */
   pthread_t threadID;     /* Thread id of thread handling this interface */
   unsigned short ifindex; /* Interface index number (if any) */
   int sock;               /* Outgoing socket for reply */
   char *hwaddr;           /* src-address to use when sending */
   int mtu;                /* the Maximum Transfer Unit (MTU) of this int... */
   int buffsize;           /* Buffer size to use when reading from socket */
   struct logging *log;    /* Pointer to the logging target for this int... */
   int log_level;          /* The log-level of this interface */
   int refcnt;             /* Reference counter */
   pthread_mutex_t iflock; /* Lock used when modifying this interface struct */
   struct ifst *next;
   struct ifst *prev;
};



/* List of devices currently avaiable, this list can be used both 
 * to find the right queue for a device and during startup to 
 * start the worker threads. */
struct aoedev 
{
   char *devicename;       /* Point to file/device name */
   int fd;                 /* file descriptor of open file */
   FILE *fp;               /* File pointer to open file */
   off_t size;             /* size in 512byte blocks */
   unsigned short shelf;   /* 16 bit */
   unsigned char slot;     /*  8 bit */
   struct ifst *interface; /* interface */
   unsigned char status;   /* Status of device thread */
   pthread_t threadID;     /* Thread id of thread handling this device */
   int writecache;         /* Writecache on or off */
   int broadcast;          /* Broadcast on or off */
   
   struct aclhdr *wacl;       /* Access list used for write operations    */
   struct aclhdr *racl;       /* Access list used for read operations     */
   struct aclhdr *cfgsetacl;  /* Access list used for cfg set             */
   struct aclhdr *cfgracl;    /* Access list used for cfg read / discover */
   
   pthread_cond_t qcv;        /* Tell thread something is in the queue */
   pthread_mutex_t queuelock; /* Lock used to access this queue */
   struct queueentry *q;           /* The first for this device */
   struct queueentry *qstart;      /* The first entry  of this queue */
   struct queueentry *qend;        /* The last entry  of this queue  */
   struct logging *log;            /* Logging */
   int log_level;             /* Loglevel for this device */
   int cfg_len;               /* The cfg-data for this device */
   char cfg[1024];            /* The cfg-data for this device */
   struct aoedev *next;       /* Pointer to the next device */
   struct aoedev *prev;       /* Pointer to the previus device */
};


struct qconfig 
{
   struct logging *log;        /* Logtarget to use by default */
   int log_level;              /* Loglevel for the qaoe daemon */
   int syslog_facility;        
   int syslog_level;
   
   pthread_rwlock_t intlistlock; /* RW lock used to access the interface-list */
   struct ifst   *intlist;      /* list of interfaces */
   struct aoedev *devdefault;  /* default values for devices */
   pthread_rwlock_t devlistlock; /* RW lock used to access the device-list */
   struct aoedev *devices;     /* Linked list with devices */
   struct aclhdr *acllist;     /* Linked list with access-lists */
   struct logging *loglist;    /* linked list with logging targets */
   
   int aclnumplan;             /* Used to give unique numbers to access-lists */
   
   char *sockpath;            /* Path to unix socket for the API interface */
   pthread_t APIthreadID;     /* ThreadID of thread for the API interface */
};

struct qconfig *qaoed_loadconfig();
int qaoed_devices(struct qconfig *conf);
int qaoed_network(struct qconfig *conf);
void qaoed_workdestroy(struct workentry *work);
int qaoed_xmit(struct workentry *work, void *pkt, int pkt_len);
  
struct aoedev * findDEV(struct aoedev *devlist, struct cfg *c, 
			struct qconfig *conf);
struct aoedev * findDFLT(struct aoedev *devlist, struct cfg *c,
		         struct qconfig *conf);

int qaoed_opensock(struct ifst *ifentry);
int getifhwaddr(struct ifst *ifentry);
int getifmtu(struct ifst *ifentry);
struct ifst *referenceint(char *name, struct qconfig *conf);
void destroyqcfg(struct qconfig *conf);
int qaoed_listener(struct threadargs *args);
int qaoed_pktdestroy(struct pkt *packet);
void processpacket(struct qconfig *conf, struct ifst *ifentry, 
		   struct pkt *Packet, struct aoe_hdr *aoepkt, int len);
int arch_getsize(int fd, void *mediasize);
int qaoed_startapi(struct qconfig *conf);
void qaoed_devdefaults(struct qconfig *conf,struct aoedev *device);
int qaoed_startdevice(struct aoedev *device);
void qaoed_intrefup(struct ifst *interface);
void qaoed_intrefdown(struct ifst *interface);
int qaoed_startlistener(struct qconfig *conf, struct ifst *ifent);
