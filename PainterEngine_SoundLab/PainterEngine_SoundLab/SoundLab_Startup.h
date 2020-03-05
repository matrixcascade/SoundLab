#ifndef SOUNDLAB_STARTUP_H
#define SOUNDLAB_STARTUP_H

#include "PainterEngine_Startup.h"

typedef struct
{
	PX_Instance *instance;
	px_texture logo;
	px_dword elpased;
	px_bool bend;
}SoundLab_Startup;


px_bool SoundLab_Startup_Initialize(PX_Instance *pIns,SoundLab_Startup *startup);
px_void SoundLab_Startup_Update(SoundLab_Startup *startup,px_dword elpased);
px_void SoundLab_Startup_Render(SoundLab_Startup *startup,px_dword elpased);
px_void SoundLab_Startup_PostEvent(SoundLab_Startup *startup,PX_Object_Event e);

#endif