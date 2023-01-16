/*
 * Insert a new_node entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
CDX_INTERFACE void __CdxListAdd(struct CdxListNodeS *new_node,
                        struct CdxListNodeS *prev, struct CdxListNodeS *next)
{
	next->prev = new_node;
	new_node->next = next;
	new_node->prev = prev;
	prev->next = new_node;
}

/**
 * list_add - add a new_node entry
 * @new_node: new_node entry to be added
 * @head: list head to add it after
 *
 * Insert a new_node entry after the specified head.
 * This is good for implementing stacks.
 */
CDX_INTERFACE void CdxListAdd(struct CdxListNodeS *new_node, struct CdxListS *list)
{
	__CdxListAdd(new_node, (struct CdxListNodeS *)list, list->head);
}

CDX_INTERFACE void CdxListAddBefore(struct CdxListNodeS *new_node,
                                    struct CdxListNodeS *pos)
{
	__CdxListAdd(new_node, pos->prev, pos);
}

CDX_INTERFACE void CdxListAddAfter(struct CdxListNodeS *new_node,
                                    struct CdxListNodeS *pos)
{
	__CdxListAdd(new_node, pos, pos->next);
}

CDX_INTERFACE void CdxListAddTail(struct CdxListNodeS *new_node, struct CdxListS *list)
{
	__CdxListAdd(new_node, list->tail, (struct CdxListNodeS *)list);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
CDX_INTERFACE void __CdxListDel(struct CdxListNodeS *prev, struct CdxListNodeS *next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
CDX_INTERFACE void __CdxListDelEntry(struct CdxListNodeS *node)
{
	__CdxListDel(node->prev, node->next);
}

CDX_INTERFACE void CdxListDel(struct CdxListNodeS *node)
{
	__CdxListDel(node->prev, node->next);
	node->next = (struct CdxListNodeS *)CDX_LIST_POISON1;
	node->prev = (struct CdxListNodeS *)CDX_LIST_POISON2;
}

/**
 * list_replace - replace old entry by new_node one
 * @old : the element to be replaced
 * @new_node : the new_node element to insert
 *
 * If @old was empty, it will be overwritten.
 */
CDX_INTERFACE void CdxListReplace(struct CdxListNodeS *old,
				                struct CdxListNodeS *new_node)
{
	new_node->next = old->next;
	new_node->next->prev = new_node;
	new_node->prev = old->prev;
	new_node->prev->next = new_node;
}

CDX_INTERFACE void CdxListReplaceInit(struct CdxListNodeS *old,
					            struct CdxListNodeS *new_node)
{
	CdxListReplace(old, new_node);
	CdxListNodeInit(old);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
CDX_INTERFACE void CdxListDelInit(struct CdxListNodeS *node)
{
	__CdxListDelEntry(node);
	CdxListNodeInit(node);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
CDX_INTERFACE void CdxListMove(struct CdxListNodeS *node, struct CdxListS *list)
{
	__CdxListDelEntry(node);
	CdxListAdd(node, list);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
CDX_INTERFACE void CdxListMoveTail(struct CdxListNodeS *node,
                                struct CdxListS *list)
{
	__CdxListDelEntry(node);
	CdxListAddTail(node, list);
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
CDX_INTERFACE int CdxListIsLast(const struct CdxListNodeS *node,
				            const struct CdxListS *list)
{
	return node->next == (struct CdxListNodeS *)list;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
CDX_INTERFACE int CdxListEmpty(const struct CdxListS *list)
{
	return (list->head == (struct CdxListNodeS *)list)
	       && (list->tail == (struct CdxListNodeS *)list);
}

/**
 * list_rotate_left - rotate the list to the left
 * @head: the head of the list
 */
CDX_INTERFACE void CdxListRotateLeft(struct CdxListS *list)
{
	struct CdxListNodeS *first;

	if (!CdxListEmpty(list)) {
		first = list->head;
		CdxListMoveTail(first, list);
	}
}

/**
 * list_is_singular - tests whether a list has just one entry.
 * @head: the list to test.
 */
CDX_INTERFACE int CdxListIsSingular(const struct CdxListS *list)
{
	return !CdxListEmpty(list) && (list->head == list->tail);
}
