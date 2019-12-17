/* A splay-tree datatype.
   Copyright (C) 1998-2017 Free Software Foundation, Inc.
   Contributed by Mark Mitchell (mark@markmitchell.com).

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* The splay tree code copied from include/splay-tree.h and adjusted,
   so that all the data lives directly in splay_tree_node_s structure
   and no extra allocations are needed.  */

/* For an easily readable description of splay-trees, see:

     Lewis, Harry R. and Denenberg, Larry.  Data Structures and Their
     Algorithms.  Harper-Collins, Inc.  1991.

   The major feature of splay trees is that all basic tree operations
   are amortized O(log n) time for a tree with n nodes.  */

#include "libgomp.h"

/* Rotate the edge joining the left child N with its parent P.  PP is the
   grandparents' pointer to P.  */

static inline void
rotate_left (splay_tree_node *pp, splay_tree_node p, splay_tree_node n)
{
  splay_tree_node tmp;
  tmp = n->right;
  n->right = p;
  p->left = tmp;
  *pp = n;
}

/* Rotate the edge joining the right child N with its parent P.  PP is the
   grandparents' pointer to P.  */

static inline void
rotate_right (splay_tree_node *pp, splay_tree_node p, splay_tree_node n)
{
  splay_tree_node tmp;
  tmp = n->left;
  n->left = p;
  p->right = tmp;
  *pp = n;
}

/* Bottom up splay of KEY.  */

static void
splay_tree_splay (splay_tree sp, splay_tree_key key)
{
  if (sp->root == NULL)
    return;

  do {
    int cmp1, cmp2;
    splay_tree_node n, c;

    n = sp->root;
    cmp1 = splay_compare (key, &n->key);

    /* Found.  */
    if (cmp1 == 0)
      return;

    /* Left or right?  If no child, then we're done.  */
    if (cmp1 < 0)
      c = n->left;
    else
      c = n->right;
    if (!c)
      return;

    /* Next one left or right?  If found or no child, we're done
       after one rotation.  */
    cmp2 = splay_compare (key, &c->key);
    if (cmp2 == 0
	|| (cmp2 < 0 && !c->left)
	|| (cmp2 > 0 && !c->right))
      {
	if (cmp1 < 0)
	  rotate_left (&sp->root, n, c);
	else
	  rotate_right (&sp->root, n, c);
	return;
      }

    /* Now we have the four cases of double-rotation.  */
    if (cmp1 < 0 && cmp2 < 0)
      {
	rotate_left (&n->left, c, c->left);
	rotate_left (&sp->root, n, n->left);
      }
    else if (cmp1 > 0 && cmp2 > 0)
      {
	rotate_right (&n->right, c, c->right);
	rotate_right (&sp->root, n, n->right);
      }
    else if (cmp1 < 0 && cmp2 > 0)
      {
	rotate_right (&n->left, c, c->right);
	rotate_left (&sp->root, n, n->left);
      }
    else if (cmp1 > 0 && cmp2 < 0)
      {
	rotate_left (&n->right, c, c->left);
	rotate_right (&sp->root, n, n->right);
      }
  } while (1);
}

/* Insert a new NODE into SP.  The NODE shouldn't exist in the tree.  */

attribute_hidden void
splay_tree_insert (splay_tree sp, splay_tree_node node)
{
  int comparison = 0;

  splay_tree_splay (sp, &node->key);

  if (sp->root)
    comparison = splay_compare (&sp->root->key, &node->key);

  if (sp->root && comparison == 0)
    gomp_fatal ("Duplicate node");
  else
  {
    /* HP-LIST */
    node->marked = false;
    node->prev = node->next = NULL;

    /* Insert it at the root.  */
    if (sp->root == NULL)
    {
      node->left = node->right = NULL;
    }
    else if (comparison < 0)
    {
      node->left = sp->root;
      node->right = node->left->right;
      node->left->right = NULL;
    }
    else
    {
      node->right = sp->root;
      node->left = node->right->left;
      node->right->left = NULL;
    }

    sp->root = node;
  }
}

/* Remove node with KEY from SP.  It is not an error if it did not exist.  */

attribute_hidden void
splay_tree_remove (splay_tree sp, splay_tree_key key)
{
  splay_tree_splay (sp, key);

  if (sp->root && splay_compare (&sp->root->key, key) == 0)
  {
    splay_tree_node left, right;

    left = sp->root->left;
    right = sp->root->right;

    /* One of the children is now the root.  Doesn't matter much
       which, so long as we preserve the properties of the tree.  */
    if (left)
    {
      sp->root = left;

      /* If there was a right child as well, hang it off the
         right-most leaf of the left child.  */
      if (right)
      {
        while (left->right)
          left = left->right;
        left->right = right;
      }
    }
    else
      sp->root = right;
  }
}

/* Mark a NODE into SP.  Also determine whether the sp->highest_marked pointer must be updated.  */

attribute_hidden int
splay_tree_hp_list_mark_node (splay_tree sp, splay_tree_node node, bool is_marked)
{
  int ret = 0;

  if (is_marked)
  {
    if (!node->marked)
    {
      sp->num_marked += 1;
      node->marked = true;
      if (sp->highest_marked == NULL || splay_compare (&node->key, &sp->highest_marked->key) > 0)
      {
        ret = 1;
        sp->highest_marked = node;
      }
    }
  }
  else
  {
    if (node->marked)
    {
      sp->num_marked -= 1;
      node->marked = false;
      if (sp->highest_marked == node)
      {
        ret = 1;
        sp->highest_marked = NULL;
        if (sp->num_marked > 0)
        {
          while ((node = node->prev) != NULL)
          {
            if (node->marked)
            {
              sp->highest_marked = node;
              break;
            }
          }
        }
      }
    }
  }

  return ret;
}

/* Insert a new NODE into SP.  The NODE shouldn't exist in the tree.  */

attribute_hidden int
splay_tree_hp_list_insert (splay_tree sp, splay_tree_node node, bool is_marked)
{
  int ret = 0;
  int comparison = 0;

  splay_tree_splay (sp, &node->key);

  if (sp->root)
    comparison = splay_compare (&sp->root->key, &node->key);

  if (sp->root && comparison == 0)
    gomp_fatal ("Duplicate node");
  else
  {
    /* Insert it at the root.  */
    if (sp->root == NULL)
    {
      node->left = node->right = NULL;
      /* HP-LIST */
      node->prev = node->next = NULL;
    }
    else if (comparison < 0)
    {
      node->left = sp->root;
      node->right = node->left->right;
      node->left->right = NULL;
      /* HP-LIST */
      node->prev = sp->root;
      node->next = node->prev->next;
      node->prev->next = node;
      if (node->next)
        node->next->prev = node;
    }
    else
    {
      node->right = sp->root;
      node->left = node->right->left;
      node->right->left = NULL;
      /* HP-LIST */
      node->next = sp->root;
      node->prev = node->next->prev;
      node->next->prev = node;
      if (node->prev)
        node->prev->next = node;
    }

    sp->root = node;

    /* HP-LIST */
    if (is_marked)
    {
      sp->num_marked += 1;
      node->marked = true;
      if (sp->highest_marked == NULL || splay_compare (&node->key, &sp->highest_marked->key) > 0)
      {
        ret = 1;
        sp->highest_marked = node;
      }
    }
    else
      node->marked = false;
  }

  return ret;
}

/* Remove node with KEY from SP.  It is not an error if it did not exist.  */

attribute_hidden int
splay_tree_hp_list_remove (splay_tree sp, splay_tree_key key)
{
  int ret = 0;

  splay_tree_splay (sp, key);

  if (sp->root && splay_compare (&sp->root->key, key) == 0)
  {
    splay_tree_node left, right;

    /* HP-LIST */
    if (sp->root->marked)
      sp->num_marked -= 1;
    if (sp->root->next)
      sp->root->next->prev = sp->root->prev;
    if (sp->root->prev)
      sp->root->prev->next = sp->root->next;
    if (sp->highest_marked == sp->root)
    {
      ret = 1;
      splay_tree_node new_hp_wait = sp->highest_marked;
      sp->highest_marked = NULL;
      if (sp->num_marked > 0)
      {
        while ((new_hp_wait = new_hp_wait->prev) != NULL)
        {
          if (new_hp_wait->marked)
          {
            sp->highest_marked = new_hp_wait;
            break;
          }
        }
      }
    }

    left = sp->root->left;
    right = sp->root->right;

    /* One of the children is now the root.  Doesn't matter much
       which, so long as we preserve the properties of the tree.  */
    if (left)
    {
      sp->root = left;

      /* If there was a right child as well, hang it off the
         right-most leaf of the left child.  */
      if (right)
      {
        while (left->right)
          left = left->right;
        left->right = right;
      }
    }
    else
      sp->root = right;
  }

  return ret;
}

/* Lookup KEY in SP, returning NODE if present, and NULL
   otherwise.  */

attribute_hidden splay_tree_key
splay_tree_lookup (splay_tree sp, splay_tree_key key)
{
  splay_tree_splay (sp, key);

  if (sp->root && splay_compare (&sp->root->key, key) == 0)
    return &sp->root->key;
  else
    return NULL;
}

/* Helper function for splay_tree_foreach.

   Run FUNC on every node in KEY.  */

static void
splay_tree_foreach_internal (splay_tree_node node, splay_tree_callback func,
			     void *data)
{
  if (!node)
    return;
  func (&node->key, data);
  splay_tree_foreach_internal (node->left, func, data);
  /* Yeah, whatever.  GCC can fix my tail recursion.  */
  splay_tree_foreach_internal (node->right, func, data);
}

/* Run FUNC on each of the nodes in SP.  */

attribute_hidden void
splay_tree_foreach (splay_tree sp, splay_tree_callback func, void *data)
{
  splay_tree_foreach_internal (sp->root, func, data);
}
