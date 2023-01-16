#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "ipaint.h"
#include <minigui/common.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/minigui.h>

#include <CdxTypes.h>
#include <CdxQueue.h>
#include <AwPool.h>
#include <cdx_log.h>
#include <tina_log.h>

//#undef CDX_LOGD
//#define CDX_LOGD TLOGD

#define MNG_FONT_SIZE_LIMIT 128

#define ARGB_A(argb) (argb>>24)
#define ARGB_R(argb) ((argb&0x00FF0000)>>16)
#define ARGB_G(argb) ((argb&0x0000FF00)>>8)
#define ARGB_B(argb) (argb&0x000000FF)

struct MNG_PaintImplS
{
	IPaintT base;

	pthread_t draw_tid;

    HWND mng_hwnd;

    PLOGFONT mng_font[MNG_FONT_SIZE_LIMIT];

	int x;
	int y;
	int w;
	int h;

	pthread_mutex_t lock;

	pthread_mutex_t cond_mutex;
	pthread_cond_t cond;

};

struct MNG_MessageS
{
	int event;
	void *param; /* struct ImageElementS/TextElementS/RectElementS */
	struct MNG_PaintImplS *handler;
};

CdxQueueT *mng_msg_queue = NULL;
AwPoolT *mng_pool = NULL;
extern int draw_thread_is_run;

static PLOGFONT MNG_GetFont(struct MNG_PaintImplS *impl, int font_size)
{
    CDX_LOG_CHECK(font_size > 0 && font_size <= MNG_FONT_SIZE_LIMIT,
    			"invalid size '%d'", font_size);

    if (!impl->mng_font[font_size])
    {
        impl->mng_font[font_size] = CreateLogFont("ttf", "fzltqh", "UTF-8",
                                   FONT_WEIGHT_BOOK, FONT_SLANT_ROMAN, FONT_FLIP_NIL,
                                   FONT_OTHER_AUTOSCALE, FONT_UNDERLINE_NONE,
                                   FONT_STRUCKOUT_NONE, font_size, 0);
    }

	CDX_LOGD("get font '%p'", impl->mng_font[font_size]);

	CDX_CHECK(impl->mng_font[font_size]);
    return impl->mng_font[font_size];
}

static int MNG_DrawImage(struct MNG_PaintImplS *impl, HDC hdc, struct DrawTaskS *dt)
{
	struct ImageElementS *elem;
    BITMAP bmp;
    int ret;
	elem = CdxContainerOf(dt, struct ImageElementS, task);

	CDX_LOGD("MNG-Draw-Image '%s' ", elem->value);


//	SetBrushColor(hdc, RGBA2Pixel(hdc, 0x00, 0x00, 0x00, 0x00));
//	FillBox(hdc, elem->x, elem->y, elem->w, elem->h);

    ret = LoadBitmapFromFile(HDC_SCREEN, &bmp, elem->value);

    CDX_LOGD("Loadfile '%d', img info[%d %d]", ret, bmp.bmWidth, bmp.bmHeight);

	ret = FillBoxWithBitmap(hdc, elem->x, elem->y, elem->w, elem->h, &bmp);
	CDX_LOGW("Fillbox '%d'", ret);

    if (bmp.bmBits != NULL)
    {
        UnloadBitmap(&bmp);
    }

    return 0;
}

static int MNG_DrawText(struct MNG_PaintImplS *impl, HDC hdc, struct DrawTaskS *dt)
{
	struct TextElementS *elem;

	elem = CdxContainerOf(dt, struct TextElementS, task);

	CDX_LOGD("MNG-Draw-Text '%s' color(0x%08x) size(%d) align(0x%x)", elem->value,
												elem->color, elem->font_size, elem->align);

    // 绘制文字
    PLOGFONT font = MNG_GetFont(impl, elem->font_size);
    SetBkMode(hdc, BM_TRANSPARENT); // 去掉文字背景
    SelectFont(hdc, font);
    SetTextColor(hdc, RGB2Pixel(HDC_SCREEN,
    		ARGB_R(elem->color), ARGB_G(elem->color), ARGB_B(elem->color)));
    		// 设置文字的颜色

    uint32_t format = 0;

    format = DT_SINGLELINE;

	if (elem->align & TEXT_HPOS_LEFT)
	{
		format |= DT_LEFT;
	}
	else if (elem->align & TEXT_HPOS_RIGHT)
	{
		format |= DT_RIGHT;
	}
	else
	{
		format |= DT_CENTER;
	}

	if (elem->align & TEXT_VPOS_TOP)
	{
		format |= DT_TOP;
	}
	else if (elem->align & TEXT_VPOS_BOTTOM)
	{
		format |= DT_BOTTOM;
	}
	else
	{
		format |= DT_VCENTER;
	}

    RECT rect;
    rect.left = elem->x;
    rect.top = elem->y;
    rect.right = elem->x + elem->w;
    rect.bottom = elem->y + elem->h;

    DrawText(hdc, elem->value, -1, &rect, format);


    return 0;
}

static int MNG_DrawRect(struct MNG_PaintImplS *impl, HDC hdc, struct DrawTaskS *dt)
{
	struct RectElementS *elem;
	int radius = 0; /* 圆角矩阵 */
	elem = CdxContainerOf(dt, struct RectElementS, task);

	CDX_LOGD("MNG-Draw-Rect '%d' color(0x%08x) size(%d, %d, %d, %d)", elem->type,
												elem->color, elem->x, elem->y, elem->w, elem->h);

    SetBrushColor(hdc,
    	RGBA2Pixel(HDC_SCREEN,
    		ARGB_A(elem->color), ARGB_R(elem->color), ARGB_G(elem->color), ARGB_B(elem->color)));
    		// 设置矩形的颜色

    if (radius)
    {
        RoundRect(hdc,
        	elem->x, elem->y, elem->x + elem->w, elem->y + elem->h,
        	1, 1);
    }
    else
    {
        FillBox(hdc, elem->x, elem->y, elem->w, elem->h);
    }

    return 0;
}

static int MNG_ClearRect(struct MNG_PaintImplS *impl, HDC hdc, struct DrawTaskS *dt)
{
	struct ClearElementS *elem;
    int ret = 0;
	elem = CdxContainerOf(dt, struct ClearElementS, task);

	CDX_LOGD("MNG-Clear ");

    SetBrushColor(hdc, RGBA2Pixel(hdc, 0x00, 0x00, 0x00, 0x00));

	FillBox(hdc, elem->x, elem->y, elem->w, elem->h);

    CDX_LOGD("Fillbox '%d', ", ret);

    return 0;
}

static LRESULT __MNG_WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case MSG_CREATE:
        {
            CDX_LOGD("msg create.");
            break;
        }
        case MSG_PAINT:
        {
            CDX_LOGD("+++++++++++++ msg paint.");
            struct DrawTaskS *dt = NULL;

			HDC hdc;
			hdc = BeginPaint(hWnd);

        	while (dt = (struct DrawTaskS *)CdxQueuePop(mng_msg_queue))
        	{

            	if (dt->type == TASK_DRAW_IMAGE)
            	{
					MNG_DrawImage(dt->cookie, hdc, dt);
            	}
            	else if (dt->type == TASK_DRAW_TEXT)
            	{
					MNG_DrawText(dt->cookie, hdc, dt);
            	}
            	else if (dt->type == TASK_DRAW_RECT)
            	{
					MNG_DrawRect(dt->cookie, hdc, dt);
            	}
            	else if (dt->type == TASK_DRAW_CLEAR)
            	{
            		MNG_ClearRect(dt->cookie, hdc, dt);
            	}
            	else
            	{
            		CDX_LOG_CHECK(0, "Program error.");
            	}

        	}

			EndPaint(hWnd, hdc);
			// TODO: should free task
            CDX_LOGD("+++++++++++++ msg paint end.");
            //break;
            return 0;
        }
        case MSG_ERASEBKGND:
        {
            CDX_LOGD("MSG_ERASEBKGND");
            return 0;
        }
        case MSG_COMMAND:
        {
            CDX_LOGD("MSG_COMMAND");
            return 0;
        }
        case MSG_CLOSE:
        {
            CDX_LOGD("msg close");
            DestroyMainWindow(hWnd);
            PostQuitMessage(hWnd);
            return 0;
        }
        default:
		{
//			CDX_LOGD("others '%x'", message);
			break;
		}
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

static void *__DrawThread(void *param)
{
	struct  MNG_PaintImplS* impl = param;
	MSG Msg;
	//HWND hMainWnd;
	MAINWINCREATE CreateInfo;

#ifdef _MGRM_PROCESSES
	JoinLayer(NAME_DEF_LAYER, "tinyui-mng", 0, 0);
#endif

	CreateInfo.dwStyle = WS_VISIBLE;
	CreateInfo.dwExStyle = WS_EX_NONE;
	CreateInfo.spCaption = "tinyui-mng";
	CreateInfo.hMenu = 0;
	CreateInfo.hCursor = 0;// GetSystemCursor(0);
	CreateInfo.hIcon = 0;
	CreateInfo.MainWindowProc = __MNG_WinProc;
	CreateInfo.lx = impl->x;
	CreateInfo.ty = impl->y;
	CreateInfo.rx = impl->x + impl->w;
	CreateInfo.by = impl->y + impl->h;
	CreateInfo.iBkColor = COLOR_lightwhite;// PIXEL_transparent;
	CreateInfo.dwAddData = 0;
	CreateInfo.hHosting = HWND_DESKTOP;

	impl->mng_hwnd = CreateMainWindow(&CreateInfo);

	if (impl->mng_hwnd == HWND_INVALID)
	{
        TLOGI("%s:%d: impl->mng_hwnd == HWND_INVALID\n", __func__, __LINE__);
		return NULL;
	}

	ShowWindow(impl->mng_hwnd, SW_SHOWNORMAL);
	ShowCursor(FALSE);
    draw_thread_is_run = 1;

	while (GetMessage(&Msg, impl->mng_hwnd))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	MainWindowThreadCleanup(impl->mng_hwnd);

	CDX_LOGD("exit mg task");
	return NULL;

}

static int __MNG_InitSurface(IPaintT *paint, int x, int y, int w,int h)
{
	struct MNG_PaintImplS *impl;
	impl = CdxContainerOf(paint, struct MNG_PaintImplS, base);

	/* single instance, init in Create/Instance */
	return 0;

#if 0
	impl->x = x;
	impl->y = y;
	impl->w = w;
	impl->h = h;

	pthread_create(&impl->draw_tid, NULL, __DrawThread, impl);

	return 0;
#endif
}

static int __MNG_DrawImage(IPaintT *paint, struct ImageElementS *elem)
{
	struct MNG_PaintImplS *impl;
	impl = CdxContainerOf(paint, struct MNG_PaintImplS, base);

	pthread_mutex_lock(&impl->lock);
	CDX_LOGD("NMG Draw Image");

	struct ImageElementS *msg_elem = malloc(sizeof(*msg_elem));

	memcpy(msg_elem, elem, sizeof(*msg_elem));

	CdxQueuePush(mng_msg_queue, &msg_elem->task);

    UpdateWindow(impl->mng_hwnd, TRUE);

	CDX_LOGD("NMG Draw Image end..");
	pthread_mutex_unlock(&impl->lock);

	return 0;
}

static int __MNG_DrawText(IPaintT *paint, struct TextElementS *elem)
{
	struct MNG_PaintImplS *impl;
	impl = CdxContainerOf(paint, struct MNG_PaintImplS, base);

	pthread_mutex_lock(&impl->lock);
	CDX_LOGD("NMG Draw Text");

	struct TextElementS *msg_elem = malloc(sizeof(*msg_elem));

	memcpy(msg_elem, elem, sizeof(*msg_elem));

	CdxQueuePush(mng_msg_queue, &msg_elem->task);

    UpdateWindow(impl->mng_hwnd, TRUE);
	CDX_LOGD("NMG Draw Text end");
	pthread_mutex_unlock(&impl->lock);

	return 0;
}


static int __MNG_DrawRect(IPaintT *paint, struct RectElementS *elem)
{
	struct MNG_PaintImplS *impl;
	impl = CdxContainerOf(paint, struct MNG_PaintImplS, base);

	pthread_mutex_lock(&impl->lock);
	CDX_LOGD("NMG Draw Rect");

	struct RectElementS *msg_elem = malloc(sizeof(*msg_elem));

	memcpy(msg_elem, elem, sizeof(*msg_elem));

	CdxQueuePush(mng_msg_queue, &msg_elem->task);

    UpdateWindow(impl->mng_hwnd, TRUE);

	CDX_LOGD("NMG Draw Rect end..");
	pthread_mutex_unlock(&impl->lock);

	return 0;
}

static int __MNG_DrawTasks(IPaintT *paint, CdxListT *task_list)
{
	struct MNG_PaintImplS *impl;
	impl = CdxContainerOf(paint, struct MNG_PaintImplS, base);

	pthread_mutex_lock(&impl->lock);
	CDX_LOGD("NMG Draw tasks");

//	ShowWindow(impl->mng_hwnd, SW_SHOW);

	struct DrawTaskS *dt;
	CdxListForEachEntry(dt, task_list, node)
	{
		dt->cookie = impl;
		CdxQueuePush(mng_msg_queue, dt);
	}

    UpdateWindow(impl->mng_hwnd, TRUE);
	CDX_LOGD("NMG Draw tasks end");
	pthread_mutex_unlock(&impl->lock);

	return 0;
}

static int __MNG_ClearRect(IPaintT *paint, int x, int y, int w,int h)
{
	struct MNG_PaintImplS *impl;
	impl = CdxContainerOf(paint, struct MNG_PaintImplS, base);


	CDX_LOGD("NMG clear rect (%d, %d, %d, %d)", x, y, w, h);
	struct ClearElementS *elem;

	elem = malloc(sizeof(*elem));
	elem->x = x;
	elem->y = y;
	elem->w = w;
	elem->h = h;

	elem->task.cookie = impl;
	elem->task.type = TASK_DRAW_CLEAR;

	pthread_mutex_lock(&impl->lock);

	CdxQueuePush(mng_msg_queue, &elem->task);

    UpdateWindow(impl->mng_hwnd, TRUE);

	pthread_mutex_unlock(&impl->lock);

	return 0;
}

static int __MNG_Destroy(IPaintT *paint)
{
	CDX_LOGD("Single instance not nedd destroy.");

	struct MNG_PaintImplS *impl;
	impl = CdxContainerOf(paint, struct MNG_PaintImplS, base);

	SendMessage(impl->mng_hwnd, MSG_CLOSE, 0, 0);

	/* we don't need release resource now... */
	return 0;
}

static struct IPaintOpsS mng_paint_ops =
{
	.init = __MNG_InitSurface,
	.draw_image = __MNG_DrawImage,
	.draw_text = __MNG_DrawText,
	.draw_rect = __MNG_DrawRect,
	.draw_tasks = __MNG_DrawTasks,
	.clear_rect = __MNG_ClearRect,
	.destroy = __MNG_Destroy
};

static pthread_mutex_t mngp_lock = PTHREAD_MUTEX_INITIALIZER;

IPaintT *MNGPaint_Instance()
{
	static struct MNG_PaintImplS *impl = NULL;

	pthread_mutex_lock(&mngp_lock);
	if (impl)
	{
		CDX_LOGD("Single instance.");
		goto out;
	}

	impl = malloc(sizeof(*impl));

	memset(impl, 0x00, sizeof(*impl));

	if (mng_pool == NULL)
	{
		mng_pool = AwPoolCreate(NULL);
	}

	if (mng_msg_queue == NULL)
	{
		mng_msg_queue = CdxQueueCreate(mng_pool);
	}

	impl->base.ops = mng_paint_ops;

	pthread_mutex_init(&impl->lock, NULL);

	impl->x = 0;
	impl->y = 0;
	impl->w = 1280;
	impl->h = 720;

	pthread_create(&impl->draw_tid, NULL, __DrawThread, impl);

out:
	pthread_mutex_unlock(&mngp_lock);
	return &impl->base;
}

