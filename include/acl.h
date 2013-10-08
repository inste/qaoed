
#define ETH_ALEN 6

struct aclentry {
  unsigned char rule;
  unsigned char h_dest[ETH_ALEN];
  unsigned char mask;
  struct aclentry *next;
};

struct aclhdr {
  char *name;        /* Name of this access-list */
  int aclnum;        /* Uniq number identifying this access-list */
  pthread_mutex_t lock; /* Mutex lock for this acl */
  int refcnt;
  unsigned char defaultpolicy;
  struct aclentry *acl;
  struct logging *log;
  struct aclhdr *next;
};

#define ACL_ERROR 255
#define ACL_ACCEPT 1
#define ACL_REJECT 2

struct aclhdr * findACL(struct aclhdr *acllist, struct cfg *c,
			struct qconfig *conf);
struct aclhdr *referenceacl(char *name, struct qconfig *conf);
int aclmatch(struct aclhdr *acl, unsigned char *h_addr);
void aclrefup(struct aclhdr *acl);
void aclrefdown(struct aclhdr *acl);
    
