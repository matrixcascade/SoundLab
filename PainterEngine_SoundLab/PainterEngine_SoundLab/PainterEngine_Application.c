#include "PainterEngine_Application.h"

PX_Application App;

#define  PX_SOUNDLAB_SAMPLE_WINDOW  1024
px_bool PX_ApplicationInitialize(PX_Application *App)
{
	if (!SoundLab_Startup_Initialize(&App->Instance,&App->startup))
	{
		return PX_FALSE;
	}

	if (!SoundLab_Main_Initialize(&App->Instance,&App->smain))
	{
		return PX_FALSE;
	}
	App->status=PX_SOUNDLAB_STATUS_STARTUP;

	
	
	return PX_TRUE;
}

px_void PX_ApplicationUpdate(PX_Application *App,px_dword elpased)
{
	switch(App->status)
	{
	case PX_SOUNDLAB_STATUS_STARTUP:
		{
			SoundLab_Startup_Update(&App->startup,elpased);
			if(App->startup.bend) App->status=PX_SOUNDLAB_STATUS_MAIN;
		}
		break;
	case PX_SOUNDLAB_STATUS_MAIN:
		{
			SoundLab_Main_Update(&App->smain,elpased);
		}
		break;
	}
}

px_void PX_ApplicationRender(PX_Application *App,px_dword elpased)
{
	PX_SurfaceClear(&App->Instance.runtime.RenderSurface,0,0,App->Instance.runtime.RenderSurface.width-1,App->Instance.runtime.RenderSurface.height-1,PX_COLOR(255,96,96,96));
	switch(App->status)
	{
	case PX_SOUNDLAB_STATUS_STARTUP:
		{
			SoundLab_Startup_Render(&App->startup,elpased);
		}
		break;
	case PX_SOUNDLAB_STATUS_MAIN:
		{
			SoundLab_Main_Render(&App->Instance.runtime.RenderSurface,&App->smain,elpased);
		}
		break;
	}
}

px_void PX_ApplicationPostEvent(PX_Application *App,PX_Object_Event e)
{
	switch(App->status)
	{
	case PX_SOUNDLAB_STATUS_STARTUP:
		{
			SoundLab_Startup_PostEvent(&App->startup,e);
		}
		break;
	case PX_SOUNDLAB_STATUS_MAIN:
		{
			SoundLab_Main_PostEvent(&App->smain,e);
		}
		break;
	}
}

