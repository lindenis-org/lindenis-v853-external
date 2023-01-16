#ifndef I_PAINT_H
#define I_PAINT_H
#include <stdint.h>
#include <CdxList.h>

#define TASK_DRAW_IMAGE 0x0801
#define TASK_DRAW_TEXT  0x0802
#define TASK_DRAW_RECT  0x0803
#define TASK_DRAW_CLEAR 0x0804

#define TEXT_HPOS_LEFT 		0x01
#define TEXT_HPOS_CENTER 	0x00
#define TEXT_HPOS_RIGHT 	0x02

#define TEXT_VPOS_TOP		0x10
#define TEXT_VPOS_CENTER 	0x00
#define TEXT_VPOS_BOTTOM	0x20


typedef struct IPaintS IPaintT;
//typedef void * SurfaceHandleT;

struct DrawTaskS
{
	CdxListNodeT node;
	int type;
	void *cookie;
};

struct ImageElementS
{
	char *value;
	int x;
	int y;
	int w;
	int h;

	struct DrawTaskS task;
};

struct TextElementS
{
	char *value;
	uint32_t color;
	uint32_t align;
	int font_size;
	int x;
	int y;
	int w;
	int h;

	struct DrawTaskS task;
};

struct RectElementS
{
	int type;
	uint32_t color;
	int x;
	int y;
	int w;
	int h;

	struct DrawTaskS task;
};

struct ClearElementS
{
	int x;
	int y;
	int w;
	int h;

	struct DrawTaskS task;
};

struct IPaintOpsS
{
	int (*init)(IPaintT *, int /*x*/, int /*y*/, int /*w*/,int /*h*/);

	int (*draw_image)(IPaintT *, struct ImageElementS *);

	int (*draw_text)(IPaintT *, struct TextElementS *);

	int (*draw_rect)(IPaintT *, struct RectElementS *);

	int (*clear_rect)(IPaintT *, int /*x*/, int /*y*/, int /*w*/,int /*h*/);

	int (*draw_tasks)(IPaintT *, CdxListT *);

	int (*destroy)(IPaintT *);
};


struct IPaintS
{
	struct IPaintOpsS ops;
};

#ifdef __cpluaplua
extern "C"
{
#endif

static inline int Paint_Init(IPaintT *paint, int x, int y, int w,int h)
{
	return paint->ops.init(paint, x, y, w, h);
}

static inline int Paint_DrawImage(IPaintT *paint, struct ImageElementS *elem)
{
	return paint->ops.draw_image(paint, elem);
}

static inline int Paint_DrawText(IPaintT *paint, struct TextElementS *elem)
{
	return paint->ops.draw_text(paint, elem);
}

static inline int Paint_DrawRect(IPaintT *paint, struct RectElementS *elem)
{
	return paint->ops.draw_rect(paint, elem);
}

static inline int Paint_DrawTasks(IPaintT *paint, CdxListT *task_list)
{
	return paint->ops.draw_tasks(paint, task_list);
}

static inline int Paint_ClearRect(IPaintT *paint, int x, int y, int w,int h)
{
	return paint->ops.clear_rect(paint, x, y, w, h);
}

static inline int Paint_Destroy(IPaintT *paint)
{
	return paint->ops.destroy(paint);
}


/* some instance here. */

/* miniGUI instance */
IPaintT *MNGPaint_Instance();

#ifdef __cpluaplua
}
#endif

#endif

