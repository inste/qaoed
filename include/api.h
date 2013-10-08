
/* API command codes */
enum {
     API_CMD_STATUS,
     API_CMD_INTERFACES_LIST,
     API_CMD_INTERFACES_STATUS,
     API_CMD_INTERFACES_ADD,
     API_CMD_INTERFACES_DEL,
     API_CMD_INTERFACES_SETMTU,
     API_CMD_TARGET_LIST,
     API_CMD_TARGET_STATUS,
     API_CMD_TARGET_ADD,
     API_CMD_TARGET_DEL,
     API_CMD_TARGET_SETOPTION,
     API_CMD_TARGET_SETACL,
     API_CMD_ACL_LIST, /* List access-lists */
     API_CMD_ACL_INFO, /* List entries in an access-list */
     API_CMD_ACL_STATUS, /* Show counters for an access-list */
     API_CMD_ACL_ADD,    /* Add an access-list */
     API_CMD_ACL_ADDENTRY, /* Add an entry from an access-list */
     API_CMD_ACL_DEL, /* Delete an access-list */
     API_CMD_ACL_DELENTRY, /* Remove an entry from an access-list */
     API_CMD_ACL_SETDFLT /* Set the default policy for an access-list */
};

enum {
  REQUEST,
  REPLY
};

enum {
  API_ALLOK,
  API_UKNOWNCMD,
  API_FAILURE
};

/* Header used in all requests and replies */
struct apihdr 
{
   unsigned char cmd; /* Command */
   unsigned char type; /* Request == 0, Reply == 1; */
   unsigned char error; /* OK == 0 */
   unsigned int  arg_len; /* Lenght of additional argument buffer */
   char          data[0];  /* Argument */
};

/* qaoed status struct, describes current status of qaoed */
struct qaoed_status 
{
   unsigned int num_targets; /* Number of targets */
   unsigned int num_interfaces; /* Number of interfaces */
   unsigned int num_threads; /* Number of running threads */
};

/* qaoed status struct, describes current status of qaoed */
struct qaoed_interfaces
{
   unsigned int num_interfaces; /* Number of targets */
   unsigned int refcounter; /* Number of users of this interfaces */
};


/* Used to adding / removing interfaces and setting MTU*/
struct qaoed_interface_cmd
{
   char ifname[20];        /* Name of interface */
   int mtu;                /* the Maximum Transfer Unit (MTU) of this int... */
};

/* Used to request status and to add / remove targets */
struct qaoed_target_info
{
   char devicename[120];       /*  file/device name */
   unsigned short shelf;   /* 16 bit */
   unsigned char slot;     /*  8 bit */
   char ifname[20];        /* Name of interface */
   int writecache;         /* Writecache on or off */
   int broadcast;          /* Broadcast on or off */
     
};

struct qaoed_target_info_detailed
{
  /* Basic info */
  struct qaoed_target_info info;
   
  /* Access-lists */
  char wacl[20];             /* Access list used for write operations    */
  char racl[20];             /* Access list used for read operations     */
  char cfgsetacl[20];        /* Access list used for cfg set             */
  char cfgracl[20];          /* Access list used for cfg read / discover */

  /* Accounting data */
  /* ... */

  /* Logging info  */
  char log[20];              /* Logging target */
  int log_level;             /* Loglevel for this device */

  /* Cfg */
  int cfg_len;               /* The cfg-data for this device */
  char cfg[1024];            /* The cfg-data for this device */

};

/* Used to list and to add / remove access-lists */
struct qaoed_acl_info
{
   char name[120];       /* name of the access-list*/
   int aclnumber;        /* Reference number */
   int defaultpolicy;    /* Default policy for this access-list */
};

/* Use to list / add / remove rows from access-lists */
struct qaoed_acl_entry 
{
  unsigned char rule;
  unsigned char h_dest[6];
};

/* Used to list and to add / remove interfaces*/
struct qaoed_if_info
{
   char name[20];        /* name of the interface */
   char hwaddr[6];        /* src-address to use when sending */
   int mtu;               /* the Maximum Transfer Unit (MTU) of this int... */
};
