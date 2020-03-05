#ifndef PAINTERENGINE_APPLICATION_H
#define PAINTERENGINE_APPLICATION_H
#include "PainterEngine_Startup.h"
#include "SoundLab_Startup.h"
#include "SoundLab_Main.h"
//////////////////////////////////////////////////////////////////////////
//Configures
#define  PX_WINDOW_NAME     "SoundLab"

#define  PX_WINDOW_WIDTH    1200
#define  PX_WINDOW_HEIGHT   800

//memorypool for runtime(bytes)
#define PX_MEMORY_UI_SIZE (1024*1024*32)
#define PX_MEMORY_RESOURCES_SIZE (1024*1024*2)
#define PX_MEMORY_GAME_SIZE (1024*1024*256)
//////////////////////////////////////////////////////////////////////////

typedef enum
{
	PX_SOUNDLAB_STATUS_STARTUP,
	PX_SOUNDLAB_STATUS_MAIN,
}PX_SOUNDLAB_STATUS;


typedef struct
{
	PX_SOUNDLAB_STATUS status;
	SoundLab_Startup startup;
	SoundLab_Main smain;
	PX_Instance Instance;
}PX_Application;

extern PX_Application App;

px_bool PX_ApplicationInitialize(PX_Application *App);
px_void PX_ApplicationUpdate(PX_Application *App,px_dword elpased);
px_void PX_ApplicationRender(PX_Application *App,px_dword elpased);
px_void PX_ApplicationPostEvent(PX_Application *App,PX_Object_Event e);

#endif
