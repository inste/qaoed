#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glob.h>

#include "rcfg.h"
#include "rcfgkw.h"

/* #define DEBUG 1 */

/* Verify the datatype of blocks and assignments */
int checkdatatype(struct tok *tokens, struct cfgerror *er)
{
  int i,t;
  char junkbuff[200];

  /* Check the datatype of each keyword */
  for(i = 0; i < tokens->tokidx ; i++)
     {
	
    for(t = 0; t < DMAX ; t += 2)
      if(strcmp(tokens->tokens[i],datatype[t]) == 0)
	{
	  /* Handle block statements */
	  if(datatype[t+1][0] == 'b' && i < tokens->tokidx)
	    if(tokens->tokens[i + 1][0] != '{')
	      {
		 if(er != NULL)
		   er->logfunc(er,
			       "%s settings should be enclosed in curly brackets {}\n",
			       tokens->tokens[i]);
		 else
		   fprintf(stderr,
			   "%s settings should be enclosed in curly brackets {}\n",
			   tokens->tokens[i]);
		 exit(-1);
	      }
	  
	  /* Handle assignments */
	  if((datatype[t+1][0] == '"' || datatype[t+1][0] == '%') && 
	     (i < tokens->tokidx))
	  {	     
	    if(!(((i + 1) < tokens->tokidx) && 
	       tokens->tokens[i + 1][0] == '='))
	      {
		 if(er != NULL)
		   er->logfunc(er,
			       " \'=\' expected after keyword \'%s\'\n",
			       tokens->tokens[i]);
		 else
		   fprintf(stderr," \'=\' expected after keyword \'%s\'\n",
			   tokens->tokens[i]);
		 exit(-1);
	      }

	    if((i + 2) >= tokens->tokidx)
	      {
		 if(er != NULL)
		   {
		      er->logfunc(er,"Error in assignment: %s %s <missing>\n",
				  tokens->tokens[i],
				  tokens->tokens[i+1]);
		      er->logfunc(er,"missing right value for %s\n",
				  tokens->tokens[i]);
		   }
		 else
		   {
		      
		      fprintf(stderr,"Error in assignment: %s %s <missing>\n",
			      tokens->tokens[i],
			      tokens->tokens[i+1]);
		      fprintf(stderr,"missing right value for %s\n",
			      tokens->tokens[i]);
		   }
		exit(-1);
	      }
	    
	    if(sscanf(tokens->tokens[i+2],datatype[t+1],junkbuff) == 0)
	      {	
		 
		 if(er != NULL)
		   {
		      er->logfunc(er,"Error in assignment: %s %s %s\n",
				  tokens->tokens[i],
				  tokens->tokens[i+1],
				  tokens->tokens[i+2]);
		      er->logfunc(er,"Wrong right value for \"%s\", " 
				  "expecting %s got \"%s\"\n",
				  tokens->tokens[i],
				  datatype[t+1],
				  tokens->tokens[i+2]);
		   }
		 else
		   {
		      fprintf(stderr,"Error in assignment: %s %s %s\n",
			      tokens->tokens[i],
			      tokens->tokens[i+1],
			      tokens->tokens[i+2]);
		      fprintf(stderr,"Wrong right value for \"%s\", " 
			      "expecting %s got \"%s\"\n",
			      tokens->tokens[i],
			      datatype[t+1],
			      tokens->tokens[i+2]);
		   }
		 
		exit(-1);
	      }

	    /* All assignments consist of 3 tokens */
	    /* lvalue equals rvalue  */
	    /* We make sure all assignments end with a semicolon */
	    if(!((i + 3) < tokens->tokidx &&
		 tokens->tokens[i + 3][0] == ';'))
	      {
		 if(er != NULL)
		   er->logfunc(er,"Missing ; after assigment %s = %s\n",
			       tokens->tokens[i],tokens->tokens[i+2]);
		 else
		   fprintf(stderr,"Missing ; after assigment %s = %s\n",
			   tokens->tokens[i],tokens->tokens[i+2]);
		 exit(-1);
	      }
	  }
	}
	
	/* We dont want to parse the rvalue of assignments */
	if(tokens->tokens[i][0] == '=')
	  i++;
     }
   
   
  return(0);
}

/* Verify that all arguments are valid */
int checkargs(struct tok *tokens, struct cfgerror *er)
{
  int i,t;

  /* Check the allowed arguments for each keyword that has a 
   * list of allowed arguments */
  for(i = 0; i < tokens->tokidx ; i++)
    for(t = 0; t < AMAX ; t += 2)
      if(strcmp(tokens->tokens[i],arglist[t]) == 0)
	if(strstr(arglist[t+1],tokens->tokens[i+2]) == NULL)
	  {
	     if(er != NULL)
	       er->logfunc(er,"\"%s\" not an allowed argument for %s (expecting %s)\n",
			   tokens->tokens[i+2],
			   tokens->tokens[i],
			   arglist[t+1]);
	     else
	       fprintf(stderr,"\"%s\" not an allowed argument for %s (expecting %s)\n",
		       tokens->tokens[i+2],
		       tokens->tokens[i],
		       arglist[t+1]);
	    return(-1);
	  }

  return(0);
}



/* Verify the keywords in the tokenlist */
int checktokenlist(struct tok *tokens, struct cfgerror *er)
{
  int i,t;
  
  /* Check for illegal keywords */
  for(i = 0; i < tokens->tokidx ; i++)
    {
      for(t = 0; t < KMAX ; t++)
	if(strcmp(tokens->tokens[i],keywords[t]) == 0)
	  break;

      
      if(t == KMAX)
	if(!(i > 0 && 
	     tokens->tokens[i-1][0] == '='))
	   {
	      if(er != NULL)
		er->logfunc(er,"Invalid keyword: %s\n",tokens->tokens[i]);
	      else
		fprintf(stderr,"Invalid keyword: %s\n",tokens->tokens[i]);
	      return(-1);
	   }
    }
  return(0);
}


/* Balance the number of curly braces in the config */
int countbraces(struct tok *tokens, struct cfgerror *er)
{
  int i;
  int left = 0;
  int right = 0;

  /* count curly braces */
  for(i = 0; i < tokens->tokidx ; i++)
    {
      if(strcmp(tokens->tokens[i],"{") == 0)
	right++;
      if(strcmp(tokens->tokens[i],"}") == 0)
	left++;
    }

  if( left < right)
    {
       if(er != NULL)
	 er->logfunc(er,"Config is missing }\n");
       else
	 fprintf(stderr,"Config is missing }\n");
      return(-1);
    }

  if( left > right)
    {
       if(er != NULL)
	 er->logfunc(er,"Config is missing {\n");
       else
	 fprintf(stderr,"Config is missing {\n");
       return(-1);
    }
  
  return(0);
}

int validatetokens(struct tok *tokens, struct cfgerror *er)
{
  if(countbraces(tokens,er) != 0)
    return(-1);

  if(checktokenlist(tokens,er) != 0)
    return(-1);

  if(checkdatatype(tokens,er) != 0)
    return(-1);
  
  if(checkargs(tokens,er) != 0)
    return(-1);
  
  return(0);
}

void destroytokens(struct tok *tokens)
{
   int i;
   for(i = 0; i < tokens->tokidx ; i++)
     free(tokens->tokens[i]);
   
   free(tokens);
     
}

struct tok *tokenize(FILE *fp,struct cfgerror *er)
{
  char linebuff[400];
  char *p = linebuff;
  struct tok *tokens = NULL;
  struct tok *newtokens = NULL;
  char *tstart;
  char *tend;
  int tlen;
  int comment = 0; 
  int  quotation = 0;
  int i = 0;
  int k = 0;
  int j = 0;
  int tlimit = 0;
  FILE * fpi = NULL;
  glob_t globbuf;
   
  while( fgets(linebuff,(sizeof(linebuff) -1 ),fp))
    {
       
#ifdef DEBUG
       printf("tokenize() - line: %s\n",linebuff);
       fflush(stdout);
#endif
       
      p = linebuff;
    start: 

      /* Remove leading whitespace, abort on zt or comment   */
      while((*p == ' ' || *p == '\t') && 
	    (*p != '\0' || *p != '#'))
	p++;

      /* Exit on line end  */
      if(*p == 0 || *p == '\r' || *p == '\n')
	continue;
      
      /* skip comments */
      if((comment == 1) ||
	 (*p == '/' && *(p + 1) == '*'))
	{
	  comment = 1;

	  /* Move to end of comment */
	  while(!(*p == '*' && *(p +1) == '/') && *p != '\0')
	      p++;
	  
	  if(*p == '*' && *(p + 1) == '/')
	    {
	      comment = 0;
	      p+=2; /* move past */
	    }
	  goto start;
	}
       
      /* extract each token */
      tstart = p;
      tend = p;
      
      /* Walk to token end */
      while(*(tend +1) != '\0' && 
	    *(tend +1) != '#'  && 
	    *(tend +1) != ';'  && 
	    *(tend +1) != '='  &&
	    *(tend +1) != '{'  &&
	    *(tend +1) != '}'  &&
	    *(tend +1) != '\n' && 
	    *(tend +1) != '\r' && 
	    *(tend +1) != '\t' &&
	    *(tend)    != '='  && 
	    *(tend)    != '\n' && 
	    *(tend)    != '\r' && 
	    *(tend)    != '\t' && 
	    *(tend)    != '#'  && 
	    *(tend)    != '{'  && 
	    *(tend)    != '}'  && 
	    *(tend)    != ';' 
	    ) 
	{
	  if(*(tend +1) == ' ' && 
	     quotation == 0)
	    break;
	  
	  if(*tend == '"' || *(tend +1)  == '"')
	    quotation = (quotation) ? 0 : 1;
	  	  
	  tend++;
	}

      /* Exit on line-end or comment */
      if(*tend == 0 || *tend == '#' || *tend == '\r' || *tend == '\n')
	continue;
      
      tlen = (int)tend - (int)tstart;

      if(tokens == NULL)
	{
	  tokens = (struct tok *) malloc(sizeof(struct tok));
	  if(tokens == NULL)
	    {
	       if(er != NULL)
		 er->logfunc(er,"Malloc failed %s\n",strerror(errno));
	       else
		 perror("malloc");
	      exit(-1);
	    }
	  
	  tokens->tokens = (char **) malloc(sizeof(char *) * 10);
	  
	  if(tokens->tokens == NULL)
	    {
	       if(er != NULL)
		 er->logfunc(er,"Malloc failed %s\n",strerror(errno));
	       else
		 perror("malloc");
	      exit(-1);
	    }
      
	  tokens->tokidx = 0;
	  tokens->tokmax = 10;
	}
      
      if(tokens->tokidx >= (tokens->tokmax  - 1))
	{
	  tokens->tokmax = tokens->tokidx + 10;
	  tokens->tokens = (char **) realloc(tokens->tokens,
					     sizeof(char *) * tokens->tokmax);
	  if(tokens->tokens == NULL)
	    {
	       if(er != NULL)
		 er->logfunc(er,"realloc failed %s\n",strerror(errno));
	       else
		 perror("realloc");
	       
	      exit(-1);
	    }
	}

       /* Remove any quotations around the token */
       if(*tstart=='"')
	 {
	   tstart++;
	   tlen--;
	 }       

       /* Remove any quotations around the token */
       if(*tend=='"')
	 tlen --;
       
#ifdef DEBUG
       printf("tsart: %X  ... tend: %X\n",tstart,tend);
       printf("tlen: %d\n",tlen);       
#endif
       
       if(tlen < 0)
	 return(NULL);
       
       tokens->tokens[tokens->tokidx] = (char *) malloc(tlen+2);
       memcpy(tokens->tokens[tokens->tokidx],tstart,tlen+1);
       tokens->tokens[tokens->tokidx][tlen+1] = 0;
       tokens->tokidx++;
              
       /* continue parsing the line */
       p = tend + 1;
       goto start;
    }
  
  if(comment == 1)
    {
       if(er != NULL)
	 er->logfunc(er,"Unclosed comment in configuration file!\n");
       else
	 fprintf(stderr,"Unclosed comment in configuration file!\n");
       
       free(tokens);
       return(NULL);
    }
    
  if(tokens && tokens->tokidx > 0)
    if(validatetokens(tokens,er) == 0){
       tlimit = tokens->tokidx;
       for(i = 0; i < tlimit; i++){
           //fprintf(stderr, "token[%d]: %s\n", i, tokens->tokens[i]);
           if(!strcmp(tokens->tokens[i], "include")){
               //fprintf(stderr, "we need to parse %s\n", tokens->tokens[i+2]);
               globbuf.gl_offs = 0;
               if(!glob(tokens->tokens[i+2], GLOB_ERR, NULL, &globbuf)){
                   for(k=0; k<globbuf.gl_pathc; k++){
                       //fprintf(stderr, "filename: %s\n", globbuf.gl_pathv[k]);

                       newtokens = NULL;

                       fpi = fopen(globbuf.gl_pathv[k],"r");
                       if(fpi == NULL)
                       {
                           if(er != NULL)
                               er->logfunc(er,"Failed to open configuration file: %s\n",globbuf.gl_pathv[k]);
                           else
                               fprintf(stderr, "Failed to open configuration file: %s\n",globbuf.gl_pathv[k]);
                           
                           free(tokens);
                           return(NULL);
                       }

                       newtokens = tokenize(fpi, er);

                       fclose(fpi);

                       if(newtokens == NULL)
                       {
                           if(er != NULL)
                               er->logfunc(er,"Failed to parse configuration file: %s\n",globbuf.gl_pathv[k]);
                           else
                               fprintf(stderr,"Failed to parse configuration file: %s\n",globbuf.gl_pathv[k]);
                           free(tokens);
                           return(NULL);
                       }

                       tokens->tokmax += newtokens->tokidx;
                       tokens->tokens = (char **) realloc(tokens->tokens,
                                            sizeof(char *) * tokens->tokmax);
                       for(j = 0; j<newtokens->tokidx; j++){
                           tokens->tokens[tokens->tokidx] = (char *) malloc(strlen(newtokens->tokens[j])+1);
                           memcpy(tokens->tokens[tokens->tokidx], newtokens->tokens[j], strlen(newtokens->tokens[j])+1);
                           tokens->tokidx++;
                       }

                   }
                   /* globfree(&globbuf); */
               } else {
                  if(er != NULL)
                        er->logfunc(er,"glob() failed: %s\n", strerror(errno));
                  else
                        fprintf(stderr,"glob() failed: %s\n", strerror(errno));
       
                  free(tokens);
                  return(NULL);
               }
           }
       }
    
    
      return(tokens);
    }
  
  /*else */
  free(tokens);
  return(NULL);
}

struct cfg *newcfg(struct cfgerror *er)
{
  struct cfg *c;

  c = (struct cfg *) malloc(sizeof(struct cfg));
  if(c == NULL)
    {
       if(er != NULL)
	 er->logfunc(er,"Malloc failed %s\n",strerror(errno));
       else
	 perror("malloc");
       exit(-1);
    }

  c->type = -1;
  c->lvalue = NULL;
  c->rvalue = NULL;
  c->block  = NULL;
  c->next   = NULL;

  return(c);
}

struct cfg *recursiveparse(struct tok *tokens, int ts, int te,struct cfgerror *er)
{
  int cbc = 0; /* curly bracket counter, used to balance sub-blocks */
  int i,j;
  struct cfg *cstart = NULL;
  struct cfg *c = NULL;

  for(i = ts; i < (te - 1 ); i++)
    {
      switch(tokens->tokens[i+1][0])
	{
	case '{':
	  cbc++;

	  /* Find end of block */
	  for(j = (i + 2) ; j < te; j++)
	    {

	      if(tokens->tokens[j][0] == '{')
		cbc++;

	      if(tokens->tokens[j][0] == '}' && --cbc == 0)
	      {
		
		if(c == NULL)
		  c = cstart = newcfg(er);
		else
		  c = c->next = newcfg(er);
		
		c->lvalue = strdup(tokens->tokens[i]);
		c->block  = recursiveparse(tokens,(i+2),j,er);
		c->type   = BLOCK;
	  
		i = j;
		break;
	      }
	    }
	  break;

	case '=':

	  if(c == NULL)
	    c = cstart = newcfg(er);
	  else
	    c = c->next = newcfg(er);

	  c->lvalue = strdup(tokens->tokens[i]);
	  c->rvalue = strdup(tokens->tokens[i+2]);
	  c->type=ASSIGNMENT;

	  i+=3;
	  break;

	default:
	   if(er != NULL)
	     er->logfunc(er,"Unknown char: %s\n",tokens->tokens[i+1]);
	   else
	     fprintf(stderr,"Unknown char: %s\n",tokens->tokens[i+1]);
	}
    }
    
  return(cstart);
}

struct cfg *processtokens(struct tok *tokens, char *filename,struct cfgerror *er)
{
  struct cfg *c;
  
  /* cfg-header */
  c         = newcfg(er);
  c->lvalue = strdup(filename);
  c->type   = BLOCK;
  c->block  = recursiveparse(tokens,0,tokens->tokidx,er);
  
  return(c);
}

struct cfg *readconfig(char *filename, struct cfgerror *er)
{
  FILE *fp;
  struct tok *tokens;
  struct cfg *config;

  fp = fopen(filename,"r");
  if(fp == NULL)
     {
	if(er != NULL)
	  er->logfunc(er,
		      "Failed to open configuration file: %s\n",filename);
	else
	  fprintf(stderr, "Failed to open configuration file: %s\n",filename);
	return(NULL);
    }

#ifdef DEBUG
   printf("tokenize() - starting\n");
   fflush(stdout);
#endif
   
  tokens = tokenize(fp,er);
  
#ifdef DEBUG
   printf("tokenize() - complete\n");
   fflush(stdout);
#endif
   
  if(tokens == NULL)
     {
	if(er != NULL)
	  er->logfunc(er,
		      "Failed to parse configuration file: %s\n",filename);
	else
	  fprintf(stderr,"Failed to parse configuration file: %s\n",filename);
	return(NULL);
     }

  config = processtokens(tokens,filename,er);

   /* Free memory used by the tokenlist */
   destroytokens(tokens);
   
  return(config);
}

int printcfg(int level, struct cfg *c)
{
  int i = 0;

  while(c)
    {
      for( i = 0; i < level; i++)
	fprintf(stdout,"   ");

      switch(c->type)
	{
	case ASSIGNMENT:
	  fprintf(stdout," %s == %s\n",c->lvalue,c->rvalue);
	  break;

	case BLOCK:
	  fprintf(stdout," %s \n",c->lvalue);
	  printcfg(level + 1, c->block);
	  break;
	}
      c = c->next;
    }
  
  return(0);
}


/* Free the memory used by the cfg struct */
void destroycfg(struct cfg *c)
{
   struct cfg *next;
   
   while(c)
     {
	switch(c->type)
	  {
	   case ASSIGNMENT:
	     free(c->rvalue);
	     free(c->lvalue);
	     c->rvalue = NULL;
	     c->lvalue = NULL;
	     break;
	     
	   case BLOCK:
	     destroycfg(c->block);
	     c->block = NULL;
	     break;
	  }
	next = c->next;
	free(c);
	c = next;
     }
}
