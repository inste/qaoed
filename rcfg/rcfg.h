struct tok {
  char **tokens; /* Array of config tokens from the file */
  int tokidx;    /* The number of tokens in the array */
  int tokmax;    /* used to count the number of free slots */
};


#define ASSIGNMENT 0
#define BLOCK 1

struct cfg {
  int type; /* block or assignment */
  char *lvalue; /* allways filled out */
  char *rvalue; /* filled if type == ASSIGNMENT */
  struct cfg *block; /* filled if type == BLOCK */
  struct cfg *next; /* Pointer to the next cfg-struct */
};


/* If you want error logging from the rcfg-framework to go anywhere other
 * then to stderr, fillout this struct */
struct cfgerror 
{
   void (*logfunc)(struct cfgerror *, char *fmt, ...);
   void *priv;
};

struct cfg *readconfig(char *filename, struct cfgerror *);
int printcfgunclaimed(struct cfg *c);
int printcfg(int level, struct cfg *c);

