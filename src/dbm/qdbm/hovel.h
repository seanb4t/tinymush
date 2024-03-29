#include "copyright.h"

#ifndef __HOVEL_H                         /* duplication check */
#define __HOVEL_H

/*
 * The GDBM-compatible API of QDBM
 */

#if defined(__cplusplus)                 /* export for C++ */
extern "C" {
#endif

#if defined(_MSC_VER) && !defined(QDBM_INTERNAL) && !defined(QDBM_STATIC)
#define MYEXTERN extern __declspec(dllimport)
#else
#define MYEXTERN extern
#endif



/*************************************************************************************************
 * API
 *************************************************************************************************/


enum {                                   /* enumeration for error codes */
  GDBM_NO_ERROR,                         /* no error */
  GDBM_MALLOC_ERROR,                     /* malloc error */
  GDBM_BLOCK_SIZE_ERROR,                 /* block size error */
  GDBM_FILE_OPEN_ERROR,                  /* file open error */
  GDBM_FILE_WRITE_ERROR,                 /* file write error */
  GDBM_FILE_SEEK_ERROR,                  /* file seek error */
  GDBM_FILE_READ_ERROR,                  /* file read error */
  GDBM_BAD_MAGIC_NUMBER,                 /* bad magic number */
  GDBM_EMPTY_DATABASE,                   /* empty database */
  GDBM_CANT_BE_READER,                   /* can't be reader */
  GDBM_CANT_BE_WRITER,                   /* can't be writer */
  GDBM_READER_CANT_DELETE,               /* reader can't delete */
  GDBM_READER_CANT_STORE,                /* reader can't store */
  GDBM_READER_CANT_REORGANIZE,           /* reader can't reorganize */
  GDBM_UNKNOWN_UPDATE,                   /* unknown update */
  GDBM_ITEM_NOT_FOUND,                   /* item not found */
  GDBM_REORGANIZE_FAILED,                /* reorganize failed */
  GDBM_CANNOT_REPLACE,                   /* cannot replace */
  GDBM_ILLEGAL_DATA,                     /* illegal data */
  GDBM_OPT_ALREADY_SET,                  /* option already set */
  GDBM_OPT_ILLEGAL                       /* option illegal */
};

typedef int gdbm_error;                  /* type of error codes */

typedef struct {                         /* type of structure for a database handle */
  DEPOT *depot;                          /* internal database handle of Depot */
  CURIA *curia;                          /* internal database handle of Curia */
  int syncmode;                          /* whether to be besyncronous mode */
} GDBM;

typedef GDBM *GDBM_FILE;                 /* type of pointer to a database handle */

typedef struct {                         /* type of structure for a key or a value */
  char *dptr;                            /* pointer to the region */
  size_t dsize;                          /* size of the region */
} datum;

enum {                                   /* enumeration for open modes */
  GDBM_READER = 1 << 0,                  /* open as a reader */
  GDBM_WRITER = 1 << 1,                  /* open as a writer */
  GDBM_WRCREAT = 1 << 2,                 /* a writer creating */
  GDBM_NEWDB = 1 << 3,                   /* a writer creating and truncating */
  GDBM_SYNC = 1 << 4,                    /* syncronous mode */
  GDBM_NOLOCK = 1 << 5,                  /* no lock mode */
  GDBM_LOCKNB = 1 << 6,                  /* non-blocking lock mode */
  GDBM_FAST = 1 << 7,                    /* fast mode */
  GDBM_SPARSE = 1 << 8                   /* create as sparse file */
};

enum {                                   /* enumeration for write modes */
  GDBM_INSERT,                           /* keep an existing value */
  GDBM_REPLACE                           /* overwrite an existing value */
};

enum {                                   /* enumeration for options */
  GDBM_CACHESIZE,                        /* set cache size */
  GDBM_FASTMODE,                         /* set fast mode */
  GDBM_SYNCMODE,                         /* set syncronous mode */
  GDBM_CENTFREE,                         /* set free block pool */
  GDBM_COALESCEBLKS                      /* set free block marging */
};


/* String containing the version information. */
MYEXTERN char *gdbm_version;


/* Last happened error code. */
#define gdbm_errno     (*gdbm_errnoptr())


/* Get a message string corresponding to an error code.
   `gdbmerrno' specifies an error code.
   The return value is the message string of the error code.  The region of the return value
   is not writable. */
char *gdbm_strerror(gdbm_error gdbmerrno);


/* Get a database handle after the fashion of GDBM.
   `name' specifies a name of a database.
   `read_write' specifies the connection mode: `GDBM_READER' as a reader, `GDBM_WRITER',
   `GDBM_WRCREAT' and `GDBM_NEWDB' as a writer.  `GDBM_WRCREAT' makes a database file or
   directory if it does not exist.  `GDBM_NEWDB' makes a new database even if it exists.
   You can add the following to writer modes by bitwise or: `GDBM_SYNC', `GDBM_NOLOCK',
   `GDBM_LCKNB', `GDBM_FAST', and `GDBM_SPARSE'.  `GDBM_SYNC' means a database is synchronized
   after every updating method.  `GDBM_NOLOCK' means a database is opened without file locking.
   `GDBM_LOCKNB' means file locking is performed without blocking.  `GDBM_FAST' is ignored.
   `GDBM_SPARSE' is an original mode of QDBM and makes database a sparse file.
   `mode' specifies a mode of a database file or a database directory as the one of `open'
   or `mkdir' call does.
   `bnum' specifies the number of elements of each bucket array.  If it is not more than 0,
   the default value is specified.
   `dnum' specifies the number of division of the database.  If it is not more than 0, the
   returning handle is created as a wrapper of Depot, else, it is as a wrapper of Curia.
   The return value is the database handle or `NULL' if it is not successful.
   If the database already exists, whether it is one of Depot or Curia is measured
   automatically. */
GDBM_FILE gdbm_open(char *name, int block_size, int read_write, int mode, void (*fatal_func)());


/* Get a database handle after the fashion of QDBM.
   `name' specifies a name of a database.
   `read_write' specifies the connection mode: `GDBM_READER' as a reader, `GDBM_WRITER',
   `GDBM_WRCREAT' and `GDBM_NEWDB' as a writer.  `GDBM_WRCREAT' makes a database file or
   directory if it does not exist.  `GDBM_NEWDB' makes a new database even if it exists.
   You can add the following to writer modes by bitwise or: `GDBM_SYNC', `GDBM_NOLOCK',
   `GDBM_LOCKNB', `GDBM_FAST', and `GDBM_SPARSE'.  `GDBM_SYNC' means a database is synchronized
   after every updating method.  `GDBM_NOLOCK' means a database is opened without file locking.
   `GDBM_LOCKNB' means file locking is performed without blocking.  `GDBM_FAST' is ignored.
   `GDBM_SPARSE' is an original mode of QDBM and makes database sparse files.
   `mode' specifies a mode of a database file as the one of `open' or `mkdir' call does.
   `bnum' specifies the number of elements of each bucket array.  If it is not more than 0,
   the default value is specified.
   `dnum' specifies the number of division of the database.  If it is not more than 0, the
   returning handle is created as a wrapper of Depot, else, it is as a wrapper of Curia.
   `align' specifies the basic size of alignment.
   The return value is the database handle or `NULL' if it is not successful. */
GDBM_FILE gdbm_open2(char *name, int read_write, int mode, int bnum, int dnum, int align);


/* Close a database handle.
   `dbf' specifies a database handle.
   Because the region of the closed handle is released, it becomes impossible to use the
   handle. */
void gdbm_close(GDBM_FILE dbf);


/* Store a record.
   `dbf' specifies a database handle connected as a writer.
   `key' specifies a structure of a key.  `content' specifies a structure of a value.
   `flag' specifies behavior when the key overlaps, by the following values: `GDBM_REPLACE',
   which means the specified value overwrites the existing one, `GDBM_INSERT', which means
   the existing value is kept.
   The return value is 0 if it is successful, 1 if it gives up because of overlaps of the key,
   -1 if other error occurs. */
int gdbm_store(GDBM_FILE dbf, datum key, datum content, int flag);


/* Delete a record.
   `dbf' specifies a database handle connected as a writer.
   `key' specifies a structure of a key.
   The return value is 0 if it is successful, -1 if some errors occur. */
int gdbm_delete(GDBM_FILE dbf, datum key);


/* Retrieve a record.
   `dbf' specifies a database handle.
   `key' specifies a structure of a key.
   The return value is a structure of the result.
   If a record corresponds, the member `dptr' of the structure is the pointer to the region
   of the value.  If no record corresponds or some errors occur, `dptr' is `NULL'.  Because
   the region pointed to by `dptr' is allocated with the `malloc' call, it should be
   released with the `free' call if it is no longer in use. */
datum gdbm_fetch(GDBM_FILE dbf, datum key);


/* Check whether a record exists or not.
   `dbf' specifies a database handle.
   `key' specifies a structure of a key.
   The return value is true if a record corresponds and no error occurs, or false, else,
   it is false. */
int gdbm_exists(GDBM_FILE dbf, datum key);


/* Get the first key of a database.
   `dbf' specifies a database handle.
   The return value is a structure of the result.
   If a record corresponds, the member `dptr' of the structure is the pointer to the region
   of the first key.  If no record corresponds or some errors occur, `dptr' is `NULL'.
   Because the region pointed to by `dptr' is allocated with the `malloc' call, it should
   be released with the `free' call if it is no longer in use. */
datum gdbm_firstkey(GDBM_FILE dbf);


/* Get the next key of a database.
   `dbf' specifies a database handle.
   The return value is a structure of the result.
   If a record corresponds, the member `dptr' of the structure is the pointer to the region
   of the next key.  If no record corresponds or some errors occur, `dptr' is `NULL'.
   Because the region pointed to by `dptr' is allocated with the `malloc' call, it should
   be released with the `free' call if it is no longer in use. */
datum gdbm_nextkey(GDBM_FILE dbf, datum key);


/* Synchronize updating contents with the file and the device.
   `dbf' specifies a database handle connected as a writer. */
void gdbm_sync(GDBM_FILE dbf);


/* Reorganize a database.
   `dbf' specifies a database handle connected as a writer.
   If successful, the return value is 0, else -1. */
int gdbm_reorganize(GDBM_FILE dbf);


/* Get the file descriptor of a database file.
   `dbf' specifies a database handle connected as a writer.
   The return value is the file descriptor of the database file.
   If the database is a directory the return value is -1. */
int gdbm_fdesc(GDBM_FILE dbf);


/* No effect.
   `dbf' specifies a database handle.
   `option' is ignored.  `size' is ignored.
   The return value is 0.
   The function is only for compatibility. */
int gdbm_setopt(GDBM_FILE dbf, int option, int *value, int size);



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


/* Get the pointer of the last happened error code. */
int *gdbm_errnoptr(void);

/* Start with the constant definitions.  */
#define  TRUE    1
#define  FALSE   0

/* Parameters to gdbm_open. */
#define  GDBM_READER  0                 /* READERS only. */
#define  GDBM_WRITER  1                 /* READERS and WRITERS.  Can not
                                         * create. */
#define  GDBM_WRCREAT 2                 /* If not found, create the db. */
#define  GDBM_NEWDB   3                 /* ALWAYS create a new db.  (WRITER) */
#define  GDBM_OPENMASK 7                /* Mask for the above. */
#define  GDBM_FAST    0x10              /* Write fast! => No fsyncs.
                                         * OBSOLETE. */
#define  GDBM_SYNC    0x20              /* Sync operations to the disk. */
#define  GDBM_NOLOCK  0x40              /* Don't do file locking operations. */

/* Parameters to gdbm_store for simple insertion or replacement in the
   case a key to store is already in the database. */
#define  GDBM_INSERT  0                 /* Do not overwrite data in the
                                         * database. */
#define  GDBM_REPLACE 1                 /* Replace the old value with the new
                                         * value. */

/* Parameters to gdbm_setopt, specifing the type of operation to perform. */
#define  GDBM_CACHESIZE 1               /* Set the cache size. */
#define  GDBM_FASTMODE  2               /* Turn on or off fast mode.
                                         * OBSOLETE. */
#define  GDBM_SYNCMODE  3               /* Turn on or off sync operations. */
#define  GDBM_CENTFREE  4               /* Keep all free blocks in the header. */
#define  GDBM_COALESCEBLKS 5            /* Attempt to coalesce free blocks. */

/* In freeing blocks, we will ignore any blocks smaller (and equal) to
   IGNORE_SIZE number of bytes. */
#define IGNORE_SIZE		4

/* The number of key bytes kept in a hash bucket. */
#define SMALL			4

/* The number of bucket_avail entries in a hash bucket. */
#define BUCKET_AVAIL		6

/* The size of the bucket cache. */
#define DEFAULT_CACHESIZE	10

/* The dbm hash bucket element contains the full 31 bit hash value, the
   "pointer" to the key and data (stored together) with their sizes.  It also
   has a small part of the actual key value.  It is used to verify the first
   part of the key has the correct value without having to read the actual
   key. */

typedef struct {
        char    start_tag[4];
        int     hash_value;             /* The complete 31 bit value. */
        char    key_start[SMALL];       /* Up to the first SMALL bytes of the
                                         * key.  */
        off_t   data_pointer;           /* The file address of the key record.
                                         * The data record directly follows
                                         * the key.  */
        int     key_size;               /* Size of key data in the file. */
        int     data_size;              /* Size of associated data in the
                                         * file. */
}       bucket_element;

#undef MYEXTERN

#if defined(__cplusplus)                 /* export for C++ */
}
#endif

#endif                                   /* __HOVEL_H */
