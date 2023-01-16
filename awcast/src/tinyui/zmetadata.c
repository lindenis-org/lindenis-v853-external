#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include <CdxList.h>

#include "zmetadata.h"

struct MD_ItemS
{
	char key[32];
	char value[512];
	CdxListNodeT node;
};

struct ZMetadataS
{
	pthread_mutex_t lock;
	CdxListT item_list; // TODO: if large data, shold use RB-Tree to store.
};

ZMetadataT *ZMD_Instance()
{
	struct ZMetadataS *md = NULL;

	md = malloc(sizeof(*md));

	pthread_mutex_init(&md->lock, NULL);
	CdxListInit(&md->item_list);

	return md;
}

int ZMD_Add(ZMetadataT *md, const char *key)
{
	struct MD_ItemS *new_item;

	new_item = malloc(sizeof(*new_item));

	snprintf(new_item->key, 31, "%s", key);

	new_item->value[0] = '\0';

	pthread_mutex_lock(&md->lock);

	CdxListAddTail(&new_item->node, &md->item_list);

	pthread_mutex_unlock(&md->lock);

	return 0;
}

int ZMD_Set(ZMetadataT *md, const char *key, const char *value)
{
	struct MD_ItemS *item;
	int ret = -1;

	pthread_mutex_lock(&md->lock);

	CdxListForEachEntry(item, &md->item_list, node)
	{
		if (strcmp(item->key, key) == 0)
		{
			sprintf(item->value, "%s", value);
			ret = 0;
			break;
		}
	}

	pthread_mutex_unlock(&md->lock);

	return ret;
}

char *ZMD_Get(ZMetadataT *md, const char *key)
{
	struct MD_ItemS *item;
	char *ret = NULL;

	pthread_mutex_lock(&md->lock);

	CdxListForEachEntry(item, &md->item_list, node)
	{
		if (strcmp(item->key, key) == 0)
		{
			ret = item->value;
			break;
		}
	}

	pthread_mutex_unlock(&md->lock);
	return ret;
}

int ZMD_Destroy(ZMetadataT *md)
{
	struct MD_ItemS *item, *next;

	CdxListForEachEntrySafe(item, next, &md->item_list, node)
	{
		CdxListDel(&item->node);
		free(item);
	}
	return 0;
}

