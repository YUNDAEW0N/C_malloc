#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define MAX(x, y)           ((x) > (y)? (x) : (y))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT           8 // 할당하는 단위

/* Basic constants and macros */
#define WSIZE               4       /* Word and header.footer size (1bytes) */
#define DSIZE               8       /* Double word size (2bytes) */
#define CHUNKSIZE           (1<<12) /* 4Kb (데이터 섹터 크기) */

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)         (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE         (ALIGN(sizeof(size_t)))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)              (*(unsigned int *)(p))
#define PUT(p, val)         (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)            ((char *)(bp) - WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PREV_FBLKP(bp)      (*(void**)(bp))
#define NEXT_FBLKP(bp)      (*(void**)(bp + WSIZE))

#define PREV_FBLK(bp, pred_bp) (PUT(bp, ((unsigned int) pred_bp)))

// static char *heap_listp;
static char **seg_list;

////////////////////////////////////////////////////////////////////////////////
static int  find_seglist(size_t size);
static void link_block(void *bp);
static void unlink_block(void *bp);
static void *best_fit(void *bp, size_t adjust_size);
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t adjust_size);
static void place(void *bp, size_t adjust_size);
//////////////////////////////////////////////////////////////////////////////
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap*/
    if ((seg_list = mem_sbrk(12*WSIZE))== (void *)-1)
        return -1;
    /* initialize set_list */
    for (int i = 0; i < DSIZE; i++) {
        seg_list[i] = NULL;
    }
    PUT(seg_list + 8, PACK(0, 1)); /* start */
    PUT(seg_list + 9, PACK(0, 1)); /* end */
    printf("%p %p %p\n", seg_list, seg_list + 8, seg_list + 9);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  size_t adjust_size;
  size_t extend_size;
  char *bp;
  if (size == 0)
    return NULL;
  if (size <=DSIZE)
    adjust_size = 2*DSIZE;
  else
    adjust_size = ALIGN(size)+8;

  if ((bp = find_fit(adjust_size)) != NULL) {
    place(bp, adjust_size);
    return bp;
  }
  printf("!\n");
  extend_size = MAX(adjust_size, CHUNKSIZE);
  if ((bp = extend_heap(extend_size/WSIZE)) == NULL){
    return NULL;
  }
  
  place(bp, adjust_size);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp),PACK(size, 0));
    PUT(FTRP(bp),PACK(size, 0));
    coalesce(bp);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void *oldptr = bp;
    void *newptr;
    size_t copy_size;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    // copy_size = *(size_t *)((char *)oldptr - WSIZE) - 9;
    copy_size = (size_t)(GET_SIZE(HDRP(oldptr))) - DSIZE;
     //copy_size = *(size_t *)((char *)oldptr - SIZE_T_SIZE); //
    if (size < copy_size)
      copy_size = size;
    memcpy(newptr, oldptr, copy_size);
    mm_free(oldptr);
    return newptr;
}



static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전블럭이 할당되어있는지 여부를 확인하는 변수
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음블럭  =
    size_t size = GET_SIZE(HDRP(bp)); 

    /* case 1: */
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    /* case 2: */
    else if (prev_alloc && !next_alloc) 
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    }
    /* case 3: */
    else if (!prev_alloc && next_alloc) 
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    /* case 4: */
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment*/
    size = (words % 2) ? (words+1) * WSIZE : words *WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header*/
    PUT(HDRP(bp), PACK(size,0));    /* Free block header*/
    PUT(FTRP(bp), PACK(size,0));    /* Free block footer*/
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); /* New epilogue header*/
    link_block(bp);
    

    return bp;  //코올레스
}

static void *find_fit(size_t adjust_size) {
    void *bp;
    for (int index = find_seglist(adjust_size); index < DSIZE; index++) {
        if ((bp = best_fit(seg_list[index], adjust_size)) != NULL) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

static void place(void *bp, size_t adjust_size)
{
    size_t cur_size = GET_SIZE(HDRP(bp));

    if ((cur_size - adjust_size) >= (2*DSIZE))
    {
        unlink_block(bp);
        PUT(HDRP(bp), PACK(adjust_size, 1));
        PUT(FTRP(bp), PACK(adjust_size, 1));
        bp=NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(cur_size - adjust_size, 0));
        PUT(FTRP(bp), PACK(cur_size - adjust_size, 0));
        link_block(bp);
    }
    else
    {
        unlink_block(bp);
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
    }
}

static int find_seglist(size_t size) {
    if (size <= (DSIZE * 2)) return 0;
    if (size <= 2 * (DSIZE * 2)) return 1;
    if (size <= 3 * (DSIZE * 2)) return 2;
    if (size <= 4 * (DSIZE * 2)) return 3;
    if (size <= 5 * (DSIZE * 2)) return 4;
    if (size <= 6 * (DSIZE * 2)) return 5;
    if (size <= 8 * (DSIZE * 2)) return 6;
    return 7;
}

static void link_block(void *bp) {
    int index = find_seglist(GET_SIZE(HDRP(bp)));
    char *list = seg_list[index];
    if (list) {
        PUT(list, (unsigned int)bp);
    }
    PUT(bp, (unsigned int)list);
    PUT(bp + 1, NULL);
    seg_list[index] = bp;
}

static void unlink_block(void *bp) {
    // end node
    if (NEXT_FBLKP(bp) == NULL && PREV_FBLKP(bp) != NULL) {
        PUT(NEXT_FBLKP(PREV_FBLKP(bp)), NULL);
        return ;
    }
    // root node
    if (NEXT_FBLKP(bp) != NULL && PREV_FBLKP(bp) == NULL) {
        PUT(PREV_FBLKP(NEXT_FBLKP(bp)), NULL);
        seg_list[find_seglist(GET_SIZE(bp))] = NEXT_FBLKP(bp);
        return ;
    }
    // else
    PUT(PREV_FBLKP(NEXT_FBLKP(bp)), PREV_FBLKP(bp));
    PUT(NEXT_FBLKP(PREV_FBLKP(bp)), NEXT_FBLKP(bp));
}

static void *best_fit(void *bp, size_t adjust_size) {
    int ret_size = GET_SIZE(bp);
    void *ret = NULL;
    for ( ; bp != NULL; bp = NEXT_FBLKP(bp)) {
        int cur_size = GET_SIZE(bp);
        if (cur_size >= adjust_size && ret_size > cur_size) {
            ret_size = cur_size;
            ret = bp;
        }
    }
    return ret;
}








