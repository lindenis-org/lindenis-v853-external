#ifndef TINYUI_H
#define TINYUI_H

typedef struct TinyUIS TinyUIT;

#ifdef __cplusplus
extern "C"
{
#endif

int TUI_MetedataAdd(TinyUIT *tui, const char *key);

int TUI_MetedataSet(TinyUIT *tui, const char *key, const char *value);

int TUI_WidgetShow(TinyUIT *tui, const char *widget_name);

int TUI_WidgetHide(TinyUIT *tui, const char *widget_name);

TinyUIT *TUI_Instance(const char *layout_xml);

int TUI_Destroy(TinyUIT *tui);

#ifdef __cplusplus
}
#endif

#endif
