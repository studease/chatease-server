/*
 * stu_rbtree.c
 *
 *  Created on: 2017-3-20
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static stu_inline void stu_rbtree_left_rotate(stu_rbtree_node_t **root,
    stu_rbtree_node_t *sentinel, stu_rbtree_node_t *node);
static stu_inline void stu_rbtree_right_rotate(stu_rbtree_node_t **root,
    stu_rbtree_node_t *sentinel, stu_rbtree_node_t *node);


void
stu_rbtree_insert(stu_rbtree_t *tree, stu_rbtree_node_t *node) {
	stu_rbtree_node_t  **root, *temp, *sentinel;

	/* a binary tree insert */
	root = (stu_rbtree_node_t **) &tree->root;
	sentinel = tree->sentinel;
	if (*root == sentinel) {
		node->parent = NULL;
		node->left = sentinel;
		node->right = sentinel;
		stu_rbtree_black(node);
		*root = node;

		return;
	}

	tree->insert(*root, node, sentinel);

	/* re-balance tree */
	while (node != *root && stu_rbtree_is_red(node->parent)) {
		if (node->parent == node->parent->parent->left) {
			temp = node->parent->parent->right;
			if (stu_rbtree_is_red(temp)) {
				stu_rbtree_black(node->parent);
				stu_rbtree_black(temp);
				stu_rbtree_red(node->parent->parent);
				node = node->parent->parent;
			} else {
				if (node == node->parent->right) {
					node = node->parent;
					stu_rbtree_left_rotate(root, sentinel, node);
				}

				stu_rbtree_black(node->parent);
				stu_rbtree_red(node->parent->parent);
				stu_rbtree_right_rotate(root, sentinel, node->parent->parent);
			}
		} else {
			temp = node->parent->parent->left;
			if (stu_rbtree_is_red(temp)) {
				stu_rbtree_black(node->parent);
				stu_rbtree_black(temp);
				stu_rbtree_red(node->parent->parent);
				node = node->parent->parent;
			} else {
				if (node == node->parent->left) {
					node = node->parent;
					stu_rbtree_right_rotate(root, sentinel, node);
				}

				stu_rbtree_black(node->parent);
				stu_rbtree_red(node->parent->parent);
				stu_rbtree_left_rotate(root, sentinel, node->parent->parent);
			}
		}
	}

	stu_rbtree_black(*root);
}

void
stu_rbtree_insert_value(stu_rbtree_node_t *temp, stu_rbtree_node_t *node, stu_rbtree_node_t *sentinel) {
	stu_rbtree_node_t  **p;

	for ( ;; ) {
		p = (node->key < temp->key) ? &temp->left : &temp->right;
		if (*p == sentinel) {
			break;
		}

		temp = *p;
	}

	*p = node;
	node->parent = temp;
	node->left = sentinel;
	node->right = sentinel;
	stu_rbtree_red(node);
}

void
stu_rbtree_insert_timer_value(stu_rbtree_node_t *temp, stu_rbtree_node_t *node, stu_rbtree_node_t *sentinel) {
	stu_rbtree_node_t  **p;

	for ( ;; ) {
		/*
		 * Timer values
		 * 1) are spread in small range, usually several minutes,
		 * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
		 * The comparison takes into account that overflow.
		 */

		/* node->key < temp->key */
		p = ((stu_rbtree_key_int_t) (node->key - temp->key) < 0) ? &temp->left : &temp->right;
		if (*p == sentinel) {
			break;
		}

		temp = *p;
	}

	*p = node;
	node->parent = temp;
	node->left = sentinel;
	node->right = sentinel;
	stu_rbtree_red(node);
}

void
stu_rbtree_delete(stu_rbtree_t *tree, stu_rbtree_node_t *node) {
	stu_uint_t          red;
	stu_rbtree_node_t **root, *sentinel, *subst, *temp, *w;

	/* a binary tree delete */
	root = (stu_rbtree_node_t **) &tree->root;
	sentinel = tree->sentinel;
	if (node->left == sentinel) {
		temp = node->right;
		subst = node;
	} else if (node->right == sentinel) {
		temp = node->left;
		subst = node;
	} else {
		subst = stu_rbtree_min(node->right, sentinel);
		if (subst->left != sentinel) {
			temp = subst->left;
		} else {
			temp = subst->right;
		}
	}

	if (subst == *root) {
		*root = temp;
		stu_rbtree_black(temp);

		/* DEBUG stuff */
		node->left = NULL;
		node->right = NULL;
		node->parent = NULL;
		node->key = 0;

		return;
	}

	red = stu_rbtree_is_red(subst);

	if (subst == subst->parent->left) {
		subst->parent->left = temp;
	} else {
		subst->parent->right = temp;
	}

	if (subst == node) {
		temp->parent = subst->parent;
	} else {
		if (subst->parent == node) {
			temp->parent = subst;
		} else {
			temp->parent = subst->parent;
		}

		subst->left = node->left;
		subst->right = node->right;
		subst->parent = node->parent;
		stu_rbtree_copy_color(subst, node);

		if (node == *root) {
			*root = subst;
		} else {
			if (node == node->parent->left) {
				node->parent->left = subst;
			} else {
				node->parent->right = subst;
			}
		}

		if (subst->left != sentinel) {
			subst->left->parent = subst;
		}

		if (subst->right != sentinel) {
			subst->right->parent = subst;
		}
	}

	/* DEBUG stuff */
	node->left = NULL;
	node->right = NULL;
	node->parent = NULL;
	node->key = 0;

	if (red) {
		return;
	}

	/* a delete fixup */
	while (temp != *root && stu_rbtree_is_black(temp)) {
		if (temp == temp->parent->left) {
			w = temp->parent->right;
			if (stu_rbtree_is_red(w)) {
				stu_rbtree_black(w);
				stu_rbtree_red(temp->parent);
				stu_rbtree_left_rotate(root, sentinel, temp->parent);
				w = temp->parent->right;
			}

			if (stu_rbtree_is_black(w->left) && stu_rbtree_is_black(w->right)) {
				stu_rbtree_red(w);
				temp = temp->parent;
			} else {
				if (stu_rbtree_is_black(w->right)) {
					stu_rbtree_black(w->left);
					stu_rbtree_red(w);
					stu_rbtree_right_rotate(root, sentinel, w);
					w = temp->parent->right;
				}

				stu_rbtree_copy_color(w, temp->parent);
				stu_rbtree_black(temp->parent);
				stu_rbtree_black(w->right);
				stu_rbtree_left_rotate(root, sentinel, temp->parent);
				temp = *root;
			}
		} else {
			w = temp->parent->left;
			if (stu_rbtree_is_red(w)) {
				stu_rbtree_black(w);
				stu_rbtree_red(temp->parent);
				stu_rbtree_right_rotate(root, sentinel, temp->parent);
				w = temp->parent->left;
			}

			if (stu_rbtree_is_black(w->left) && stu_rbtree_is_black(w->right)) {
				stu_rbtree_red(w);
				temp = temp->parent;
			} else {
				if (stu_rbtree_is_black(w->left)) {
					stu_rbtree_black(w->right);
					stu_rbtree_red(w);
					stu_rbtree_left_rotate(root, sentinel, w);
					w = temp->parent->left;
				}

				stu_rbtree_copy_color(w, temp->parent);
				stu_rbtree_black(temp->parent);
				stu_rbtree_black(w->left);
				stu_rbtree_right_rotate(root, sentinel, temp->parent);
				temp = *root;
			}
		}
	}

	stu_rbtree_black(temp);
}


static stu_inline void
stu_rbtree_left_rotate(stu_rbtree_node_t **root, stu_rbtree_node_t *sentinel, stu_rbtree_node_t *node) {
	stu_rbtree_node_t  *temp;

	temp = node->right;
	node->right = temp->left;
	if (temp->left != sentinel) {
		temp->left->parent = node;
	}

	temp->parent = node->parent;
	if (node == *root) {
		*root = temp;
	} else if (node == node->parent->left) {
		node->parent->left = temp;
	} else {
		node->parent->right = temp;
	}

	temp->left = node;
	node->parent = temp;
}

static stu_inline void
stu_rbtree_right_rotate(stu_rbtree_node_t **root, stu_rbtree_node_t *sentinel, stu_rbtree_node_t *node) {
	stu_rbtree_node_t  *temp;

	temp = node->left;
	node->left = temp->right;
	if (temp->right != sentinel) {
		temp->right->parent = node;
	}

	temp->parent = node->parent;
	if (node == *root) {
		*root = temp;
	} else if (node == node->parent->right) {
		node->parent->right = temp;
	} else {
		node->parent->left = temp;
	}

	temp->right = node;
	node->parent = temp;
}

