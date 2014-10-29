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
*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include "node_shared.h"

/*
* These macros provide short convenient names for structure members,
* which are embellished with dict_ prefixes so that they are
* properly confined to the documented namespace. It's legal for a 
* program which uses dict to define, for instance, a macro called ``parent''.
* Such a macro would interfere with the dnode_t struct definition.
* In general, highly portable and reusable C modules which expose their
* structures need to confine structure member names to well-defined spaces.
* The resulting identifiers aren't necessarily convenient to use, nor
* readable, in the implementation, however!
*/

#define left dict_left
#define right dict_right
#define parent dict_parent
#define color dict_color
#define key dict_key

#define nilnode dict_nilnode

#define dictptr dict_dictptr

#define dict_root(D) ((D)->nilnode.left)
#define dict_nil(D) (&(D)->nilnode)

#pragma warning( disable:4706 ) // assignment in conditional -- need to suppress for marked line in wcscmp_inl

static __inline int __cdecl wcscmp_inl(
									   const wchar_t * src,
									   const wchar_t * dst
									   )
{
	int ret = 0 ;
	
	while( ! (ret = (int)(*src - *dst)) && *dst) /* there is an assignment in this conditional */
		++src, ++dst;
	
	if ( ret < 0 )
		ret = -1 ;
	else if ( ret > 0 )
		ret = 1 ;
	
	return( ret );
}

static void dict_insert_internal( dict_t * dict, dnode_t * parent, dnode_t * node, int result );

/*
* Perform a ``left rotation'' adjustment on the tree.	The given node P and
* its right child C are rearranged so that the P instead becomes the left
* child of C.	 The left subtree of C is inherited as the new right subtree
* for P.  The ordering of the keys within the tree is thus preserved.
*/

static void rotate_left(dnode_t *upper)
{
	dnode_t *lower, *lowleft, *upparent;
	
	lower = upper->right;
	upper->right = lowleft = lower->left;
	lowleft->parent = upper;
	
	lower->parent = upparent = upper->parent;
	
	/* don't need to check for root node here because root->parent is
	   the sentinel nil node, and root->parent->left points back to root */
	
	if (upper == upparent->left) {
		upparent->left = lower;
	} else {
		upparent->right = lower;
	}
	
	lower->left = upper;
	upper->parent = lower;
}

/*
* This operation is the ``mirror'' image of rotate_left. It is
* the same procedure, but with left and right interchanged.
*/

static void rotate_right(dnode_t *upper)
{
	dnode_t *lower, *lowright, *upparent;
	
	lower = upper->left;
	upper->left = lowright = lower->right;
	lowright->parent = upper;
	
	lower->parent = upparent = upper->parent;
	
	if (upper == upparent->right) {
		upparent->right = lower;
	} else {
		upparent->left = lower;
	}
	
	lower->right = upper;
	upper->parent = lower;
}

/*
* Do a postorder traversal of the tree rooted at the specified
* node and free everything under it.  Used by dict_free().
*/

static void free_nodes(dnode_t *node, dnode_t *nil)
{
	dnode_t * l, * r;
	if (node == nil)
		return;
	
	l = node->left;
	r = node->right;
	free( node );
	
	free_nodes(l, nil);
	free_nodes(r, nil);
}


/*
* Dynamically allocate and initialize a dictionary object.
*/

dict_t * dict_create()
{
	dict_t *new = node_malloc(sizeof *new);
	
	if (new) {
		new->nilnode.left = &new->nilnode;
		new->nilnode.right = &new->nilnode;
		new->nilnode.parent = &new->nilnode;
		new->nilnode.color = dnode_black;
	}
	return new;
}


/*
* Free all the nodes in the dictionary by using the dictionary's
* installed free routine. The dictionary is emptied.
*/

void dict_free_nodes(dict_t *dict)
{
	dnode_t *nil = dict_nil(dict), *root = dict_root(dict);
	free_nodes(root, nil);
	dict->nilnode.left = &dict->nilnode;
	dict->nilnode.right = &dict->nilnode;
}

dnode_t * dict_lookupA(dict_t *dict, const char *key)
{
	dnode_t *root;
	dnode_t *nil;
	int result;
	
	if( dict == NULL )
		return NULL;
	
	root = dict_root(dict);
	nil = dict_nil(dict);
	
	/* simple binary search */
	
	while (root != nil) 
	{
		result = strcmp(key, (const char *)root->key);
		if (result < 0)
			root = root->left;
		else if (result > 0)
			root = root->right;
		else 
			return root;
	}
	
	return NULL;
}


dnode_t * dict_ensure_existsA(dict_t *dict, const char *key )
{
	dnode_t *root;
	dnode_t *parent;
	dnode_t *nil;
	int result = -1;
	size_t size;

	int keylen = 0;
	
	parent = root = dict_root(dict);
	nil = dict_nil(dict);
	
	/* simple binary search */
	
	while (root != nil) 
	{
		parent = root;
		result = strcmp(key, (const char *)root->key);
		if (result < 0)
			root = root->left;
		else if (result > 0)
			root = root->right;
		else 
		{
			return root;
		}
	}
	
	/* not found - create */
	keylen = strlen(key)+1;
	size = sizeof(dnode_t) + sizeof(char)*keylen - DNODE_KEY_BASE_LENGTH;
	root = (dnode_t *)node_malloc( size );
	if (root != NULL) 
	{
		strcpy_s( (char *)(root->dict_key), keylen, key );
		
		dict_insert_internal( dict, parent, root, result );
		
		root->dict_hash = node_hashA( key );
		root->dict_refcount = 0;
	}
	
	return root;
}

dnode_t * dict_lookupW(dict_t *dict, const wchar_t *key)
{
	dnode_t *root;
	dnode_t *nil;
	int result;
	
	if( dict == NULL )
		return NULL;
	
	root = dict_root(dict);
	nil = dict_nil(dict);
	
	/* simple binary search */
	
	while (root != nil) 
	{
		result = wcscmp_inl(key, (const wchar_t*)root->key);
		if (result < 0)
			root = root->left;
		else if (result > 0)
			root = root->right;
		else 
			return root;
	}
	
	return NULL;
}

dnode_t * dict_ensure_existsW(dict_t *dict, const wchar_t *key )
{
	dnode_t *root;
	dnode_t *parent;
	dnode_t *nil;
	size_t size;
	int result = -1;
	int keylen = 0;
	
	if( dict == NULL )
		return NULL;
	
	parent = root = dict_root(dict);
	nil = dict_nil(dict);
	
	/* simple binary search */
	
	while (root != nil) 
	{
		parent = root;
		result = wcscmp_inl(key, (const wchar_t*)root->key);
		if (result < 0)
			root = root->left;
		else if (result > 0)
			root = root->right;
		else 
			return root;
	}

	keylen = wcslen(key);

	/* not found - create */
	size = sizeof(dnode_t) + sizeof(wchar_t)*(keylen + 1) - DNODE_KEY_BASE_LENGTH;
	root = (dnode_t *)node_malloc( size );
	if (root != NULL) 
	{
		wcscpy_s( (wchar_t *)(root->dict_key), keylen, key );
		
		dict_insert_internal( dict, parent, root, result );
		
		root->dict_hash = node_hashW( key );
		root->dict_refcount = 0;
	}
	
	return root;
}

/*
* Insert a node into the dictionary. The node should have been
* initialized with a data field. All other fields are ignored.
*/
static void dict_insert_internal( dict_t * dict, dnode_t * parent, dnode_t * node, int result )
{
	dnode_t * nil = dict_nil(dict);
	dnode_t *uncle, *grandpa;
	
	if (result < 0)
		parent->left = node;
	else
		parent->right = node;
	
	node->parent = parent;
	node->left = nil;
	node->right = nil;
	
	/* red black adjustments */
	
	node->color = dnode_red;
	
	while (parent->color == dnode_red) 
	{
		grandpa = parent->parent;
		if (parent == grandpa->left) 
		{
			uncle = grandpa->right;
			if (uncle->color == dnode_red) 
			{	/* red parent, red uncle */
				parent->color = dnode_black;
				uncle->color = dnode_black;
				grandpa->color = dnode_red;
				node = grandpa;
				parent = grandpa->parent;
			} 
			else 
			{				/* red parent, black uncle */
				if (node == parent->right) 
				{
					rotate_left(parent);
					parent = node;
					/* rotation between parent and child preserves grandpa */
				}
				parent->color = dnode_black;
				grandpa->color = dnode_red;
				rotate_right(grandpa);
				break;
			}
		} 
		else 
		{	/* symmetric cases: parent == parent->parent->right */
			uncle = grandpa->left;
			if (uncle->color == dnode_red) 
			{
				parent->color = dnode_black;
				uncle->color = dnode_black;
				grandpa->color = dnode_red;
				node = grandpa;
				parent = grandpa->parent;
			} 
			else 
			{
				if (node == parent->left) 
				{
					rotate_right(parent);
					parent = node;
				}
				parent->color = dnode_black;
				grandpa->color = dnode_red;
				rotate_left(grandpa);
				break;
			}
		}
	}
	
	dict_root(dict)->color = dnode_black;
	
}

/*
* Delete the given node from the dictionary. If the given node does not belong
* to the given dictionary, undefined behavior results.  A pointer to the
* deleted node is returned.
*/

dnode_t * dict_delete(dict_t *dict, dnode_t *delete)
{
	dnode_t *nil = dict_nil(dict), *child, *delparent = delete->parent;
	
	/* basic deletion */
	
	/*
	* If the node being deleted has two children, then we replace it with its
	* successor (i.e. the leftmost node in the right subtree.) By doing this,
	* we avoid the traditional algorithm under which the successor's key and
	* value *only* move to the deleted node and the successor is spliced out
	* from the tree. We cannot use this approach because the user may hold
	* pointers to the successor, or nodes may be inextricably tied to some
	* other structures by way of embedding, etc. So we must splice out the
	* node we are given, not some other node, and must not move contents from
	* one node to another behind the user's back.
	*/
	
	if (delete->left != nil && delete->right != nil) {
		dnode_t *next = dict_next(dict, delete);
		dnode_t *nextparent = next->parent;
		dnode_color_t nextcolor = next->color;
		
		/*
		* First, splice out the successor from the tree completely, by
		* moving up its right child into its place.
		*/
		
		child = next->right;
		child->parent = nextparent;
		
		if (nextparent->left == next) {
			nextparent->left = child;
		} else {
			nextparent->right = child;
		}
		
		/*
		* Now that the successor has been extricated from the tree, install it
		* in place of the node that we want deleted.
		*/
		
		next->parent = delparent;
		next->left = delete->left;
		next->right = delete->right;
		next->left->parent = next;
		next->right->parent = next;
		next->color = delete->color;
		delete->color = nextcolor;
		
		if (delparent->left == delete) {
			delparent->left = next;
		} else {
			delparent->right = next;
		}
		
	} else {
		child = (delete->left != nil) ? delete->left : delete->right;
		
		child->parent = delparent = delete->parent; 	
		
		if (delete == delparent->left) {
			delparent->left = child;	
		} else {
			delparent->right = child;
		}
	}
	
	delete->parent = NULL;
	delete->right = NULL;
	delete->left = NULL;
	
	/* red-black adjustments */
	
	if (delete->color == dnode_black) {
		dnode_t *parent, *sister;
		
		dict_root(dict)->color = dnode_red;
		
		while (child->color == dnode_black) {
			parent = child->parent;
			if (child == parent->left) {
				sister = parent->right;
				if (sister->color == dnode_red) {
					sister->color = dnode_black;
					parent->color = dnode_red;
					rotate_left(parent);
					sister = parent->right;
				}
				if (sister->left->color == dnode_black
					&& sister->right->color == dnode_black) {
					sister->color = dnode_red;
					child = parent;
				} else {
					if (sister->right->color == dnode_black) {
						sister->left->color = dnode_black;
						sister->color = dnode_red;
						rotate_right(sister);
						sister = parent->right;
					}
					sister->color = parent->color;
					sister->right->color = dnode_black;
					parent->color = dnode_black;
					rotate_left(parent);
					break;
				}
			} else {	/* symmetric case: child == child->parent->right */
				sister = parent->left;
				if (sister->color == dnode_red) {
					sister->color = dnode_black;
					parent->color = dnode_red;
					rotate_right(parent);
					sister = parent->left;
				}
				if (sister->right->color == dnode_black
					&& sister->left->color == dnode_black) {
					sister->color = dnode_red;
					child = parent;
				} else {
					if (sister->left->color == dnode_black) {
						sister->right->color = dnode_black;
						sister->color = dnode_red;
						rotate_left(sister);
						sister = parent->left;
					}
					sister->color = parent->color;
					sister->left->color = dnode_black;
					parent->color = dnode_black;
					rotate_right(parent);
					break;
				}
			}
		}
		
		child->color = dnode_black;
		dict_root(dict)->color = dnode_black;
	}
	
	
	return delete;
}

/*
* Return the node with the lowest (leftmost) key. If the dictionary is empty
* (that is, dict_isempty(dict) returns 1) a null pointer is returned.
*/

dnode_t * dict_first(dict_t *dict)
{
	dnode_t *nil = dict_nil(dict), *root = dict_root(dict), *left;
	
	if (root != nil)
		while ((left = root->left) != nil)
			root = left;
		
	return (root == nil) ? NULL : root;
}

/*
* Return the given node's successor node---the node which has the
* next key in the the left to right ordering. If the node has
* no successor, a null pointer is returned rather than a pointer to
* the nil node.
*/

dnode_t * dict_next(dict_t *dict, dnode_t *curr)
{
	dnode_t *nil = dict_nil(dict), *parent, *left;
	
	if (curr->right != nil) 
	{
		curr = curr->right;
		while ((left = curr->left) != nil)
			curr = left;
		return curr;
	}
	
	parent = curr->parent;
	
	while (parent != nil && curr == parent->right) 
	{
		curr = parent;
		parent = curr->parent;
	}
	
	return (parent == nil) ? NULL : parent;
}
