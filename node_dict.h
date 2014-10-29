/*
 * Dictionary Abstract Data Type
 * Copyright (C) 1997 Kaz Kylheku <kaz@ashi.footprints.net>
 *
 * Free Software License:
 *
 * All rights are reserved by the author, with the following exceptions:
 * Permission is granted to freely reproduce and distribute this software,
 * possibly in exchange for a fee, provided that this copyright notice appears
 * intact. Permission is also granted to adapt this software to produce
 * derivative works, as long as the modified versions carry this copyright
 * notice and additional notices stating that the work has been modified.
 * This source code may be translated into executable form and incorporated
 * into proprietary software; there is no requirement for such software to
 * contain a copyright notice related to this source.
 *
 * Heavily modified 3/2006 Cubane Software Inc. to be useful for interning 
 * strings in node library
 */

#ifndef DICT_H
#define DICT_H

#include <limits.h>

/*
 * Blurb for inclusion into C++ translation units
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long dictcount_t;
#define DICTCOUNT_T_MAX ULONG_MAX

/*
 * The dictionary is implemented as a red-black tree
 */

typedef enum { dnode_red, dnode_black } dnode_color_t;

#define DNODE_KEY_BASE_LENGTH 4

typedef struct dnode_t 
{
    struct dnode_t *dict_left;
    struct dnode_t *dict_right;
    struct dnode_t *dict_parent;
    unsigned int dict_color:2;
	unsigned int dict_hash:30;
	int dict_refcount;
    unsigned char dict_key[DNODE_KEY_BASE_LENGTH];
} dnode_t;

typedef struct dict_t 
{
    dnode_t dict_nilnode;
} dict_t;

extern dict_t * dict_create();
extern void dict_free_nodes(dict_t *);

extern dnode_t * dict_lookupA(dict_t *, const char *);
extern dnode_t * dict_ensure_existsA(dict_t *dict, const char *key );

extern dnode_t * dict_lookupW(dict_t *, const wchar_t *);
extern dnode_t * dict_ensure_existsW(dict_t *dict, const wchar_t *key );

extern dnode_t * dict_delete(dict_t *, dnode_t *);

extern dnode_t * dict_first(dict_t *);
extern dnode_t * dict_next(dict_t *, dnode_t *);

extern dnode_t * dnode_init(dnode_t *);

#ifdef __cplusplus
}
#endif

#endif
