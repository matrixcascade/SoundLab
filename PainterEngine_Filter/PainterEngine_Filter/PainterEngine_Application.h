#ifndef PAINTERENGINE_APPLICATION_H
#define PAINTERENGINE_APPLICATION_H
#include "Filter_Main.h"

//////////////////////////////////////////////////////////////////////////
//Configures
#define  PX_WINDOW_NAME     "PainterEngine Filter"

#define  PX_WINDOW_WIDTH    240
#define  PX_WINDOW_HEIGHT   180

//memorypool for runtime(bytes)
#define PX_MEMORY_UI_SIZE (1024*1024*2)
#define PX_MEMORY_RESOURCES_SIZE (1024*1024*2)
#define PX_MEMORY_GAME_SIZE (1024*1024*16)
//////////////////////////////////////////////////////////////////////////

typedef struct
{
	Filter_Main filterMain;
	PX_Instance Instance;
	px_shape shape_volume;
	px_texture tex_logo;
	PX_Object *ui_root,*btn_switch,*sliderbar_volume;
}PX_Application;

extern PX_Application App;

px_bool PX_ApplicationInitialize(PX_Application *App);
px_void PX_ApplicationUpdate(PX_Application *App,px_dword elpased);
px_void PX_ApplicationRender(PX_Application *App,px_dword elpased);
px_void PX_ApplicationPostEvent(PX_Application *App,PX_Object_Event e);

#endif
