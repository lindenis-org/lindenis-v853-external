#include <stdint.h>
#include <unistd.h>

#include <sys/time.h>
#include <pthread.h>

#include <libxml/parser.h>
//#include <libxml/tree.h>

#include <cdx_log.h>
#include <CdxList.h>
#include <AwPool.h>

#include "ipaint.h"
#include "zmetadata.h"
#include "tinyui.h"
#include <tina_log.h>

#define ITEM_TYPE_IMAGE 0
#define ITEM_TYPE_TEXT  1
#define ITEM_TYPE_RECT  2

#define TAG_LAYER "layer"
#define TAG_WIDGET "widget"
#define TAG_ELEMENT "element"
#define TAG_ITEM "item"
#define TAG_IMAGE "image"
#define TAG_TEXT "text"

#define ATTR_X "x"
#define ATTR_Y "y"
#define ATTR_W "w"
#define ATTR_H "h"
#define ATTR_BG "background"
#define ATTR_NAME "name"
#define ATTR_INDEX "index"
#define ATTR_EFFECT "effect"
#define ATTR_TIMER "timer"

#define ATTR_FONT_SIZE "font_size"
#define ATTR_COLOR "color"
#define ATTR_VALUE "value"
#define ATTR_TYPE "type"
#define ATTR_MOTION_STEP "motion_step"
#define ATTR_VPOS "vpos"
#define ATTR_HPOS "hpos"

#define VAL_HPOS_LEFT "left"
#define VAL_HPOS_RIGHT "right"
#define VAL_VPOS_TOP "top"
#define VAL_VPOS_BOTTOM "bottom"

#define VAL_TYPE_IMAGE "image"
#define VAL_TYPE_TEXT "text"
#define VAL_TYPE_RECT "rect"

#define VAL_STATIC 			"static"
#define VAL_ONCE_MOTION 	"once_motion"
#define VAL_CYCLE_MOTION 	"cycle_motion"

#define VAL_TRUE "true"
#define VAL_FALSE "false"

#define METADATA_PREFIX "metadata."
#define METADATA_PREFIX_LEN 9

#define TUI_SCREEN_W 1280
#define TUI_SCREEN_H 720

static const char *def_val_static = "static";

struct TAG_TextS
{
	char *value;
	uint32_t color;
	int font_size;
	uint32_t align;
	CdxListNodeT node;
};

struct TAG_RectS
{
	char *value;
	uint32_t color;
	CdxListNodeT node;
};

struct TAG_ImageS
{
	char *value;
	CdxListNodeT node;
};

struct TAG_ItemS
{
	int type;
	char *index;

	const char *effect; /* default: static, once_motion, cycle_motion */
	int motion_step; /* ms */

	CdxListT contents;
	CdxListNodeT node;
};

struct TAG_ElementS
{
	char *name;
	int x;
	int y;
	int w;
	int h;
	char *index; /* null or match one item's index */
	CdxListT items;
	CdxListNodeT node;
};

struct TAG_WidgetS
{
	char *name;
	int x;
	int y;
	int w;
	int h;
	int64_t timer_us;
	CdxListT elements;
	CdxListNodeT node;

	CdxListNodeT TT_node;
	int64_t time_stamp;

	int showing;
//	uint64_t timeout;
};

struct TAG_LayerS
{
	char *bg_image;
	int x;
	int y;
	int w;
	int h;
	CdxListT widgets;

};

struct TT_NodeS
{
	CdxListNodeT node;
	struct TAG_WidgetS *widget;
	int64_t time_stamp;
	int64_t duration_ms;
};

struct TinyUIS
{
//	AwPoolT *pool;
	const char *layout_xml;

	struct TAG_LayerS layer;

	ZMetadataT *metadata;
	IPaintT *paint;

	pthread_t TT_pid; /* timer task */
	CdxListT TT_list;
	pthread_mutex_t TT_lock;
	int TT_exit;
};

static AwPoolT *tui_pool = NULL;
int draw_thread_is_run = 0;

static int64_t getTime_US()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

static int xmlAttrGetInt(xmlNodePtr node, const char *attr, int def)
{
	xmlChar *attr_val;
	int ret = 0;

	attr_val = xmlGetProp(node, attr);
	if (attr_val)
	{
		ret = atoi(attr_val);
		xmlFree(attr_val);
	}
	else
	{
		ret = def;
	}
	return ret;
}

static uint32_t xmlAttrGetHex(xmlNodePtr node, const char *attr, uint32_t def)
{
	xmlChar *attr_val;
	uint32_t ret = 0;

	attr_val = xmlGetProp(node, attr);
	if (attr_val)
	{
		ret = (uint32_t)strtoll(attr_val, NULL, 16);
		xmlFree(attr_val);
	}
	else
	{
		ret = def;
	}
	return ret;
}

static char *xmlAttrGetString(xmlNodePtr node, const char *attr)
{
	xmlChar *attr_val;
	char *ret = NULL;

	attr_val = xmlGetProp(node, attr);
	if (attr_val)
	{
		ret = Palloc(tui_pool, xmlStrlen(attr_val) + 2);
		sprintf(ret, "%s", attr_val);
		xmlFree(attr_val);
	}

	return ret;
}

static int percent2SizeHorizontal(int percent)
{
	return (percent*TUI_SCREEN_W)/100;
}

static int percent2SizeVertical(int percent)
{
	return (percent*TUI_SCREEN_H)/100;
}

static struct TAG_TextS *parseTAG_Text(xmlNodePtr node)
{
	struct TAG_TextS *text = NULL;

	CDX_LOG_CHECK(xmlStrcasecmp(node->name, BAD_CAST TAG_TEXT) == 0, "xml invalid.");

	text = Palloc(tui_pool, sizeof(*text));
	memset(text, 0x00, sizeof(*text));

	text->value = xmlAttrGetString(node, ATTR_VALUE);
	text->color = xmlAttrGetHex(node, ATTR_COLOR, 0x0);
	text->font_size = xmlAttrGetInt(node, ATTR_FONT_SIZE, 11);
	text->align = 0;

	xmlChar *hpos = xmlAttrGetString(node, ATTR_HPOS);
	if (hpos)
	{
		if (xmlStrcasecmp(hpos, VAL_HPOS_LEFT) == 0)
		{
			text->align |= TEXT_HPOS_LEFT;
		}
		else if (xmlStrcasecmp(hpos, VAL_HPOS_RIGHT) == 0)
		{
			text->align |= TEXT_HPOS_RIGHT;
		}
		else /* default center. */
		{
		}
	}


	xmlChar *vpos = xmlAttrGetString(node, ATTR_VPOS);
	if (vpos)
	{
		if (xmlStrcasecmp(hpos, VAL_VPOS_TOP) == 0)
		{
			text->align |= TEXT_VPOS_TOP;
		}
		else if (xmlStrcasecmp(hpos, VAL_VPOS_BOTTOM) == 0)
		{
			text->align |= TEXT_VPOS_BOTTOM;
		}
		else /* default center. */
		{
		}
	}

	return text;
}

static struct TAG_ImageS *parseTAG_Image(xmlNodePtr node)
{
	struct TAG_ImageS *image;

	CDX_LOG_CHECK(xmlStrcasecmp(node->name, BAD_CAST TAG_IMAGE) == 0, "xml invalid.");

	image = Palloc(tui_pool, sizeof(*image));

	image->value = xmlAttrGetString(node, ATTR_VALUE);

	return image;
}

static struct TAG_ItemS *parseTAG_Item(xmlNodePtr node)
{
	struct TAG_ItemS *item= NULL;
	xmlNodePtr child;
	xmlChar *attr_type;
	xmlChar *attr_temp;

	CDX_LOG_CHECK(xmlStrcasecmp(node->name, BAD_CAST TAG_ITEM) == 0, "xml invalid.");

	item = Palloc(tui_pool, sizeof(*item));

	attr_type = xmlAttrGetString(node, ATTR_TYPE);
	CDX_LOG_CHECK(attr_type != NULL, "xml invalid.");

	if (xmlStrcasecmp(attr_type, VAL_TYPE_IMAGE) == 0)
	{
		item->type = ITEM_TYPE_IMAGE;
	}
	else if (xmlStrcasecmp(attr_type, VAL_TYPE_TEXT) == 0)
	{
		item->type = ITEM_TYPE_TEXT;
	}
	else if (xmlStrcasecmp(attr_type, VAL_TYPE_RECT) == 0)
	{
		item->type = ITEM_TYPE_RECT;
	}
	else
	{
		CDX_LOG_CHECK(0, "xml invalid.");
	}
	Pfree(tui_pool, attr_type);

	item->index = xmlAttrGetString(node, ATTR_INDEX);
	item->effect = xmlAttrGetString(node, ATTR_EFFECT);
	if (item->effect == NULL)
	{
		item->effect = def_val_static;
	}
	item->motion_step = xmlAttrGetInt(node, ATTR_MOTION_STEP, 500);

	CdxListInit(&item->contents);
	if (item->type == ITEM_TYPE_IMAGE) /* image */
	{
		attr_temp = xmlAttrGetString(node, ATTR_VALUE);
		if (attr_temp) /* value in item tag, must be only one entry. */
		{
			struct TAG_ImageS *image = Palloc(tui_pool, sizeof(*image));
			image->value = attr_temp;
			CdxListAddTail(&image->node, &item->contents);
		}
		else /* value in sub-item tag */
		{
			struct TAG_ImageS *image;
			int value_num = 0;

			child = node->children;
			while (child != NULL)
			{
				image = parseTAG_Image(child);
				CdxListAddTail(&image->node, &item->contents);
				value_num++;
				child = child->next;
			}
			CDX_LOG_CHECK(value_num>0, "invalid layout, check ");
		}
	}
	else if (item->type == ITEM_TYPE_TEXT)/* text */
	{
		attr_temp = xmlAttrGetString(node, ATTR_VALUE);
		if (attr_temp) /* value in item tag, must be only one entry. */
		{
			struct TAG_TextS *text = Palloc(tui_pool, sizeof(*text));
			text->value = attr_temp;
			text->color = xmlAttrGetHex(node, ATTR_COLOR, 0x0);
			text->font_size = xmlAttrGetInt(node, ATTR_FONT_SIZE, 11);

			text->align = 0;
			xmlChar *hpos = xmlAttrGetString(node, ATTR_HPOS);
			if (hpos)
			{
				if (xmlStrcasecmp(hpos, VAL_HPOS_LEFT) == 0)
				{
					text->align |= TEXT_HPOS_LEFT;
				}
				else if (xmlStrcasecmp(hpos, VAL_HPOS_RIGHT) == 0)
				{
					text->align |= TEXT_HPOS_RIGHT;
				}
				else /* default center. */
				{
				}
			}

			xmlChar *vpos = xmlAttrGetString(node, ATTR_VPOS);
			if (vpos)
			{
				if (xmlStrcasecmp(hpos, VAL_VPOS_TOP) == 0)
				{
					text->align |= TEXT_VPOS_TOP;
				}
				else if (xmlStrcasecmp(hpos, VAL_VPOS_BOTTOM) == 0)
				{
					text->align |= TEXT_VPOS_BOTTOM;
				}
				else /* default center. */
				{
				}
			}

			CdxListAddTail(&text->node, &item->contents);
		}
		else /* value in sub-item tag */
		{
			struct TAG_TextS *text;

			child = node->children;
			while (child != NULL)
			{
				text = parseTAG_Text(child);
				CdxListAddTail(&text->node, &item->contents);

				child = child->next;
			}

		}
	}
	else if (item->type == ITEM_TYPE_RECT)/* text */
	{
		struct TAG_RectS *rect = Palloc(tui_pool, sizeof(*rect));
		rect->color = xmlAttrGetHex(node, ATTR_COLOR, 0x0);

		CdxListAddTail(&rect->node, &item->contents);
	}

	return item;
}


static struct TAG_ElementS *parseTAG_Element(xmlNodePtr node)
{
	struct TAG_ItemS *item= NULL;
	struct TAG_ElementS *element = NULL;
	xmlNodePtr child;
	CDX_LOG_CHECK(xmlStrcasecmp(node->name, BAD_CAST TAG_ELEMENT) == 0, "xml invalid.");

	element = Palloc(tui_pool, sizeof(*element));

	element->name = xmlAttrGetString(node, ATTR_NAME);
	element->index = xmlAttrGetString(node, ATTR_INDEX);

	element->x = xmlAttrGetInt(node, ATTR_X, 0);
	element->x = percent2SizeHorizontal(element->x);

	element->y = xmlAttrGetInt(node, ATTR_Y, 0);
	element->y = percent2SizeVertical(element->y);

	element->w = xmlAttrGetInt(node, ATTR_W, 0);
	element->w = percent2SizeHorizontal(element->w);

	element->h = xmlAttrGetInt(node, ATTR_H, 0);
	element->h = percent2SizeVertical(element->h);


	CdxListInit(&element->items);

	child = node->children;
	while (child != NULL)
	{
		item = parseTAG_Item(child);
		CdxListAddTail(&item->node, &element->items);

		child = child->next;
	}

	return element;
}

static struct TAG_WidgetS *parseTAG_Widget(xmlNodePtr node)
{
	struct TAG_WidgetS *widget= NULL;
	struct TAG_ElementS *element = NULL;
	xmlNodePtr child;
	CDX_LOG_CHECK(xmlStrcasecmp(node->name, BAD_CAST TAG_WIDGET) == 0,
			"xml invalid. '%s'", node->name);

	widget = Palloc(tui_pool, sizeof(*widget));
	memset(widget, 0x00, sizeof(*widget));

	widget->name = xmlAttrGetString(node, ATTR_NAME);

	widget->x = xmlAttrGetInt(node, ATTR_X, 0);
	widget->x = percent2SizeHorizontal(widget->x);

	widget->y = xmlAttrGetInt(node, ATTR_Y, 0);
	widget->y = percent2SizeVertical(widget->y);

	widget->w = xmlAttrGetInt(node, ATTR_W, 0);
	widget->w = percent2SizeHorizontal(widget->w);

	widget->h = xmlAttrGetInt(node, ATTR_H, 0);
	widget->h = percent2SizeVertical(widget->h);


	widget->timer_us = xmlAttrGetInt(node, ATTR_TIMER, 0);
	widget->timer_us = widget->timer_us * 1000;

	CDX_LOGD("widget(%s) (%d, %d, %d, %d)", widget->name, widget->x, widget->y,
									   widget->w, widget->h);

	CdxListInit(&widget->elements);

	child = node->children;
	while (child != NULL)
	{
		element = parseTAG_Element(child);
		CdxListAddTail(&element->node, &widget->elements);

		child = child->next;
	}

	return widget;
}

static int parseTAG_Layer(struct TinyUIS *tui, xmlNodePtr node)
{
	xmlNodePtr child;
	struct TAG_WidgetS *widget;
	CDX_LOG_CHECK(xmlStrcasecmp(node->name, BAD_CAST TAG_LAYER) == 0, "xml invalid.");

	tui->layer.bg_image = xmlAttrGetString(node, ATTR_BG);

	tui->layer.x = xmlAttrGetInt(node, ATTR_X, 0);
	tui->layer.x = percent2SizeHorizontal(tui->layer.x);

	tui->layer.y = xmlAttrGetInt(node, ATTR_Y, 0);
	tui->layer.y = percent2SizeVertical(tui->layer.y);

	tui->layer.w = xmlAttrGetInt(node, ATTR_W, 0);
	tui->layer.w = percent2SizeHorizontal(tui->layer.w);

	tui->layer.h = xmlAttrGetInt(node, ATTR_H, 0);
	tui->layer.h = percent2SizeVertical(tui->layer.h);

	CDX_LOGD("layer (%d, %d, %d, %d)", tui->layer.x, tui->layer.y,
									   tui->layer.w, tui->layer.h);
	CdxListInit(&tui->layer.widgets);

	child = node->children;
	while (child != NULL)
	{
		if (child->type != XML_COMMENT_NODE)
		{
			widget = parseTAG_Widget(child);
			CdxListAddTail(&widget->node, &tui->layer.widgets);
		}
		child = child->next;
	}
	return 0;
}
static int parseLayoutXML(struct TinyUIS *tui, const char *layout_xml)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr node;
	xmlChar *attr_val;
	int ret = 0;

	xmlKeepBlanksDefault(0);
	doc = xmlReadFile(layout_xml, "UTF-8", 0);
	if (NULL == doc)
	{
		CDX_LOGE("open xml file err, '%s'", layout_xml);
		return -1;
	}

	node = xmlDocGetRootElement(doc); // read root node

	CDX_LOG_CHECK(NULL != node, "xml invalid.");

	parseTAG_Layer(tui, node);

out:

	if (doc)
	{
		xmlFreeDoc(doc);
		doc = NULL;
	}
	return ret;
}

char *tryMetedata(TinyUIT *tui, char *value)
{
	char *ret;
	/* index maybe value by metadata  */
	if (strncmp(METADATA_PREFIX, value, METADATA_PREFIX_LEN) == 0)
	{
		ret = ZMD_Get(tui->metadata, value);
	}
	else
	{
		ret = value;
	}

	CDX_LOG_CHECK(ret!=NULL, "invalid layout file.");

	return ret;
}

int drawItem(TinyUIT *tui, struct TAG_ElementS *element, struct TAG_ItemS *item)
{
	CDX_LOGD("------------> draw item, '%d'", item->type);
	if (strcmp(item->effect, VAL_STATIC)== 0)
	{
		if (item->type == ITEM_TYPE_IMAGE)
		{
			struct ImageElementS image_elem;
			struct TAG_ImageS *image = NULL;

			image = CdxContainerOf(item->contents.head, struct TAG_ImageS, node);

			image_elem.value = image->value;
			image_elem.x = element->x;
			image_elem.y = element->y;
			image_elem.w = element->w;
			image_elem.h = element->h;
			image_elem.task.type = TASK_DRAW_IMAGE;

			Paint_DrawImage(tui->paint, &image_elem);

		}
		else if (item->type == ITEM_TYPE_TEXT)
		{
			struct TextElementS text_elem;
			struct TAG_TextS *text = NULL;

			text = CdxContainerOf(item->contents.head, struct TAG_TextS, node);

			text_elem.value = text->value;
			text_elem.x = element->x;
			text_elem.y = element->y;
			text_elem.w = element->w;
			text_elem.h = element->h;
			text_elem.color = text->color;
			text_elem.align = text->align;
			text_elem.font_size = text->font_size;
			text_elem.task.type = TASK_DRAW_TEXT;

			Paint_DrawText(tui->paint, &text_elem);
		}
		else if (item->type == ITEM_TYPE_RECT)
		{
			struct RectElementS rect_elem;
			struct TAG_RectS *rect = NULL;

			rect = CdxContainerOf(item->contents.head, struct TAG_RectS, node);

			rect_elem.color = rect->color;
			rect_elem.x = element->x;
			rect_elem.y = element->y;
			rect_elem.w = element->w;
			rect_elem.h = element->h;
			rect_elem.task.type = TASK_DRAW_RECT;

			Paint_DrawRect(tui->paint, &rect_elem);
		}
		else
		{
			CDX_LOG_CHECK(0, "invalid layout.");
		}
	}
	else if (strcmp(item->effect, VAL_ONCE_MOTION))
	{
		CDX_LOG_CHECK(0, "not support now");
	}
	else if (strcmp(item->effect, VAL_CYCLE_MOTION))
	{
		CDX_LOG_CHECK(0, "not support now");
	}
	else
	{
		CDX_LOG_CHECK(0, "invalid layout.xml '%s'", item->effect);
	}
}

int showElement(TinyUIT *tui, struct TAG_ElementS *element)
{
	CDX_LOGD("------------> show element '%s'", element->name);
	if (element->index == NULL) /* NULL, will show head-node */
	{
		struct TAG_ItemS *item;
		item = CdxContainerOf(element->items.head, struct TAG_ItemS, node);
		drawItem(tui, element, item);
	}
	else
	{
		struct TAG_ItemS *item;
		char *index_val;
		int done = 0;

		/* index maybe value by metadata  */
		if (strncmp(METADATA_PREFIX, element->index, METADATA_PREFIX_LEN) == 0)
		{
			index_val = ZMD_Get(tui->metadata, element->index);
		}
		else
		{
			index_val = element->index;
		}

		CdxListForEachEntry(item, &element->items, node)
		{
			if (strcmp(index_val, element->index) == 0)
			{
				drawItem(tui, element, item);
				done = 1;
				break;
			}
		}
		CDX_LOG_CHECK(done == 1, "invalid, layout config, '%s'", element->index);
	}
	return 0;
}

struct DrawTaskS *newTaskFromItem(TinyUIT *tui, struct TAG_ElementS *element,
										struct TAG_ItemS *item)
{
//	CDX_LOGD("------------> draw item, '%d'", item->type);
	if (strcmp(item->effect, VAL_STATIC)== 0)
	{
		if (item->type == ITEM_TYPE_IMAGE)
		{
			struct ImageElementS *image_elem = malloc(sizeof(*image_elem));
			struct TAG_ImageS *image = NULL;

			image = CdxContainerOf(item->contents.head, struct TAG_ImageS, node);

			image_elem->value = image->value;
			image_elem->x = element->x;
			image_elem->y = element->y;
			image_elem->w = element->w;
			image_elem->h = element->h;
			image_elem->task.type = TASK_DRAW_IMAGE;

			return &image_elem->task;
		}
		else if (item->type == ITEM_TYPE_TEXT)
		{
			struct TextElementS *text_elem = malloc(sizeof(*text_elem));
			struct TAG_TextS *text = NULL;

			text = CdxContainerOf(item->contents.head, struct TAG_TextS, node);

			text_elem->value = tryMetedata(tui, text->value);

			text_elem->x = element->x;
			text_elem->y = element->y;
			text_elem->w = element->w;
			text_elem->h = element->h;
			text_elem->color = text->color;
			text_elem->align = text->align;
			text_elem->font_size = text->font_size;
			text_elem->task.type = TASK_DRAW_TEXT;

			return &text_elem->task;
		}
		else if (item->type == ITEM_TYPE_RECT)
		{
			struct RectElementS *rect_elem = malloc(sizeof(*rect_elem));
			struct TAG_RectS *rect = NULL;

			rect = CdxContainerOf(item->contents.head, struct TAG_RectS, node);

			rect_elem->x = element->x;
			rect_elem->y = element->y;
			rect_elem->w = element->w;
			rect_elem->h = element->h;
			rect_elem->color = rect->color;
			rect_elem->task.type = TASK_DRAW_RECT;

			return &rect_elem->task;
		}
		else
		{
			CDX_LOG_CHECK(0, "invalid layout.");
		}
	}
	else if (strcmp(item->effect, VAL_ONCE_MOTION))
	{
		CDX_LOG_CHECK(0, "not support now");
	}
	else if (strcmp(item->effect, VAL_CYCLE_MOTION))
	{
		CDX_LOG_CHECK(0, "not support now");
	}
	else
	{
		CDX_LOG_CHECK(0, "invalid layout.xml '%s'", item->effect);
	}
	return NULL;
}

struct DrawTaskS *getElementTask(TinyUIT *tui, struct TAG_ElementS *element)
{
	CDX_LOGD("------------> show element '%s'", element->name);
	struct DrawTaskS *dt = NULL;

	if (element->index == NULL) /* NULL, will show head-node */
	{
		struct TAG_ItemS *item;
		item = CdxContainerOf(element->items.head, struct TAG_ItemS, node);

		dt = newTaskFromItem(tui, element, item);
	}
	else
	{
		struct TAG_ItemS *item;
		char *index_val;
		int done = 0;

		/* index maybe value by metadata  */
		index_val = tryMetedata(tui, element->index);

		CdxListForEachEntry(item, &element->items, node)
		{
			if (strcmp(index_val, item->index) == 0)
			{
				dt = newTaskFromItem(tui, element, item);
				done = 1;
				break;
			}
		}
		CDX_LOG_CHECK(done == 1, "invalid, layout config, '%s'", element->index);
	}

	return dt;
}

static int showWidget(TinyUIT *tui, struct TAG_WidgetS *widget)
{
	CDX_LOGD("showWidget '%s' show.", widget->name);

	if (widget->showing)
	{
		CDX_LOGW("'%s' showing now.", widget->name);
//		return 0;
	}

	if (widget->timer_us)
	{
		pthread_mutex_lock(&tui->TT_lock);

		if (!widget->time_stamp) /* time_stamp = 0, means not in list */
		{
			CdxListAddTail(&widget->TT_node, &tui->TT_list);
		}
		widget->time_stamp = getTime_US();

		pthread_mutex_unlock(&tui->TT_lock);
	}

	struct TAG_ElementS *element;
	CdxListT task_list;
	CdxListInit(&task_list);

	struct DrawTaskS *dt = NULL;
	CdxListForEachEntry(element, &widget->elements, node)
	{
		dt = getElementTask(tui, element);
		CdxListAddTail(&dt->node, &task_list);
	}

	Paint_DrawTasks(tui->paint, &task_list);

	widget->showing = 1;

	return 0;
}

static int hideWidget(TinyUIT *tui, struct TAG_WidgetS *widget)
{
	if (!widget->showing)
	{
		CDX_LOGW("'%s' not show now.", widget->name);
		return 0;
	}

#if 0
	struct ClearElementS *elem = malloc(sizeof(*elem));

	elem->x = widget->x;
	elem->y = widget->y;
	elem->w = widget->w;
	elem->h = widget->h;
	elem->task.type = TASK_DRAW_CLEAR;

	CdxListT task_list;
	CdxListInit(&task_list);

	CdxListAddTail(&elem->task.node, &task_list);

	Paint_DrawTasks(tui->paint, &task_list);
#endif

	Paint_ClearRect(tui->paint, widget->x, widget->y, widget->w, widget->h);

	widget->showing = 0;

	return 0;
}

static void *__TT_Proc(void *param)
{
	TinyUIT *tui = param;

	while (!tui->TT_exit)
	{
		usleep(100000); /* 100ms */

		struct TAG_WidgetS *widget = NULL, *widget_next = NULL;
		pthread_mutex_lock(&tui->TT_lock);

		CdxListForEachEntrySafe(widget, widget_next, &tui->TT_list, TT_node)
		{
			int64_t time_now = getTime_US();
			if (time_now > (widget->time_stamp + widget->timer_us))
			{
				widget->time_stamp = 0;
				hideWidget(tui, widget);
				CdxListDel(&widget->TT_node);
			}
		}

		pthread_mutex_unlock(&tui->TT_lock);

	}
	return NULL;
}

int TUI_MetedataAdd(TinyUIT *tui, const char *key)
{
	return ZMD_Add(tui->metadata, key);
}

int TUI_MetedataSet(TinyUIT *tui, const char *key, const char *value)
{
	return ZMD_Set(tui->metadata, key, value);
}

int TUI_WidgetShow(TinyUIT *tui, const char *widget_name)
{
	struct TAG_WidgetS *widget;

	int done = 0;
	CDX_LOGD("show widget '%s' start", widget_name);
	CdxListForEachEntry(widget, &tui->layer.widgets, node)
	{
		if (strcmp(widget_name, widget->name) == 0)
		{
			showWidget(tui, widget);
			done = 1;
			break;
		}

	}

	CDX_LOG_CHECK(done == 1, "invalid, layout config, '%s'", widget_name);

	CDX_LOGD("show widget '%s' end", widget_name);

	return 0;
}

int TUI_WidgetHide(TinyUIT *tui, const char *widget_name)
{
	struct TAG_WidgetS *widget;

	int done = 0;
	CDX_LOGD("hide widget '%s' start", widget_name);
	CdxListForEachEntry(widget, &tui->layer.widgets, node)
	{
		if (strcmp(widget_name, widget->name) == 0)
		{
			hideWidget(tui, widget);
			done = 1;
			break;
		}

	}

	CDX_LOG_CHECK(done == 1, "invalid, layout config, '%s'", widget_name);

	CDX_LOGD("hide widget '%s' end", widget_name);

	return 0;
}


TinyUIT *TUI_Instance(const char *layout_xml)
{
    struct TinyUIS *tui;
    int ret;
    int time = 100;

	tui_pool = AwPoolCreate(NULL);
	tui = Palloc(tui_pool, sizeof(*tui));

	memset(tui, 0x00, sizeof(*tui));

	tui->metadata = ZMD_Instance();
	tui->paint = MNGPaint_Instance();
    if(tui->paint == NULL){
        TLOGE("tui->paint == NULL");
        return NULL;
    }

    while(!draw_thread_is_run && time){
        TLOGI("Waiting for draw thread run, time=%d", time);
        usleep(10*1000);
        time--;
    }

    if(time == 0){
        TLOGE("Waiting for draw thread run timeout");
        return NULL;
    }

	parseLayoutXML(tui, layout_xml);

	ret = Paint_Init(tui->paint, tui->layer.x, tui->layer.y, tui->layer.w, tui->layer.h);
	if (ret != 0)
	{
		CDX_LOGE("Render Init failure...");
		return NULL;
	}

	pthread_mutex_init(&tui->TT_lock, NULL);
	CdxListInit(&tui->TT_list);
	tui->TT_exit = 0;
	ret = pthread_create(&tui->TT_pid, NULL, __TT_Proc, tui);
	if (ret != 0)
	{
		CDX_LOGE("pthread_create failure...");
		return NULL;
	}

	return tui;
}

int TUI_Destroy(TinyUIT *tui)
{
	tui->TT_exit = 1;
	pthread_join(tui->TT_pid, NULL);

	Pfree(tui_pool, tui);
	AwPoolDestroy(tui_pool);
	tui_pool = NULL;
	return 0;
}

