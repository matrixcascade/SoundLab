#ifndef FILTER_MAIN_H
#define FILTER_MAIN_H
#include "PainterEngine_Startup.h"
#include "Filter_Text.h"
#include <dsound.h>

#define PX_SOUNDLAB_WINDOW_WIDTH 1024
#define FILTERMAIN_MAX_BANKS 64

typedef struct  
{
	px_dword magic;//sinc
	px_double pitchshift;
	px_double filter[PX_SOUNDLAB_WINDOW_WIDTH];
}SOUNDLAB_FILTER_HEADER;

typedef struct
{
	px_double filter[PX_SOUNDLAB_WINDOW_WIDTH];
	px_double pitchshift;
	PX_ANN  ann;
}FilterMain_Filter;

typedef struct
{
	PX_Instance *Ins;
	PX_Tuning tuning;
	FilterMain_Filter banks[FILTERMAIN_MAX_BANKS];
	px_int activateCount;
	px_bool Activate;
	volatile  px_bool pitchAdapt;
	volatile  px_bool filterAdapt;
	volatile  int CaptureCacheSize;
	px_byte CaptureCache[PX_SOUNDLAB_WINDOW_WIDTH*2*8];
	PX_MFCC mfcc;
}Filter_Main;

px_bool PX_FilterMainInitialize(PX_Instance *Ins,Filter_Main *pfm);
px_void PX_FilterMainUpdate(Filter_Main *pfm,px_dword elpased);


#endif