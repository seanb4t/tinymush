/* alloc.c - memory allocation subsystem */

#include "copyright.h"
#include "config.h"
#include "system.h"

#include "typedefs.h"	/* required by mudconf */
#include "game.h"	/* required by mudconf */
#include "alloc.h"	/* required by mudconf */
#include "flags.h"	/* required by mudconf */
#include "htab.h"	/* required by mudconf */
#include "ltdl.h"	/* required by mudconf */
#include "udb.h"	/* required by mudconf */
#include "udb_defs.h"	/* required by mudconf */

#include "mushconf.h"	/* required by code */
#include "db.h"		/* required by externs.h */
#include "interface.h"
#include "externs.h"	/* required by code */

POOL pools[NUM_POOLS];
const char *poolnames[] =
{ "Sbufs", "Mbufs", "Lbufs", "Bools", "Descs", "Qentries", "Pcaches" };

#define POOL_MAGICNUM 0xdeadbeef

void pool_init( int poolnum, int poolsize ) {
    pools[poolnum].pool_size = poolsize;
    pools[poolnum].free_head = NULL;
    pools[poolnum].chain_head = NULL;
    pools[poolnum].tot_alloc = 0;
    pools[poolnum].num_alloc = 0;
    pools[poolnum].max_alloc = 0;
    pools[poolnum].num_lost = 0;
    return;
}

static void pool_err( const char *logsys, int logflag, int poolnum, const char *tag, POOLHDR *ph, const char *action, const char *reason ) {
    if( !mudstate.logging ) {
        log_write( logflag, logsys, "ALLOC", "%s[%d] (tag %s) %s at %lx. (%s)", action, pools[poolnum].pool_size, tag, reason, ( long ) ph, mudstate.debug_cmd );
    } else if( logflag != LOG_ALLOCATE ) {
        log_write( logflag | LOG_FORCE, logsys, "ALLOC", "%s[%d] (tag %s) %s at %lx", action, pools[poolnum].pool_size, tag, reason, ( long ) ph );
    }
}

static void pool_vfy( int poolnum, const char *tag ) {
    POOLHDR *ph, *lastph;

    POOLFTR *pf;

    char *h;

    int psize;

    lastph = NULL;
    psize = pools[poolnum].pool_size;
    for( ph = pools[poolnum].chain_head; ph; lastph = ph, ph = ph->next ) {
        h = ( char * ) ph;
        h += sizeof( POOLHDR );
        h += pools[poolnum].pool_size;
        pf = ( POOLFTR * ) h;

        if( ph->magicnum != POOL_MAGICNUM ) {
            pool_err( "BUG", LOG_ALWAYS, poolnum, tag, ph, "Verify", "header corrupted (clearing freelist)" );

            /*
             * Break the header chain at this point so we don't
             * generate an error for EVERY alloc and free, also
             * we can't continue the scan because the next
             * pointer might be trash too.
             */

            if( lastph ) {
                lastph->next = NULL;
            } else {
                pools[poolnum].chain_head = NULL;
            }
            return;	/* not safe to continue */
        }
        if( pf->magicnum != POOL_MAGICNUM ) {
            pool_err( "BUG", LOG_ALWAYS, poolnum, tag, ph, "Verify", "footer corrupted" );
            pf->magicnum = POOL_MAGICNUM;
        }
        if( ph->pool_size != psize ) {
            pool_err( "BUG", LOG_ALWAYS, poolnum, tag, ph, "Verify", "header has incorrect size" );
        }
    }
}

void pool_check( const char *tag ) {
    int poolnum;

    for( poolnum = 0; poolnum < NUM_POOLS; ++poolnum ) {
        pool_vfy( poolnum, tag );
    }
}

char *pool_alloc( int poolnum, const char *tag ) {
    int *p;

    char *h;

    POOLHDR *ph;

    POOLFTR *pf;

    if( mudconf.paranoid_alloc ) {
        pool_check( tag );
    }
    do {
        if( pools[poolnum].free_head == NULL ) {
            h = ( char * ) RAW_MALLOC( pools[poolnum].pool_size + sizeof( POOLHDR ) + sizeof( POOLFTR ), "pool_alloc.ph" );
            if( h == NULL ) {
                log_write_raw( 1, "ABORT! alloc.c, pool_alloc() failed to get memory." );
                abort();
            }
            ph = ( POOLHDR * ) h;
            h += sizeof( POOLHDR );
            p = ( int * ) h;
            h += pools[poolnum].pool_size;
            pf = ( POOLFTR * ) h;
            ph->next = pools[poolnum].chain_head;
            ph->nxtfree = NULL;
            ph->magicnum = POOL_MAGICNUM;
            ph->pool_size = pools[poolnum].pool_size;
            pf->magicnum = POOL_MAGICNUM;
            *p = POOL_MAGICNUM;
            pools[poolnum].chain_head = ph;
            pools[poolnum].max_alloc++;
        } else {
            ph = ( POOLHDR * )( pools[poolnum].free_head );
            h = ( char * ) ph;
            h += sizeof( POOLHDR );
            p = ( int * ) h;
            h += pools[poolnum].pool_size;
            pf = ( POOLFTR * ) h;
            pools[poolnum].free_head = ph->nxtfree;

            /*
             * If corrupted header we need to throw away the
             * freelist as the freelist pointer may be corrupt.
             */

            if( ph->magicnum != POOL_MAGICNUM ) {
                pool_err( "BUG", LOG_ALWAYS, poolnum, tag, ph,
                          "Alloc", "corrupted buffer header" );

                /*
                 * Start a new free list and record stats
                 */

                p = NULL;
                pools[poolnum].free_head = NULL;
                pools[poolnum].num_lost +=
                    ( pools[poolnum].tot_alloc -
                      pools[poolnum].num_alloc );
                pools[poolnum].tot_alloc =
                    pools[poolnum].num_alloc;
            }
            /*
             * Check for corrupted footer, just report and fix it
             */

            if( pf->magicnum != POOL_MAGICNUM ) {
                pool_err( "BUG", LOG_ALWAYS, poolnum, tag, ph,
                          "Alloc", "corrupted buffer footer" );
                pf->magicnum = POOL_MAGICNUM;
            }
        }
    } while( p == NULL );

    ph->buf_tag = ( char * ) tag;
    pools[poolnum].tot_alloc++;
    pools[poolnum].num_alloc++;

    pool_err( "DBG", LOG_ALLOCATE, poolnum, tag, ph, "Alloc", "buffer" );

    /*
     * If the buffer was modified after it was last freed, log it.
     */

    if( ( *p != POOL_MAGICNUM ) && ( !mudstate.logging ) ) {
        pool_err( "BUG", LOG_PROBLEMS, poolnum, tag, ph, "Alloc",
                  "buffer modified after free" );
    }
    *p = 0;
    return ( char * ) p;
}

void pool_free( int poolnum, char **buf ) {
    int *ibuf;

    char *h;

    POOLHDR *ph;

    POOLFTR *pf;

    ibuf = ( int * ) *buf;
    h = ( char * ) ibuf;
    h -= sizeof( POOLHDR );
    ph = ( POOLHDR * ) h;
    h = ( char * ) ibuf;
    h += pools[poolnum].pool_size;
    pf = ( POOLFTR * ) h;
    if( mudconf.paranoid_alloc ) {
        pool_check( ph->buf_tag );
    }

    /*
     * Make sure the buffer header is good.  If it isn't, log the error
     * and throw away the buffer.
     */

    if( ph->magicnum != POOL_MAGICNUM ) {
        pool_err( "BUG", LOG_ALWAYS, poolnum, ph->buf_tag, ph, "Free",
                  "corrupted buffer header" );
        pools[poolnum].num_lost++;
        pools[poolnum].num_alloc--;
        pools[poolnum].tot_alloc--;
        return;
    }
    /*
     * Verify the buffer footer.  Don't unlink if damaged, just repair
     */

    if( pf->magicnum != POOL_MAGICNUM ) {
        pool_err( "BUG", LOG_ALWAYS, poolnum, ph->buf_tag, ph, "Free",
                  "corrupted buffer footer" );
        pf->magicnum = POOL_MAGICNUM;
    }
    /*
     * Verify that we are not trying to free someone else's buffer
     */

    if( ph->pool_size != pools[poolnum].pool_size ) {
        pool_err( "BUG", LOG_ALWAYS, poolnum, ph->buf_tag, ph, "Free",
                  "Attempt to free into a different pool." );
        return;
    }
    pool_err( "DBG", LOG_ALLOCATE, poolnum, ph->buf_tag, ph, "Free",
              "buffer" );

    /*
     * Make sure we aren't freeing an already free buffer.  If we are,
     * log an error, otherwise update the pool header and stats
     */

    if( *ibuf == POOL_MAGICNUM ) {
        pool_err( "BUG", LOG_BUGS, poolnum, ph->buf_tag, ph, "Free",
                  "buffer already freed" );
    } else {
        *ibuf = POOL_MAGICNUM;
        ph->nxtfree = pools[poolnum].free_head;
        pools[poolnum].free_head = ph;
        pools[poolnum].num_alloc--;
    }
}

static char *pool_stats( int poolnum, const char *text ) {
    char *buf;

    buf = alloc_mbuf( "pool_stats" );
    sprintf( buf, "%-15s %5d%9d%9d%9d%9d", text, pools[poolnum].pool_size, pools[poolnum].num_alloc, pools[poolnum].max_alloc, pools[poolnum].tot_alloc, pools[poolnum].num_lost );
    return buf;
}

static void pool_trace( dbref player, int poolnum, const char *text ) {
    POOLHDR *ph;

    int numfree, *ibuf;

    char *h;

    numfree = 0;
    notify_check( player, player, MSG_PUP_ALWAYS|MSG_ME_ALL|MSG_F_DOWN, "----- %s -----", text );
    for( ph = pools[poolnum].chain_head; ph != NULL; ph = ph->next ) {
        if( ph->magicnum != POOL_MAGICNUM ) {
            notify( player, "*** CORRUPTED BUFFER HEADER, ABORTING SCAN ***" );
            notify_check( player, player, MSG_PUP_ALWAYS|MSG_ME_ALL|MSG_F_DOWN, "%d free %s (before corruption)", numfree, text );
            return;
        }
        h = ( char * ) ph;
        h += sizeof( POOLHDR );
        ibuf = ( int * ) h;
        if( *ibuf != POOL_MAGICNUM ) {
            notify( player, ph->buf_tag );
        } else {
            numfree++;
        }
    }
    notify_check( player, player, MSG_PUP_ALWAYS|MSG_ME_ALL|MSG_F_DOWN, "%d free %s", numfree, text );
}

void list_bufstats( dbref player ) {
    int i;

    char *buff;

    notify( player, "Buffer Stats     Size    InUse    Total   Allocs     Lost" );

    for( i = 0; i < NUM_POOLS; i++ ) {
        buff = pool_stats( i, poolnames[i] );
        notify( player, buff );
        free_mbuf( buff );
    }
}

void list_buftrace( dbref player ) {
    int i;

    for( i = 0; i < NUM_POOLS; i++ ) {
        pool_trace( player, i, poolnames[i] );
    }
}

void pool_reset( void ) {
    POOLHDR *ph, *phnext, *newchain;

    int i, *ibuf;

    char *h;


    for( i = 0; i < NUM_POOLS; i++ ) {
        newchain = NULL;
        for( ph = pools[i].chain_head; ph != NULL; ph = phnext ) {
            h = ( char * ) ph;
            phnext = ph->next;
            h += sizeof( POOLHDR );
            ibuf = ( int * ) h;
            if( *ibuf == POOL_MAGICNUM ) {
                RAW_FREE( ph, "pool_reset.ph" );
            } else {
                if( !newchain ) {
                    newchain = ph;
                    ph->next = NULL;
                } else {
                    ph->next = newchain;
                    newchain = ph;
                }
                ph->nxtfree = NULL;
            }
        }
        pools[i].chain_head = newchain;
        pools[i].free_head = NULL;
        pools[i].max_alloc = pools[i].num_alloc;
    }
}

/*
 * ---------------------------------------------------------------------------
 * Track allocations that don't use the memory pools.
 */

#ifdef TEST_MALLOC

int malloc_count = 0;
int malloc_bytes = 0;
void *malloc_ptr;
void *malloc_str;

void *xmalloc(size_t x, const char *y) {
    
    log_write( LOG_MALLOC, "MEM", "TRACE", "xmalloc: %s/%d", y, x);  
    malloc_count++;
    malloc_ptr = (void *)malloc(x + sizeof(int));
    malloc_bytes += x;
    *(int *)malloc_ptr = x;
    return(malloc_ptr + sizeof(int));
}

void *xcalloc(size_t x, size_t z, const char *y) {
    log_write( LOG_MALLOC, "MEM", "TRACE", "xcalloc: %s/%d", y, x*z);
    malloc_count++;
    malloc_ptr = (void *)malloc(x * z  + sizeof(int) );
    malloc_bytes += x * z;
    memset(malloc_ptr, 0, x * z + sizeof(int));
    *(int *)malloc_ptr = x * z;
    return(malloc_ptr + sizeof(int));
}

void *xrealloc(void *x, size_t z, const char *y) {
    log_write( LOG_MALLOC, "MEM", "TRACE", "xrealloc: %s/%d", y, z);
    malloc_ptr = (void *)malloc( z + sizeof(int));
    malloc_bytes += z;
    if(x) {
        malloc_bytes -= *(int *)((void *) x - sizeof(int));
        memcpy(malloc_ptr + sizeof(int), x, *(int *)((void *) x - sizeof(int)));
        free((void *)x - sizeof(int));
    }
    *(int *)malloc_ptr = z;
    return(malloc_ptr + sizeof(int));
}

void *xstrdup(const void *x, const char *y) {
    if(x || *x) {
        malloc_str = (char *) x;
        log_write( LOG_MALLOC, "MEM", "TRACE", "xstrdup: %s/%d", y, strlen(malloc_str) + 1 );
        malloc_count++;
        malloc_ptr = (void *)malloc(strlen(malloc_str) + 1 + sizeof(int));
        malloc_bytes += strlen(malloc_str) + 1;
        strcpy(malloc_ptr + sizeof(int), malloc_str);
        *(int *)malloc_ptr = strlen(malloc_str) + 1;
        return(malloc_ptr + sizeof(int));
    } else {
        return(NULL);
}



void *xstrndup(const void *x, size_t z, const char *y) {
    if(x || *x) {
        malloc_str = (void *) x;
        log_write( LOG_MALLOC, "MEM", "TRACE", "xstrndup: %s/%d", y, strlen(malloc_str) + 1 );
        malloc_count++;
        malloc_ptr = (void *)malloc(z + sizeof(int));
        malloc_bytes += z;
        strncpy(malloc_ptr + sizeof(int), malloc_str, z);
        *(int *)malloc_ptr = z;
        return(malloc_ptr + sizeof(int));
    } else {
        return(NULL);
    }
}

void xfree(void *x, const char *y) {
    log_write( LOG_MALLOC, "MEM", "TRACE", "Free: %s/%d", y, x ? *(int *)((void *) x - sizeof(int))  : 0);
    if(x) {
        malloc_count--;
        malloc_bytes -= *(int *)((void *) x - sizeof(int));
        free((void *) x - sizeof(int));
        x=NULL;
    }
}

#endif

void *xstrprintf(const char *y, const char *format, ...) {
    char msg[LBUF_SIZE];
    va_list ap;
    
    if( format || *format ) {
        va_start( ap, format );
        vsnprintf(msg, LBUF_SIZE, format, ap);
        va_end( ap );
        return( XSTRDUP( (void *) msg, y ));
    } else {
        return( (void *) NULL );
    }
}

#ifdef RAW_MEMTRACKING

static int sort_memtable( const void *p1, const void *p2 ) {
    return strcmp( ( * ( MEMTRACK ** ) p1 )->buf_tag, ( * ( MEMTRACK ** ) p2 )->buf_tag );
}

void list_rawmemory( dbref player ) {
    MEMTRACK *tptr, **t_array;

    int n_tags, total, c_tags, c_total, u_tags, i, j;

    const char *ntmp;

    n_tags = total = 0;

    raw_notify( player, NULL, "Memory Tag                           Allocs      Bytes");

    for( tptr = mudstate.raw_allocs; tptr != NULL; tptr = tptr->next ) {
        n_tags++;
        total += ( int ) tptr->alloc;
    }

    t_array = ( MEMTRACK ** ) RAW_CALLOC( total, sizeof( MEMTRACK * ), "list_rawmemory" );

    for( i = 0, tptr = mudstate.raw_allocs; tptr != NULL; i++, tptr = tptr->next ) {
        t_array[i] = tptr;
    }

    qsort( ( void * ) t_array, n_tags, sizeof( MEMTRACK * ), ( int ( * )( const void *, const void * ) ) sort_memtable );

    for( i = 0; i < n_tags; ) {
        u_tags++;
        ntmp = t_array[i]->buf_tag;
        if( ( i < n_tags - 1 ) && !strcmp( ntmp, t_array[i + 1]->buf_tag ) ) {
            c_tags = 2;
            c_total = ( int ) t_array[i]->alloc + ( int ) t_array[i + 1]->alloc;
            for( j = i + 2; ( j < n_tags ) && !strcmp( ntmp, t_array[j]->buf_tag ); j++ ) {
                c_tags++;
                c_total += ( int ) t_array[j]->alloc;
            }
            i = j;
        } else {
            c_tags = 1;
            c_total = ( int ) t_array[i]->alloc;
            i++;
        }
        raw_notify( player, "%-35.35s  %6d   %8d", ntmp, c_tags, c_total );
    }

    RAW_FREE( t_array, "list_rawmemory" );

    raw_notify( player, "Total: %d raw allocations in %d unique tags. %d bytes (%d K).", n_tags, u_tags, total, ( int ) total / 1024 );
}

static void trace_alloc( site_t amount, const char *name, void *ptr ) {
    /*
     * We maintain an unsorted list, most recently-allocated things at
     * the head, based on the belief that it's basically a stack -- when
     * we go to free something it's usually the most recent thing that
     * we've allocated.
     */

    MEMTRACK *tptr;

    tptr = ( MEMTRACK * ) RAW_MALLOC( sizeof( MEMTRACK ), "trace_alloc.tptr" );
    if( !tptr ) {
        return;
    }

    tptr->buf_tag = name;
    tptr->bptr = ptr;
    tptr->alloc = amount;
    tptr->next = mudstate.raw_allocs;
    mudstate.raw_allocs = tptr;
}

static void trace_free( const char *name, void *ptr ) {
    MEMTRACK *tptr, *prev;

    prev = NULL;
    for( tptr = mudstate.raw_allocs; tptr != NULL; tptr = tptr->next ) {
        if( tptr->bptr == ptr ) {
            if( strcmp( tptr->buf_tag, name ) ) {
                log_write( LOG_BUGS, "MEM", "TRACE", "Free mismatch, tag %s allocated as %s", name, tptr->buf_tag );
            }
            if( mudstate.raw_allocs == tptr ) {
                mudstate.raw_allocs = tptr->next;
            }
            if( prev ) {
                prev->next = tptr->next;
            }
            RAW_FREE( tptr, "trace_alloc.tptr" );
            return;
        }
        prev = tptr;
    }

    log_write( LOG_BUGS, "MEM", "TRACE", "Attempt to free unknown with tag %s", name );
}

void *track_malloc( size_t amount, const char *name ) {
    void *r;

    r = malloc( amount );
    trace_alloc( amount, name, r );
    return ( r );
}

void *track_calloc( size_t elems, size_t esize, const char *name ) {
    void *r;

    r = calloc( elems, esize );
    trace_alloc( elems * esize, name, r );
    return ( r );
}

void *track_realloc( void *ptr, size_tamount, const char *name ) {
    void *r;

    trace_free( name, r );
    r = realloc( ptr, amount );
    trace_alloc( amount, name, r );
    return ( r );
}

char *track_strdup( const char *str, const char *name ) {
    char *r;
    
    if(str || *str) {
        r = strdup( str );
        trace_alloc( strlen( str ) + 1, name, r );
        return ( r );
    } else {
        return(NULL)
    }
}

void track_free( void *ptr, const char *name ) {
    trace_free( name, ptr );
    free( ptr );
}

#else

void list_rawmemory( dbref player ) {
    notify( player, "Feature not supported." );
}

#endif				/* RAW_MEMTRACKING */

void safe_copy_chr(char src, char buff[], char **bufp, int max) {
    char *tp;
    
    tp = *bufp;
    if ((tp - buff) < max) {
	*tp++ = src;
	*bufp = tp;
	*tp = '\0';
    } else {
	buff[max] = '\0';
    }
}
