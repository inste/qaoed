 /* 
  * Definitions for the ATA over Ethernet protocol.
  */

#include "eth.h"

/* Valid commands for aoeproc.c */
#define CMDEINVAL   ((int)( -1))
#define CMDREG      ((int)(  0))
#define CMDUNREG    ((int)(  1))
#define CMDHOSTMASK ((int)(  3))
#define CMDRMMASK   ((int)(  4))

/* Bit field in ver_flags of aoe-header */
#define AOE_FLAG_RSP (1<<3)
#define AOE_FLAG_ERR (1<<2)

/* Error codes for the error field in the aoe header */
#define AOE_ERR_BADCMD       1	/* Unrecognized command code */
#define AOE_ERR_BADARG       2	/* Bad argument parameter */
#define AOE_ERR_DEVUNAVAIL   3	/* Device unavailable */
#define AOE_ERR_CFG_SET      4	/* Config string present */
#define AOE_ERR_UNSUPVER     5	/* Unsupported version */

/* Commands in cmd-field of aoe-header */
#define AOE_CMD_ATA 0
#define AOE_CMD_CFG 1

/* CFG read/match/write commands */
#define AOE_CFG_READ 0
#define AOE_CFG_MATCH 1
#define AOE_CFG_MATCH_PARTIAL 2
#define AOE_CFG_SET_IF_EMPTY 3
#define AOE_CFG_SET 4


struct aoe_hdr {
	struct ethhdr eth;       /* ethernet header, see eth.h */
	unsigned char ver_flags; /* bit field: 7-4 ver, 3 rsp, 2 error, 0&1 zero */
	unsigned char error;	 /* If error bit is set, fill out this field */
	unsigned short shelf;
	unsigned char slot;
	unsigned char cmd;	 /* ATA or CFG */
	unsigned int  tag;	 /* Uniqueue request tag */
} __attribute__ ((packed));

#define MAXATALBA (int)(0x0fffffff)

/* Bitfields for the flags field in the aoe ata header */
#define AOE_ATAFLAG_LBA48 (1 << 6)
#define AOE_ATAFLAG_ASYNC (1 << 1)
#define AOE_ATAFLAG_WRITE (1 << 0)

struct aoe_atahdr {
   struct aoe_hdr aoehdr;
   unsigned char flags;	      /* bitfield: 7 zero, 6 LBA48, 5 zero
			       * 4 Device/head, 3 zero, 2 zero, 
			       * 1 Async IO, 0 Read/Write */
   unsigned char err_feature;  /* Check linux/hdreg.h for status codes */
   unsigned char nsect;	       /* Number of sectors to transfer */
   unsigned char cmdstat;      /* cmd in request and status in reply */
   unsigned char lba[6];       /* 48bit LBA addressing */
   unsigned char notused[2];   /* reserved */
   unsigned char data[0];
} __attribute__ ((packed));

struct aoe_cfghdr {
   struct aoe_hdr aoehdr;
   unsigned short queuelen;   /* The number of requests we can queue */
   unsigned short firmware;   /* Firmware version */
   unsigned char  maxsect;    /* Sector count */
   unsigned char  aoever_cmd; /* bit field: 7-4 aoe-version, 3 - 0 cmd */
   unsigned short cfg_len;    /* data length */
   unsigned char  cfg[0];     /* Just a pointer to this point */
} __attribute__ ((packed));

