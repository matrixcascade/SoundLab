#include "SoundLab_Startup.h"

px_bool SoundLab_Startup_Initialize(PX_Instance *pIns,SoundLab_Startup *startup)
{
	startup->elpased=0;
	startup->instance=pIns;
	startup->bend=PX_FALSE;
	if(!PX_LoadTextureFromFile(&pIns->runtime.mp_resources,&startup->logo,SOUNDLAB_PATH_LOGO))return PX_FALSE;
	return PX_TRUE;
}

px_void SoundLab_Startup_Update(SoundLab_Startup *startup,px_dword elpased)
{
	startup->elpased+=elpased;
	if (startup->elpased>6000)
	{
		//switch
		startup->bend=PX_TRUE;
		startup->elpased=0;
	}
}

px_void SoundLab_Startup_Render(SoundLab_Startup *startup,px_dword elpased) 
{
	PX_TEXTURERENDER_BLEND blend;
	px_int Threshold;
 	px_texture *prenderTarget=&startup->instance->runtime.RenderSurface;
	
	Threshold=startup->elpased%2000;
	if(Threshold>1000) Threshold=2000-Threshold;

	blend.alpha=1.0f;
	blend.hdr_R=0.75f+(Threshold)/4000.0f;
	blend.hdr_G=0.75f+(Threshold)/4000.0f;
	blend.hdr_B=0.75f+(Threshold)/4000.0f;

	PX_TextureRender(prenderTarget,&startup->logo,prenderTarget->width/2,prenderTarget->height/2,PX_TEXTURERENDER_REFPOINT_CENTER,&blend);
	PX_FontDrawText(prenderTarget,startup->instance->runtime.width-10,startup->instance->runtime.height-24,"Code By DBinary all rights reserved.(matrixcascade@gmail.com)",PX_COLOR(255,128,255,64),PX_FONT_ALIGN_XRIGHT);
}

px_void SoundLab_Startup_PostEvent(SoundLab_Startup *startup,PX_Object_Event e)
{
	if (e.Event==PX_OBJECT_EVENT_CURSORCLICK)
	{
		startup->bend=PX_TRUE;
	}
}
