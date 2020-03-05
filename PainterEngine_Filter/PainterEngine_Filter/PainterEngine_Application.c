#include "PainterEngine_Application.h"

PX_Application App;

DWORD WINAPI PX_ApplicationOnSync(void *ptr)
{
	PX_Application *App=(PX_Application *)ptr;
	DWORD time,elpased;
	DWORD lastUpdateTime=timeGetTime();
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);

	while (PX_TRUE)
	{
		time=timeGetTime();
		elpased=time-lastUpdateTime;
		lastUpdateTime=time;
		PX_FilterMainUpdate(&App->filterMain,elpased);
	}


}

px_void PX_ApplicationOnVolumeChange(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	px_int vol;
	PX_Application *App=(PX_Application *)ptr;

	vol=PX_Object_SliderBarGetValue(App->sliderbar_volume);

	PX_AudioSetVolume(vol*10-1000);
}

px_bool PX_ApplicationInitialize(PX_Application *App)
{
	DWORD threadId;
	if(!PX_LoadShapeFromFile(&App->Instance.runtime.mp_resources,&App->shape_volume,FILTER_PATH_VOLUME)) return PX_FALSE;
	if(!PX_LoadTextureFromFile(&App->Instance.runtime.mp_resources,&App->tex_logo,FILTER_PATH_LOGO)) return PX_FALSE;
	App->ui_root=PX_ObjectCreate(&App->Instance.runtime.mp_ui,PX_NULL,0,0,0,0,0,0);
	if(!App->ui_root) return PX_FALSE;

	App->btn_switch=PX_Object_PushButtonCreate(&App->Instance.runtime.mp_ui,App->ui_root,PX_WINDOW_WIDTH/2-48,PX_WINDOW_HEIGHT/2-48-12,96,96,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(App->btn_switch,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(App->btn_switch,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetPushColor(App->btn_switch,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBackgroundColor(App->btn_switch,PX_COLOR(0,0,0,0));

	App->sliderbar_volume=PX_Object_SliderBarCreate(&App->Instance.runtime.mp_ui,App->ui_root,48,PX_WINDOW_HEIGHT-32,PX_WINDOW_WIDTH-64,24,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
	PX_Object_SliderBarSetBackgroundColor(App->sliderbar_volume,PX_COLOR(255,0,0,0));
	PX_Object_SliderBarSetColor(App->sliderbar_volume,PX_COLOR(255,0,255,0));
	PX_Object_SliderBarSetSliderButtonLength(App->sliderbar_volume,32);
	PX_Object_SliderBarSetMax(App->sliderbar_volume,100);
	PX_Object_SliderBarSetValue(App->sliderbar_volume,100);
	PX_Object_GetSliderBar(App->sliderbar_volume)->lastValue=100;
	PX_ObjectRegisterEvent(App->sliderbar_volume,PX_OBJECT_EVENT_VALUECHAGE,PX_ApplicationOnVolumeChange,App);

	
	if(!PX_FilterMainInitialize(&App->Instance,&App->filterMain)) return PX_FALSE;
	
	CreateThread(NULL, 0, PX_ApplicationOnSync, App, 0, &threadId);

	return PX_TRUE;
}

px_void PX_ApplicationUpdate(PX_Application *App,px_dword elpased)
{
	
}

px_void PX_ApplicationRender(PX_Application *App,px_dword elpased)
{
	px_char content[64];
	PX_SurfaceClear(&App->Instance.runtime.RenderSurface,0,0,App->Instance.runtime.width-1,App->Instance.runtime.height-1,PX_COLOR(255,0,0,0));
	PX_ObjectRender(&App->Instance.runtime.RenderSurface,App->ui_root,elpased);
	PX_ShapeRender(&App->Instance.runtime.RenderSurface,&App->shape_volume,24,PX_WINDOW_HEIGHT-20,PX_TEXTURERENDER_REFPOINT_CENTER,PX_COLOR(255,0,255,0));
	PX_TextureRender(&App->Instance.runtime.RenderSurface,&App->tex_logo,PX_WINDOW_WIDTH/2,PX_WINDOW_HEIGHT/2-12,PX_TEXTURERENDER_REFPOINT_CENTER,PX_NULL);

	PX_sprintf1(content,sizeof(content),"Tuning with %1 filters",PX_STRINGFORMAT_INT(App->filterMain.activateCount));
	PX_FontDrawText(&App->Instance.runtime.RenderSurface,PX_WINDOW_WIDTH/2,5,content,PX_COLOR(255,0,255,0),PX_FONT_ALIGN_XCENTER);
	
}

px_void PX_ApplicationPostEvent(PX_Application *App,PX_Object_Event e)
{
	PX_ObjectPostEvent(App->ui_root,e);
}

