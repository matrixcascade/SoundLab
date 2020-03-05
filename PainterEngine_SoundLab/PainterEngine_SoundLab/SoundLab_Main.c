#include "SoundLab_Main.h"

#define SOUNDLAB_DEFAULT_FRAME_COLOR 255,96,192,255

static px_char TuningRuntime[1024*1024*8];

DWORD WINAPI SoundLab_Main_ApplyTuning(LPVOID ptr)
{
	px_double zero0=0;
	px_int offset,i;
	px_double pitchshift,timeshift;
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	PX_Tuning Tuning1;
	PX_Tuning Tuning2;
	px_short *pDataU16;
	px_double Frame1[PX_SOUNDLAB_WINDOW_WIDTH*8];
	px_double Frame2[PX_SOUNDLAB_WINDOW_WIDTH*8];
	px_double Frame[PX_SOUNDLAB_WINDOW_WIDTH*8];
	px_double filter[PX_SOUNDLAB_WINDOW_WIDTH];

	px_int FrameSize;
	px_memory solveData;
	offset=0;
	pMain->schedule=0;

	if (pMain->sourcePCMSize)
	{
		px_memorypool mp;

		mp=MP_Create(TuningRuntime,sizeof(TuningRuntime));

		PX_MemoryInit(&pMain->Ins->runtime.mp_game,&solveData);

		pitchshift=(PX_Object_SliderBarGetValue(pMain->SliderBar_PitchShift)-50.0)/100;
		if(pitchshift==0)
		{
			pitchshift=1;
		}
		else if (pitchshift>0)
		{
			pitchshift=1+pitchshift/5*(2.75)*10;
		}
		else
		{
			pitchshift=-pitchshift/5*(2.75)*10;
			pitchshift=1/(1+pitchshift);
		}

		timeshift=(PX_Object_SliderBarGetValue(pMain->SliderBar_TimeScale)-50.0)/100;
		if(timeshift==0)
		{
			timeshift=1;
		}
		else if (timeshift>0)
		{
			timeshift=1+timeshift/5*(2.75)*10;
		}
		else
		{
			timeshift=-timeshift/5*(2.75)*10;
			timeshift=1/(1+timeshift);
		}
		//filter
		for (i=0;i<PX_COUNTOF(filter);i++)
		{
			filter[i]=0;
		}
		PX_Object_FilterEditorMapData(pMain->FilterEditor,filter,200);

		PX_TuningInitialize(&mp,&Tuning1,pitchshift,timeshift,PX_NULL,filter,PX_NULL,PX_TUNING_WINDOW_SIZE_1024);
		PX_TuningInitialize(&mp,&Tuning2,pitchshift,timeshift,PX_NULL,filter,PX_NULL,PX_TUNING_WINDOW_SIZE_1024);
	

		for (i=0;i<512*3;i++)
		{
			PX_MemoryCat(&solveData,&zero0,sizeof(px_double));
		}

		while (offset<pMain->sourcePCMSize-PX_SOUNDLAB_WINDOW_WIDTH*2*2*2)
		{
			pDataU16=(px_short *)(pMain->sourcePCM+offset);
			pMain->schedule=offset*1.0/pMain->sourcePCMSize*100;
			//channel 1
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i]=pDataU16[i*2]/32768.0;
			}

			FrameSize=PX_TuningFilter(&Tuning1,Frame,PX_SOUNDLAB_WINDOW_WIDTH,Frame1);

			//channel 2
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i]=pDataU16[i*2+1]/32768.0;
			}
			PX_TuningFilter(&Tuning2,Frame,PX_SOUNDLAB_WINDOW_WIDTH,Frame2);

			for (i=0;i<FrameSize;i++)
			{
				if (PX_ABS(Frame1[i])<1)
				{
					Frame1[i]*=32768.0;
				}
				else
				{
					if (Frame1[i]>0)
					{
						Frame1[i]=32768.0;
					}
					else
					{
						Frame1[i]=-32767.0;
					}
				}
				
				if (PX_ABS(Frame2[i])<1)
				{
					Frame2[i]*=32768.0;
				}
				else
				{
					if (Frame2[i]>0)
					{
						Frame2[i]=32768;
					}
					else
					{
						Frame2[i]=-32767;
					}
				}
			}


			//blend
			for (i=0;i<FrameSize;i++)
			{
				px_short val;
				val=(px_short)Frame1[i];
				PX_MemoryCat(&solveData,&val,sizeof(px_short));
				val=(px_short)Frame2[i];
				PX_MemoryCat(&solveData,&val,sizeof(px_short));
			}
			offset+=PX_SOUNDLAB_WINDOW_WIDTH*2*2;
		}


		PX_SoundStaticDataFree(&pMain->SoundData);
		pMain->SoundData.buffer=solveData.buffer;
		pMain->SoundData.mp=solveData.mp;
		pMain->SoundData.size=solveData.usedsize;
		pMain->SoundData.channel=PX_SOUND_CHANNEL_DOUBLE;
	}
	
	pMain->bDone=PX_TRUE;
	return 0;
}

DWORD WINAPI SoundLab_Main_InitializeSoundData(LPVOID ptr)
{
	px_int i,w,x,y;
	px_int max=1;
	px_int min=-1;

	px_double pmax=0;
	px_complex frame[PX_SOUNDLAB_WINDOW_WIDTH];
	px_double window[PX_SOUNDLAB_WINDOW_WIDTH];

	SoundLab_Main *pMain=(SoundLab_Main *)ptr;

	if (pMain->SoundData.size)
	{
		px_short *PCM16=(px_short *)pMain->SoundData.buffer;
		for (i=0;i<pMain->SoundData.size/2;i+=2)
		{
			if (PCM16[i]>max)
			{
				max=PCM16[i];
			}
			if (PCM16[i]<min)
			{
				min=PCM16[i];
			}
		}
		pMain->wave_min=min;
		pMain->wave_max=max;

		//Build Spectrum
		if (pMain->tex_Spectrum.MP)
		{
			PX_TextureFree(&pMain->tex_Spectrum);
		}

		w=pMain->SoundData.size/2/2/PX_SOUNDLAB_WINDOW_WIDTH*4;//2 bytes 2 channels 75% overlap
		pMain->process_memories=w*PX_SOUNDLAB_WINDOW_WIDTH/2*4;
		pMain->lastError[0]=0;
		pMain->schedule=0;

		if(!PX_TextureCreate(&pMain->Ins->runtime.mp_game,&pMain->tex_Spectrum,w,PX_SOUNDLAB_WINDOW_WIDTH/2)) 
		{
			PX_strset(pMain->lastError,"Out Of Memory.");
			pMain->bDone=PX_TRUE;
			return 0;
		}
		PX_WindowFunction_hamming(window,PX_SOUNDLAB_WINDOW_WIDTH);

		for (x=0;x<w;x++)
		{
			//Data Sampling 
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				frame[i].re=PCM16[x*2*PX_SOUNDLAB_WINDOW_WIDTH/4+i*2];//2bytes
				frame[i].im=0;
				frame[i].re*=window[i];
			}
			PX_FFT(frame,frame,PX_SOUNDLAB_WINDOW_WIDTH);
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				if (frame[i].re*frame[i].re+frame[i].im*frame[i].im>pmax)
				{
					pmax=frame[i].re*frame[i].re+frame[i].im*frame[i].im;
				}
			}
			pMain->schedule=1.0*x/w/2*100;
		}
		pMain->MaxPower=PX_sqrtd(pmax);
		
		pmax=PX_log(pmax+1);
		
		
		for (x=0;x<w;x++)
		{
			px_double unit;
			px_color pixel;

			//Data Sampling 
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				frame[i].re=PCM16[x*2*PX_SOUNDLAB_WINDOW_WIDTH/4+i*2];//2bytes
				frame[i].im=0;
				frame[i].re*=window[i];
			}
			//FFT
			PX_FFT(frame,frame,PX_SOUNDLAB_WINDOW_WIDTH);

			for (y=0;y<PX_SOUNDLAB_WINDOW_WIDTH/2;y++)
			{
				
				unit=PX_log(frame[y].re*frame[y].re+frame[y].im*frame[y].im+1)/pmax;
				if (unit<pMain->PowerThreshold)
				{
					unit=0;
				}
				pixel._argb.a=(px_uchar)(64+191*unit);
				pixel._argb.g=(px_uchar)(255*unit);
				pixel._argb.r=(px_uchar)(255*unit);
				pixel._argb.b=(px_uchar)(255-pixel._argb.r);
				PX_SurfaceDrawPixel(&pMain->tex_Spectrum,x,pMain->tex_Spectrum.height-y-1,pixel);
			}
			pMain->schedule=1.0*x/w/2*100+50;
		}
	}

	pMain->schedule=100;

	PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Cepstrum,55.125);
	PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Cepstrum,PX_SOUNDLAB_WINDOW_WIDTH/2);
	pMain->bDone=PX_TRUE;
	return 1;
}

px_void SoundLab_Main_Pause(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);
	pMain->status=SOUNDLAB_MAIN_STATUS_STOP;
}

px_void SoundLab_Main_Reset(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);
	pMain->status=SOUNDLAB_MAIN_STATUS_STOP;
	pMain->Ins->soundplay.Sounds[0].offset=0;
	pMain->offset=0;
}

px_void SoundLab_Main_Play(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	PX_Sound sound;
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	if (!pMain->SoundData.buffer)
	{
		return;
	}
	if (pMain->Ins->soundplay.Sounds[0].data)
	{
		PX_SoundPlayPause(&pMain->Ins->soundplay,PX_FALSE);
	}
	else
	{
		sound=PX_SoundCreate(&pMain->SoundData,PX_TRUE);
		pMain->Ins->soundplay.Sounds[PX_SOUNDLAB_SOUNDANALYSIS_INDEX]=sound;
		PX_SoundPlayPause(&pMain->Ins->soundplay,PX_FALSE);
		pMain->Ins->soundplay.Sounds[0].offset=pMain->offset;
	}
	pMain->status=SOUNDLAB_MAIN_STATUS_PLAY;
}

px_void SoundLab_Main_OnButtonSTFT(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_STFT;
}

px_void SoundLab_Main_OnButtonSpectrum(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_SPECTRUM;
}

px_void SoundLab_Main_OnButtonCepstrum(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_CEPSTRUM;
}

px_void SoundLab_Main_OnButtonAdapt(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_ADAPT;
}



px_void SoundLab_Main_OnButtonANN(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;

	if (PX_AudioCaptureOpen(GUID_NULL,1))
	{
		pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_ANN;
		PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);
		pMain->status=SOUNDLAB_MAIN_STATUS_ANN;
		pMain->Ins->soundplay.Sounds[0].offset=0;
		pMain->offset=0;
		pMain->mfccsamplecount=0;
		pMain->bTrainRun=PX_FALSE;
		pMain->AnnSucceeded=PX_FALSE;
		pMain->AnnEpoch=0;
		pMain->EnterTestMode=PX_FALSE;
	}
	else
	{
		PX_MessageBoxAlertOk(&pMain->messagebox,(const char *)L"Could not open microphone.");
	}

}

px_void SoundLab_Main_OnButtonRecord(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;

	if (PX_AudioCaptureOpen(GUID_NULL,1))
	{
		pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_RECORDING;
		PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);
		pMain->status=SOUNDLAB_MAIN_STATUS_RECORDING;
		pMain->Ins->soundplay.Sounds[0].offset=0;
		pMain->offset=0;
		pMain->recorderWCursorByte=0;

		PX_ObjectSetVisible(pMain->btn_Play,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_ann,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_Pause,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_stft,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_cepstrum,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_reset,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_OpenFile,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_spectrum,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_adapt,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_Record,PX_FALSE);
		PX_ObjectSetVisible(pMain->btn_RecordFinish,PX_TRUE);
		PX_ObjectSetVisible(pMain->btn_save,PX_FALSE);
	}
	else
	{
		PX_MessageBoxAlertOk(&pMain->messagebox,(const char *)L"Could not open microphone.");
	}
}


px_void SoundLab_Main_RecordFinish(SoundLab_Main *pMain)
{
	DWORD id;
	px_int i;
	px_short *pRecordData=(px_short *)pMain->recordCache;
	px_short *pTarget;

	PX_AudioCaptureClose();
	PX_ObjectSetVisible(pMain->btn_stft,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_cepstrum,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_reset,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_OpenFile,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_spectrum,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_adapt,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_Record,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_Play,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_ann,PX_TRUE);
	PX_ObjectSetVisible(pMain->btn_RecordFinish,PX_FALSE);
	PX_ObjectSetVisible(pMain->btn_save,PX_TRUE);

	pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_STFT;


	
	PX_SoundPlayClear(&pMain->Ins->soundplay);


	if (pMain->SoundData.mp)
	{
		PX_SoundStaticDataFree(&pMain->SoundData);
	}

	pMain->SoundData.mp=&pMain->Ins->runtime.mp_game;
	pMain->SoundData.channel=PX_SOUND_CHANNEL_DOUBLE;
	pMain->SoundData.buffer=(px_byte *)MP_Malloc(pMain->SoundData.mp,pMain->recorderWCursorByte*2);
	
	if (!pMain->SoundData.buffer)
	{
		MessageBox(PX_GetWindowHwnd(),"Out of memories","Error",MB_OK);
		return;
	}

	pMain->SoundData.size=pMain->recorderWCursorByte*2;

	pTarget=(px_short *)pMain->SoundData.buffer;

	for (i=0;i<pMain->SoundData.size/2/2;i++)
	{
		pTarget[i*2]=pRecordData[i];
		pTarget[i*2+1]=pRecordData[i];
	}

	if (pMain->sourcePCM)
	{
		free(pMain->sourcePCM);
	}
	pMain->sourcePCM=(px_byte *)malloc(pMain->SoundData.size);

	if (!pMain->sourcePCM)
	{
		MessageBox(PX_GetWindowHwnd(),"Out of memories","Error",MB_OK);
		PX_SoundStaticDataFree(&pMain->SoundData);
		return;
	}

	PX_memcpy(pMain->sourcePCM,pMain->SoundData.buffer,pMain->SoundData.size);
	pMain->sourcePCMSize=pMain->SoundData.size;

	
	PX_strset(pMain->FilePath,"Record PCM Data");

	pMain->bDone=PX_FALSE;
	pMain->offset=0;
	pMain->Ins->soundplay.Sounds[0].offset=0;
	PX_MessageBoxAlertOk(&pMain->messagebox,pMain->MessageText);
	PX_ObjectSetVisible(pMain->messagebox.btn_Ok,PX_FALSE);

	CreateThread(NULL, 0, SoundLab_Main_InitializeSoundData, pMain, 0, &id);


	pMain->status=SOUNDLAB_MAIN_STATUS_ANALYSISING;
	PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);



}
px_void SoundLab_Main_OnButtonRecordFinish(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	SoundLab_Main_RecordFinish(pMain);
}

px_void SoundLab_Main_OnButtonSave(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	SOUNDLAB_FILTER_HEADER header;
	px_double PitchShift;
	px_char *filePath;
	px_byte *memData;

	px_int memsize,wsize;

	PX_memset(&header,0,sizeof(header));

	PX_memcpy(&header.magic,"sinc",4);

	PX_Object_FilterEditorMapData(pMain->FilterEditor,header.filter,200);


	PitchShift=(PX_Object_SliderBarGetValue(pMain->SliderBar_PitchShift)-50.0)/100;

	if(PitchShift==0)
	{
		PitchShift=1;
	}
	else if (PitchShift>0)
	{
		PitchShift=1+PitchShift/5*(2.75)*10;
	}
	else
	{
		PitchShift=-PitchShift/5*(2.75)*10;
		PitchShift=1/(1+PitchShift);
	}
	header.pitchshift=PitchShift;
	if(!PX_ANNExport(&pMain->ann,PX_NULL,&memsize)) return;
	memsize+=sizeof(header);

	memData=(px_byte *)MP_Malloc(&pMain->Ins->runtime.mp_game,memsize);


	filePath=PX_SaveFileDialog("Filter File(*.Filter)\0*.filter\0");
	if (filePath)
	{
		FILE *pf=fopen(filePath,"wb");
		if (!pf)
		{
			goto _ERROR;
		}
		PX_memcpy(memData,&header,sizeof(header));
		if(!PX_ANNExport(&pMain->ann,memData+sizeof(header),&wsize)) goto _ERROR;
		
		fwrite(memData,1,memsize,pf);
		fclose(pf);
	}
	else
	{
		MP_Free(&pMain->Ins->runtime.mp_game,memData);
		return;
	}
	MP_Free(&pMain->Ins->runtime.mp_game,memData);
	PX_MessageBoxAlertOk(&pMain->messagebox,(const px_char *)L"Export Succeeded.");
	return;
_ERROR:
	PX_MessageBoxAlertOk(&pMain->messagebox,(const px_char *)"Export Failed.");
	MP_Free(&pMain->Ins->runtime.mp_game,memData);
	return;
}


px_void SoundLab_Main_OnButtonAdaptApply(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	DWORD id;

	pMain->bDone=PX_FALSE;
	pMain->offset=0;
	pMain->Ins->soundplay.Sounds[0].offset=0;
	PX_MessageBoxAlertOk(&pMain->messagebox,pMain->MessageText);
	PX_ObjectSetVisible(pMain->messagebox.btn_Ok,PX_FALSE);

	CreateThread(NULL, 0, SoundLab_Main_ApplyTuning, pMain, 0, &id);

	pMain->status=SOUNDLAB_MAIN_STATUS_TUNING;
	PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);
}

px_void SoundLab_Main_OnButtonFilterMode(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	pMain->bFilterMode=!pMain->bFilterMode;
}

px_void SoundLab_Main_OnButtonResetAdapt(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	PX_Object_FilterEditorReset(pMain->FilterEditor);
	PX_Object_SliderBarSetValue(pMain->SliderBar_PitchShift,50);
	PX_Object_SliderBarSetValue(pMain->SliderBar_TimeScale,50);
}


px_void SoundLab_Main_OnTimeDomainClick(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;

	if (PX_ObjectIsCursorInRegion(pObj,e))
	{
		px_int offset;
		pMain->status=SOUNDLAB_MAIN_STATUS_STOP;
		PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);

		offset=(px_int)(pMain->offset+(e.Param_int[0]-pMain->Map_TimeDomain->x-pMain->Map_TimeDomain->Width/2)/PX_Object_CoordinatesGetCoordinateWidth(pMain->Map_TimeDomain)*PX_COUNTOF(pMain->TimeDomainDataVertical)*pMain->TimeDomainDurationCount*2*2);

		if (offset<0)
		{
			offset=0;
		}
		else if((px_int)offset>pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2)
		{
			offset=pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2;
		}

		//4byte aligned
		offset=4*(offset/4);

		pMain->offset=offset;
		pMain->Ins->soundplay.Sounds[0].offset=offset;

	}
}

px_void SoundLab_Main_OnTimeDomainWheel(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;

	if (PX_ObjectIsCursorInRegion(pObj,e))
	{
		if (e.Param_int[2]>0)
		{
			pMain->TimeDomainDurationCount*=2;
		}
		else
		{
			pMain->TimeDomainDurationCount/=2;
		}
		if (pMain->TimeDomainDurationCount>2048)
		{
			pMain->TimeDomainDurationCount=2048;
		}
		if (pMain->TimeDomainDurationCount<=0)
		{
			pMain->TimeDomainDurationCount=1;
		}

	}
}

px_void SoundLab_Main_OnSpectrumClick(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;

	if (PX_ObjectIsCursorInRegion(pObj,e))
	{
		px_int offset;
		pMain->status=SOUNDLAB_MAIN_STATUS_STOP;
		PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);

		offset=(px_int)(pMain->offset+(e.Param_int[0]-pMain->SpectrumCursor->x-pMain->SpectrumCursor->Width/2)*PX_SOUNDLAB_WINDOW_WIDTH);

		if (offset<0)
		{
			offset=0;
		}
		else if((px_int)offset>pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2)
		{
			offset=pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2;
		}
		pMain->offset=offset;
		pMain->Ins->soundplay.Sounds[0].offset=offset;

	}

}

px_void SoundLab_SpectrumCursorOnMouseMove(PX_Object *pObj,PX_Object_Event e,px_void *ptr)
{
	SoundLab_SpectrumCursor *pCursor=(SoundLab_SpectrumCursor *)pObj->pObject;
	pCursor->bshow=PX_FALSE;
	if (PX_ObjectIsCursorInRegion(pObj,e))
	{
		pCursor->bshow=PX_TRUE;
		pCursor->xOffset=e.Param_int[0]-64;
		pCursor->yOffset=e.Param_int[1]-8;
	}
}


px_void SoundLab_Main_LoadAudio(PX_Object *pObject,PX_Object_Event e,px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	px_char *FilePath;
	DWORD id;
	PX_SoundPlayClear(&pMain->Ins->soundplay);

	FilePath=PX_OpenFileDialog("WaveFile\0*.wav");
	if (!FilePath)
	{
		return;
	}
	if (pMain->SoundData.mp)
	{
		PX_SoundStaticDataFree(&pMain->SoundData);
	}
	
	if (!PX_LoadSoundFromFile(&pMain->Ins->runtime.mp_game,&pMain->SoundData,FilePath))
	{
		MessageBox(PX_GetWindowHwnd(),"Could not load target file.","Error",MB_OK);
		return;
	}

	if (pMain->sourcePCM)
	{
		free(pMain->sourcePCM);
	}
	pMain->sourcePCM=(px_byte *)malloc(pMain->SoundData.size);
	if (!pMain->sourcePCM)
	{
		MessageBox(PX_GetWindowHwnd(),"Out of memories","Error",MB_OK);
		PX_SoundStaticDataFree(&pMain->SoundData);
		return;
	}
	PX_memcpy(pMain->sourcePCM,pMain->SoundData.buffer,pMain->SoundData.size);
	pMain->sourcePCMSize=pMain->SoundData.size;


	PX_strset(pMain->FilePath,FilePath);
	pMain->bDone=PX_FALSE;
	pMain->offset=0;
	pMain->Ins->soundplay.Sounds[0].offset=0;
	PX_MessageBoxAlertOk(&pMain->messagebox,pMain->MessageText);
	PX_ObjectSetVisible(pMain->messagebox.btn_Ok,PX_FALSE);

	CreateThread(NULL, 0, SoundLab_Main_InitializeSoundData, pMain, 0, &id);
	

	pMain->status=SOUNDLAB_MAIN_STATUS_ANALYSISING;
	PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);
}

px_bool SoundLab_Main_Initialize(PX_Instance *Ins,SoundLab_Main *pMain)
{
	px_int i;
	PX_Object_CoordinateData data;
	
	pMain->status=SOUNDLAB_MAIN_STATUS_STOP;
	pMain->offset=0;
	pMain->PowerThreshold=0.05;
	pMain->TimeDomainDurationCount=256;
	pMain->Ins=Ins;

	pMain->displayMode=SOUNDLAB_MAIN_DISPLAY_STFT;
	pMain->MaxPower=1;
	pMain->FilePath[0]=0;
	pMain->bFilterMode=PX_FALSE;
	pMain->rotAnimationAngle=0;
	pMain->currentAnnOffset=0;
	pMain->sourcePCM=PX_NULL;
	pMain->sourcePCMSize=0;
	pMain->mfccsamplecount=0;
	pMain->bTrainRun=PX_FALSE;
	pMain->AnnEpoch=0;
	pMain->AnnSucceeded=PX_FALSE;
	pMain->bTrain=PX_TRUE;
	pMain->bTest=PX_TRUE;
	pMain->EnterTestMode=0;
	pMain->TestIndex=0;

	for (i=0;i<PX_COUNTOF(pMain->ann_time);i++)
	{
		pMain->ann_time[i]=i;
		pMain->trainloss[i]=0;
		pMain->testloss[i]=0;
	}
	
	//BP Neural Network for sound classifier
	PX_ANNInitialize(&pMain->Ins->runtime.mp_game,&pMain->ann,0.05,PX_ANN_REGULARZATION_NONE,0);
	//PX_ANN_ACTIVATION_FUNCTION_SIGMOID input nodes for MFCC
	PX_ANNAddLayer(&pMain->ann,SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT,0.1,PX_ANN_ACTIVATION_FUNCTION_LINEAR,PX_ANN_LAYER_WEIGHT_INITMODE_GAUSSRAND,0);
	//hidden
	PX_ANNAddLayer(&pMain->ann,SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT,0.1,PX_ANN_ACTIVATION_FUNCTION_SIGMOID,PX_ANN_LAYER_WEIGHT_INITMODE_GAUSSRAND,0);
	//out
	PX_ANNAddLayer(&pMain->ann,2,0.1,PX_ANN_ACTIVATION_FUNCTION_SIGMOID,PX_ANN_LAYER_WEIGHT_INITMODE_GAUSSRAND,0);



	PX_MFCCInitialize(&pMain->mfcc,PX_SOUNDLAB_WINDOW_WIDTH,44100,80,2000);

	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_file,SOUNDLAB_PATH_FILE))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_play,SOUNDLAB_PATH_PLAY))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_record,SOUNDLAB_PATH_RECORD))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_recording,SOUNDLAB_PATH_RECORDING))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_recordfinish,SOUNDLAB_PATH_RECORDFINISH))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_ann,SOUNDLAB_PATH_ANN))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_sampling,SOUNDLAB_PATH_SAMPLING))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_save,SOUNDLAB_PATH_SAVE))return PX_FALSE;

	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_pause,SOUNDLAB_PATH_STOP))return PX_FALSE;
	if(!PX_LoadTextureFromFile(&Ins->runtime.mp_resources,&pMain->tex_reset,SOUNDLAB_PATH_RESET))return PX_FALSE;
	if(!PX_FontModuleInitialize(&Ins->runtime.mp_resources,&pMain->fm))return PX_FALSE;
	if(!PX_LoadFontModuleFromFile(&pMain->fm,SOUNDLAB_PATH_EN32))return PX_FALSE;
	if(!PX_MessageBoxInitialize(&Ins->runtime,&pMain->messagebox,&pMain->fm,Ins->runtime.width,Ins->runtime.height))return PX_FALSE;
	pMain->messagebox.Message=pMain->MessageText;

	PX_TextureCreate(&Ins->runtime.mp_ui,&pMain->tex_Spectrum_renderTarget,Ins->runtime.width-64*2,512);
	PX_TextureCreate(&Ins->runtime.mp_ui,&pMain->tex_Spectrum,Ins->runtime.width-64*2,512);
	PX_SurfaceClear(&pMain->tex_Spectrum_renderTarget,0,0,pMain->tex_Spectrum_renderTarget.width-1,pMain->tex_Spectrum_renderTarget.height-1,PX_COLOR(255,0,0,0));
	PX_SurfaceClear(&pMain->tex_Spectrum,0,0,pMain->tex_Spectrum.width-1,pMain->tex_Spectrum.height-1,PX_COLOR(255,0,0,0));

	pMain->root=PX_ObjectCreate(&Ins->runtime.mp_ui,PX_NULL,0,0,0,0,0,0);

	
	pMain->Map_Spectrum=PX_Object_CoordinatesCreate(&Ins->runtime.mp_ui,pMain->root,0,0,Ins->runtime.width,512);
	PX_Object_GetCoordinates(pMain->Map_Spectrum)->borderColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_GetCoordinates(pMain->Map_Spectrum)->FontColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_CoordinatesSetLeftVerticalMin(pMain->Map_Spectrum,0);
	PX_Object_CoordinatesSetLeftVerticalMax(pMain->Map_Spectrum,1.0);
	PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Spectrum,0);
	PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Spectrum,22050);
	PX_Object_CoordinatesSetTitleRightShow(pMain->Map_Spectrum,PX_FALSE);
	PX_Object_CoordinatesSetMargin(PX_Object_GetCoordinates(pMain->Map_Spectrum),64,64,16,16);
	PX_Object_CoordinatesShowHelpLine(pMain->Map_Spectrum,PX_TRUE);
	PX_ObjectSetVisible(pMain->Map_Spectrum,PX_FALSE);

	data.Color=PX_COLOR(255,0,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->SpectrumX;
	data.MapVerticalArray=pMain->SpectrumY;
	data.Size=PX_COUNTOF(pMain->SpectrumX);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_Spectrum,data);

	pMain->Map_Cepstrum=PX_Object_CoordinatesCreate(&Ins->runtime.mp_ui,pMain->root,0,0,Ins->runtime.width,512);
	PX_Object_GetCoordinates(pMain->Map_Cepstrum)->borderColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_GetCoordinates(pMain->Map_Cepstrum)->FontColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_CoordinatesSetLeftVerticalMin(pMain->Map_Cepstrum,0);
	PX_Object_CoordinatesSetLeftVerticalMax(pMain->Map_Cepstrum,1.0);
	PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Cepstrum,0);
	PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Cepstrum,1);
	PX_Object_CoordinatesSetTitleRightShow(pMain->Map_Cepstrum,PX_FALSE);
	PX_Object_CoordinatesSetMargin(PX_Object_GetCoordinates(pMain->Map_Cepstrum),64,64,16,16);
	//PX_Object_CoordinatesSetStyle(pMain->Map_Cepstrum,PX_OBJECT_COORDINATES_LINEMODE_PILLAR);
	PX_ObjectSetVisible(pMain->Map_Cepstrum,PX_FALSE);
	PX_Object_CoordinatesShowHelpLine(pMain->Map_Cepstrum,PX_TRUE);

	data.Color=PX_COLOR(255,0,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->CepstrumX;
	data.MapVerticalArray=pMain->CepstrumY;
	data.Size=PX_COUNTOF(pMain->CepstrumX);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_Cepstrum,data);

	pMain->Map_Adapt=PX_Object_CoordinatesCreate(&Ins->runtime.mp_ui,pMain->root,0,0,Ins->runtime.width,512);
	PX_Object_GetCoordinates(pMain->Map_Adapt)->borderColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_GetCoordinates(pMain->Map_Adapt)->FontColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_CoordinatesSetLeftVerticalMin(pMain->Map_Adapt,0);
	PX_Object_CoordinatesSetLeftVerticalMax(pMain->Map_Adapt,1.0);
	PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Adapt,0);
	PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Adapt,8613);
	PX_Object_CoordinatesSetTitleRightShow(pMain->Map_Adapt,PX_FALSE);
	PX_Object_CoordinatesSetMargin(PX_Object_GetCoordinates(pMain->Map_Adapt),64,64,16,16);
	//PX_Object_CoordinatesSetStyle(pMain->Map_Cepstrum,PX_OBJECT_COORDINATES_LINEMODE_PILLAR);
	PX_ObjectSetVisible(pMain->Map_Adapt,PX_FALSE);
	PX_Object_CoordinatesShowHelpLine(pMain->Map_Adapt,PX_TRUE);
	PX_Object_GetCoordinates(pMain->Map_Adapt)->ScaleEnabled=PX_FALSE;
	data.Color=PX_COLOR(128,0,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->SpectrumX;
	data.MapVerticalArray=pMain->SpectrumY;
	data.Size=PX_COUNTOF(pMain->SpectrumX);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_Adapt,data);

	data.Color=PX_COLOR(192,255,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->PreviewSpectrumX;
	data.MapVerticalArray=pMain->PreviewSpectrumY;
	data.Size=PX_COUNTOF(pMain->PreviewSpectrumX);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_Adapt,data);

	//////////////////////////////////////////////////////////////////////////
	//ANN
	pMain->Map_Ann=PX_Object_CoordinatesCreate(&Ins->runtime.mp_ui,pMain->root,0,0,Ins->runtime.width,512);
	PX_Object_GetCoordinates(pMain->Map_Ann)->borderColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_GetCoordinates(pMain->Map_Ann)->FontColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_CoordinatesSetLeftVerticalMin(pMain->Map_Ann,0);
	PX_Object_CoordinatesSetLeftVerticalMax(pMain->Map_Ann,1.0);
	PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Ann,0);
	PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Ann,100);
	PX_Object_CoordinatesSetTitleRightShow(pMain->Map_Ann,PX_FALSE);
	PX_Object_CoordinatesSetMargin(PX_Object_GetCoordinates(pMain->Map_Ann),64,64,16,16);
	//PX_Object_CoordinatesSetStyle(pMain->Map_Cepstrum,PX_OBJECT_COORDINATES_LINEMODE_PILLAR);
	PX_ObjectSetVisible(pMain->Map_Ann,PX_FALSE);
	PX_Object_CoordinatesShowHelpLine(pMain->Map_Ann,PX_TRUE);
	PX_Object_GetCoordinates(pMain->Map_Ann)->ScaleEnabled=PX_FALSE;
	data.Color=PX_COLOR(255,0,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->ann_time;
	data.MapVerticalArray=pMain->trainloss;
	data.Size=PX_COUNTOF(pMain->trainloss);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_Ann,data);

	data.Color=PX_COLOR(192,255,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->ann_time;
	data.MapVerticalArray=pMain->testloss;
	data.Size=PX_COUNTOF(pMain->testloss);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_Ann,data);
	//////////////////////////////////////////////////////////////////////////


	pMain->btn_AdaptApply=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->Map_Adapt,(px_int)pMain->Map_Adapt->x+(px_int)pMain->Map_Adapt->Width-128,(px_int)pMain->Map_Adapt->y+16+3,64,24,"Apply",PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBorderColor(pMain->btn_AdaptApply,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_AdaptApply,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_AdaptApply,PX_COLOR(128,128,128,128));
	PX_ObjectRegisterEvent(pMain->btn_AdaptApply,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonAdaptApply,pMain);

	pMain->btn_FilterMode=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->Map_Adapt,(px_int)pMain->Map_Adapt->x+(px_int)pMain->Map_Adapt->Width-128,(px_int)pMain->Map_Adapt->y+16+3+26,64,24,"Filter",PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBorderColor(pMain->btn_FilterMode,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_FilterMode,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_FilterMode,PX_COLOR(128,128,128,128));
	PX_ObjectRegisterEvent(pMain->btn_FilterMode,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonFilterMode,pMain);

	pMain->btn_ResetAdapt=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->Map_Adapt,(px_int)pMain->Map_Adapt->x+(px_int)pMain->Map_Adapt->Width-128,(px_int)pMain->Map_Adapt->y+16+3+26+26,64,24,"Reset",PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBorderColor(pMain->btn_ResetAdapt,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_ResetAdapt,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_ResetAdapt,PX_COLOR(128,128,128,128));
	PX_ObjectRegisterEvent(pMain->btn_ResetAdapt,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonResetAdapt,pMain);



	pMain->SliderBar_PitchShift=PX_Object_SliderBarCreate(&Ins->runtime.mp_ui,pMain->Map_Adapt,(px_int)pMain->Map_Adapt->x+(px_int)pMain->Map_Adapt->Width-128-256-16,(px_int)pMain->Map_Adapt->y+20,256,23,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
	PX_Object_SliderBarSetBackgroundColor(pMain->SliderBar_PitchShift,PX_COLOR(0,0,0,0));
	PX_Object_SliderBarSetColor(pMain->SliderBar_PitchShift,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_SliderBarSetMax(pMain->SliderBar_PitchShift,100);
	PX_Object_SliderBarSetValue(pMain->SliderBar_PitchShift,50);

	pMain->SliderBar_TimeScale=PX_Object_SliderBarCreate(&Ins->runtime.mp_ui,pMain->Map_Adapt,(px_int)pMain->Map_Adapt->x+(px_int)pMain->Map_Adapt->Width-128-256-16,(px_int)pMain->Map_Adapt->y+20+25,256,23,PX_OBJECT_SLIDERBAR_TYPE_HORIZONTAL,PX_OBJECT_SLIDERBAR_STYLE_BOX);
	PX_Object_SliderBarSetBackgroundColor(pMain->SliderBar_TimeScale,PX_COLOR(0,0,0,0));
	PX_Object_SliderBarSetColor(pMain->SliderBar_TimeScale,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_SliderBarSetMax(pMain->SliderBar_TimeScale,100);
	PX_Object_SliderBarSetValue(pMain->SliderBar_TimeScale,50);

	pMain->FilterEditor=PX_Object_FilterEditorCreate(&Ins->runtime.mp_ui,pMain->Map_Adapt,(px_int)(pMain->Map_Adapt->x+64),(px_int)(pMain->Map_Adapt->y+16),(px_int)(pMain->Map_Adapt->Width-128),(px_int)(pMain->Map_Adapt->Height-32));
	PX_Object_FilterEditorSetOperateCount(pMain->FilterEditor,100);
	PX_Object_FilterEditorSetRange(pMain->FilterEditor,60);


	pMain->Map_TimeDomain=PX_Object_CoordinatesCreate(&Ins->runtime.mp_ui,pMain->root,0,520,Ins->runtime.width,128);
	PX_Object_GetCoordinates(pMain->Map_TimeDomain)->borderColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_GetCoordinates(pMain->Map_TimeDomain)->FontColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_CoordinatesSetLeftVerticalMin(pMain->Map_TimeDomain,-1);
	PX_Object_CoordinatesSetLeftVerticalMax(pMain->Map_TimeDomain,1);
	PX_Object_CoordinatesSetTitleRightShow(pMain->Map_TimeDomain,PX_FALSE);
	PX_Object_CoordinatesSetMargin(PX_Object_GetCoordinates(pMain->Map_TimeDomain),64,64,16,16);
	//PX_Object_CoordinatesSetStyle(pMain->Map_TimeDomain,PX_OBJECT_COORDINATES_LINEMODE_PILLAR);
	PX_Object_GetCoordinates(pMain->Map_TimeDomain)->ScaleEnabled=PX_FALSE;
	PX_ObjectRegisterEvent(pMain->Map_TimeDomain,PX_OBJECT_EVENT_CURSORCLICK,SoundLab_Main_OnTimeDomainClick,pMain);
	PX_ObjectRegisterEvent(pMain->Map_TimeDomain,PX_OBJECT_EVENT_CURSORWHEEL,SoundLab_Main_OnTimeDomainWheel,pMain);
	
	data.Color=PX_COLOR(255,0,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->TimeDomainDataHorizontal;
	data.MapVerticalArray=pMain->TimeDomainDataVertical;
	data.Size=PX_COUNTOF(pMain->TimeDomainDataVertical);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_TimeDomain,data);

	

	pMain->Map_PreviewTimeDomain=PX_Object_CoordinatesCreate(&Ins->runtime.mp_ui,pMain->root,0,520,Ins->runtime.width,128);
	PX_Object_GetCoordinates(pMain->Map_PreviewTimeDomain)->borderColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_GetCoordinates(pMain->Map_PreviewTimeDomain)->FontColor=PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR);
	PX_Object_CoordinatesSetLeftVerticalMin(pMain->Map_PreviewTimeDomain,-1);
	PX_Object_CoordinatesSetLeftVerticalMax(pMain->Map_PreviewTimeDomain,1);
	PX_Object_CoordinatesSetHorizontalMin(pMain->Map_PreviewTimeDomain,0);
	PX_Object_CoordinatesSetHorizontalMax(pMain->Map_PreviewTimeDomain,PX_SOUNDLAB_WINDOW_WIDTH);
	PX_Object_CoordinatesSetTitleRightShow(pMain->Map_PreviewTimeDomain,PX_FALSE);
	PX_Object_CoordinatesSetMargin(PX_Object_GetCoordinates(pMain->Map_PreviewTimeDomain),64,64,16,16);
	//PX_Object_CoordinatesSetStyle(pMain->Map_PreviewTimeDomain,PX_OBJECT_COORDINATES_LINEMODE_PILLAR);
	PX_Object_GetCoordinates(pMain->Map_PreviewTimeDomain)->ScaleEnabled=PX_FALSE;
	PX_ObjectSetVisible(pMain->Map_PreviewTimeDomain,PX_FALSE);

	data.Color=PX_COLOR(255,0,255,64);
	data.ID=0;
	data.linewidth=1;
	data.Map=PX_OBJECT_COORDINATES_COORDINATEDATA_MAP_LEFT;
	data.MapHorizontalArray=pMain->PreviewTimeDomainDataHorizontal;
	data.MapVerticalArray=pMain->PreviewTimeDomainDataVertical;
	data.Size=PX_COUNTOF(pMain->PreviewTimeDomainDataVertical);
	data.Visibled=PX_TRUE;
	PX_Object_CoordinatesAddData(pMain->Map_PreviewTimeDomain,data);


	pMain->btn_OpenFile=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,700,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_OpenFile,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_OpenFile,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_OpenFile,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_OpenFile,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_OpenFile,&pMain->tex_file);

	PX_ObjectRegisterEvent(pMain->btn_OpenFile,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_LoadAudio,pMain);


	pMain->btn_Record=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->btn_OpenFile->x+74,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_Record,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_Record,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_Record,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_Record,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_Record,&pMain->tex_record);

	PX_ObjectRegisterEvent(pMain->btn_Record,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonRecord,pMain);


	pMain->btn_ann=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->btn_Record->x+74,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_ann,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_ann,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_ann,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_ann,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_ann,&pMain->tex_ann);

	PX_ObjectRegisterEvent(pMain->btn_ann,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonANN,pMain);




	pMain->btn_Play=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->btn_ann->x+74,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_Play,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_Play,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_Play,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_Play,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_Play,&pMain->tex_play);

	PX_ObjectRegisterEvent(pMain->btn_Play,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_Play,pMain);

	pMain->btn_Pause=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->btn_ann->x+74,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_Pause,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_Pause,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_Pause,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_Pause,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_Pause,&pMain->tex_pause);

	PX_ObjectRegisterEvent(pMain->btn_Pause,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_Pause,pMain);

	pMain->btn_reset=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->btn_Pause->x+74,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_reset,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_reset,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_reset,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_reset,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_reset,&pMain->tex_reset);

	PX_ObjectRegisterEvent(pMain->btn_reset,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_Reset,pMain);

	pMain->btn_RecordFinish=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->btn_Pause->x+74,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_RecordFinish,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_RecordFinish,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_RecordFinish,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_RecordFinish,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_RecordFinish,&pMain->tex_recordfinish);
	PX_ObjectSetVisible(pMain->btn_RecordFinish,PX_FALSE);
	PX_ObjectRegisterEvent(pMain->btn_RecordFinish,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonRecordFinish,pMain);

	pMain->btn_save=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->btn_RecordFinish->x+74,692,64,64,"",PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetBorderColor(pMain->btn_save,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_save,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_save,PX_COLOR(128,128,128,128));
	PX_Object_PushButtonSetStyle(pMain->btn_save,PX_OBJECT_PUSHBUTTON_STYLE_ROUNDRECT);
	PX_Object_PushButtonSetImage(pMain->btn_save,&pMain->tex_save);
	PX_ObjectRegisterEvent(pMain->btn_save,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonSave,pMain);



	pMain->btn_stft=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->Ins->runtime.width-64,16,48,24,"STFT",PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBorderColor(pMain->btn_stft,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_stft,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_stft,PX_COLOR(128,128,128,128));
	PX_ObjectRegisterEvent(pMain->btn_stft,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonSTFT,pMain);
	
	pMain->btn_spectrum=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->Ins->runtime.width-64,(px_int)(pMain->btn_stft->y+pMain->btn_stft->Height+1),48,24,"SPEC",PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBorderColor(pMain->btn_spectrum,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_spectrum,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_spectrum,PX_COLOR(128,128,128,128));
	PX_ObjectRegisterEvent(pMain->btn_spectrum,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonSpectrum,pMain);


	pMain->btn_cepstrum=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->Ins->runtime.width-64,(px_int)(pMain->btn_spectrum->y+pMain->btn_spectrum->Height+1),48,24,"CEPS",PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBorderColor(pMain->btn_cepstrum,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_cepstrum,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_cepstrum,PX_COLOR(128,128,128,128));
	PX_ObjectRegisterEvent(pMain->btn_cepstrum,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonCepstrum,pMain);

	pMain->btn_adapt=PX_Object_PushButtonCreate(&Ins->runtime.mp_ui,pMain->root,(px_int)pMain->Ins->runtime.width-64,(px_int)(pMain->btn_cepstrum->y+pMain->btn_cepstrum->Height+1),48,24,"APT",PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBorderColor(pMain->btn_adapt,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));
	PX_Object_PushButtonSetBackgroundColor(pMain->btn_adapt,PX_COLOR(0,0,0,0));
	PX_Object_PushButtonSetCursorColor(pMain->btn_adapt,PX_COLOR(128,128,128,128));
	PX_ObjectRegisterEvent(pMain->btn_adapt,PX_OBJECT_EVENT_EXECUTE,SoundLab_Main_OnButtonAdapt,pMain);

	pMain->SpectrumCursor=SoundLab_SpectrumCursorCreate(&Ins->runtime.mp_ui,pMain->root,pMain);
	PX_ObjectRegisterEvent(pMain->SpectrumCursor,PX_OBJECT_EVENT_CURSORMOVE,SoundLab_SpectrumCursorOnMouseMove,pMain);
	PX_ObjectRegisterEvent(pMain->SpectrumCursor,PX_OBJECT_EVENT_CURSORCLICK,SoundLab_Main_OnSpectrumClick,pMain);

	PX_memset(pMain->RecorderCache,0,sizeof(pMain->RecorderCache));
	pMain->recorderSoundData.buffer=(px_byte *)pMain->RecorderCache;
	pMain->recorderSoundData.size=sizeof(pMain->RecorderCache);
	pMain->recorderSoundData.channel=PX_SOUND_CHANNEL_ONE;
	pMain->recorderSoundData.mp=PX_NULL;
	pMain->recorderWCursorByte=0;
	Ins->soundplay.Sounds[PX_SOUNDLAB_SOUNDCAPTURE_INDEX].data=&pMain->recorderSoundData;
	Ins->soundplay.Sounds[PX_SOUNDLAB_SOUNDCAPTURE_INDEX].loop=PX_TRUE;
	Ins->soundplay.Sounds[PX_SOUNDLAB_SOUNDCAPTURE_INDEX].offset=0;

	PX_TuningInitialize(&Ins->runtime.mp_game,&pMain->DebugTuning,1.0,1.0,PX_NULL,PX_NULL,PX_NULL,PX_TUNING_WINDOW_SIZE_1024);
	

	return PX_TRUE;
}


DWORD WINAPI SoundLab_Main_AnnTrain(px_void *ptr)
{
	SoundLab_Main *pMain=(SoundLab_Main *)ptr;
	px_int sidx=0,i;
	px_double _out[2],expect[2],randomMfcc[SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT];
	px_double loss;

	while (PX_TRUE)
	{
		if (!pMain->bTrainRun)
		{
			break;
		}

		if (pMain->AnnEpoch>=SOUBDLAB_TRAIN_TARGET_EPOCH)
		{
			if (pMain->currenttrainloss<0.001)
			{
				pMain->AnnSucceeded=PX_TRUE;
			}
			else
			{
				pMain->AnnSucceeded=PX_FALSE;
			}
			break;
		}

		if (pMain->bTest)
		{
			loss=0;
			expect[0]=0.99999;
			expect[1]=0.00001;
			PX_ANNForward(&pMain->ann,pMain->mfccsample[sidx].mfcc_factor);
			PX_ANNGetOutput(&pMain->ann,_out);
			loss+=(expect[0]-_out[0])*(expect[0]-_out[0])+(expect[1]-_out[1])*(expect[1]-_out[1]);

			for (i=0;i<SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT;i++)
			{
				randomMfcc[i]=PX_randRange(0.00000001,0.99999999);
			}

			expect[1]=0.99999;
			expect[0]=0.00001;

			PX_ANNForward(&pMain->ann,randomMfcc);
			PX_ANNGetOutput(&pMain->ann,_out);
			
			loss+=(expect[0]-_out[0])*(expect[0]-_out[0])+(expect[1]-_out[1])*(expect[1]-_out[1]);

			loss/=2;
			pMain->currenttestloss=loss;

		}

		if (pMain->bTrain)
		{
			loss=0;
			expect[0]=0.99999;
			expect[1]=0.00001;

			loss+=PX_ANNTrain(&pMain->ann,pMain->mfccsample[sidx].mfcc_factor,expect);
			

			for (i=0;i<SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT;i++)
			{
				randomMfcc[i]=PX_randRange(0.00000001,0.99999999);
			}

			expect[1]=0.99999;
			expect[0]=0.00001;

			loss+=PX_ANNTrain(&pMain->ann,randomMfcc,expect);

			loss/=2;
			pMain->currenttrainloss=loss;
			pMain->AnnEpoch++;
		}
		sidx++;

		if (sidx>=SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT)
		{
			sidx=0;
		}
	}
	return 0;
}

px_void SoundLab_Main_Update(SoundLab_Main *pMain,px_dword elpased)
{
	px_int i;

	if (pMain->messagebox.show)
	{
		switch(pMain->status)
		{
		case SOUNDLAB_MAIN_STATUS_ANALYSISING:
			{
				px_char text[64];
				px_word *pw;
				PX_sprintf3(text,sizeof(text),"Analysing:%1.2% Memories:%2.1/%3.1 MB",PX_STRINGFORMAT_FLOAT((px_float)pMain->schedule),\
					PX_STRINGFORMAT_FLOAT((px_float)(pMain->process_memories/1024.0/1024.0*pMain->schedule/100.0)),\
					PX_STRINGFORMAT_FLOAT((px_float)(pMain->process_memories/1024.0/1024.0))
					);
				pw=(px_word *)pMain->MessageText;
				for (i=0;i<sizeof(pMain->MessageText)/2-1;i++)
				{
					pw[i]=text[i];
					if(!text[i]) break;
				}
				PX_MessageBoxUpdate(&pMain->messagebox,elpased);
				if (pMain->bDone)
				{
					if (pMain->lastError[0])
					{
						PX_ObjectSetVisible(pMain->messagebox.btn_Ok,PX_TRUE);
					}
					else
					{
						pMain->status=SOUNDLAB_MAIN_STATUS_STOP;
						PX_MessageBoxClose(&pMain->messagebox);
					}

				}
				return;
			}
			break;
		case SOUNDLAB_MAIN_STATUS_TUNING:
			{
				px_char text[64];
				px_word *pw;
				DWORD id;
				PX_sprintf3(text,sizeof(text),"Processing:%1.2% Memories:%2.1/%3.1 MB",PX_STRINGFORMAT_FLOAT((px_float)pMain->schedule),\
					PX_STRINGFORMAT_FLOAT((px_float)(pMain->process_memories/1024.0/1024.0*pMain->schedule/100.0)),\
					PX_STRINGFORMAT_FLOAT((px_float)(pMain->process_memories/1024.0/1024.0))
					);
				pw=(px_word *)pMain->MessageText;
				for (i=0;i<sizeof(pMain->MessageText)/2-1;i++)
				{
					pw[i]=text[i];
					if(!text[i]) break;
				}
				PX_MessageBoxUpdate(&pMain->messagebox,elpased);
				if (pMain->bDone)
				{
					if (pMain->lastError[0])
					{
						PX_ObjectSetVisible(pMain->messagebox.btn_Ok,PX_TRUE);
					}
					else
					{
						pMain->bDone=PX_FALSE;
						pMain->offset=0;
						pMain->Ins->soundplay.Sounds[0].offset=0;
						PX_MessageBoxAlertOk(&pMain->messagebox,pMain->MessageText);
						PX_ObjectSetVisible(pMain->messagebox.btn_Ok,PX_FALSE);
						CreateThread(NULL, 0, SoundLab_Main_InitializeSoundData, pMain, 0, &id);
						pMain->status=SOUNDLAB_MAIN_STATUS_ANALYSISING;
						PX_SoundPlayPause(&pMain->Ins->soundplay,PX_TRUE);
						
					}

				}
				return;
			}
			break;
		default:
			{
				PX_MessageBoxUpdate(&pMain->messagebox,elpased);
			}
			break;
		}
		
	}

	switch (pMain->status)
	{
	case SOUNDLAB_MAIN_STATUS_PLAY:
	case SOUNDLAB_MAIN_STATUS_STOP:
		{
			if (pMain->SoundData.buffer)
			{
				px_double timeLength=0,CurrentTime=0;
				px_int minIndex=0,MaxIndex=0;
				px_int offset=0,cOffset=0;
				timeLength=pMain->TimeDomainDurationCount*PX_COUNTOF(pMain->TimeDomainDataVertical)/44100.0;
				offset=pMain->offset;
				if (pMain->Ins->soundplay.Sounds[0].data)
				{
					offset=pMain->Ins->soundplay.Sounds[0].offset;
					if(offset%4!=0)offset=offset/4*4;
					pMain->offset=offset;
				}
				CurrentTime=1.0*offset/2/2/44100;

				PX_Object_CoordinatesSetHorizontalMin(pMain->Map_TimeDomain,CurrentTime-timeLength/2);
				PX_Object_CoordinatesSetHorizontalMax(pMain->Map_TimeDomain,CurrentTime+timeLength/2);

				//Sample Data

				for (i=0;i<PX_COUNTOF(pMain->TimeDomainDataVertical);i++)
				{
					cOffset=offset+(i-PX_COUNTOF(pMain->TimeDomainDataVertical)/2)*pMain->TimeDomainDurationCount*4;
					if (cOffset%(pMain->TimeDomainDurationCount*4))
					{
						cOffset=cOffset/(pMain->TimeDomainDurationCount*4)*(pMain->TimeDomainDurationCount*4);
					}
					if (cOffset<0)
					{
						pMain->TimeDomainDataVertical[i]=0;
					}
					else if(cOffset>pMain->SoundData.size-1)
					{
						pMain->TimeDomainDataVertical[i]=0;
					}
					else
					{
						pMain->TimeDomainDataVertical[i]=*(short *)(pMain->SoundData.buffer+cOffset);
					}

					if(pMain->TimeDomainDataVertical[i]>0)pMain->TimeDomainDataVertical[i]/=pMain->wave_max;
					if(pMain->TimeDomainDataVertical[i]<0)pMain->TimeDomainDataVertical[i]/=-pMain->wave_min;

					pMain->TimeDomainDataHorizontal[i]=1.0*cOffset/2/2/44100;
				}

			}

			switch (pMain->status)
			{
			case SOUNDLAB_MAIN_STATUS_PLAY:
				{
					PX_ObjectSetVisible(pMain->btn_Pause,PX_TRUE);
					PX_ObjectSetVisible(pMain->btn_Play,PX_FALSE);
				}
				break;
			case SOUNDLAB_MAIN_STATUS_STOP:
				{
					PX_ObjectSetVisible(pMain->btn_Play,PX_TRUE);
					PX_ObjectSetVisible(pMain->btn_Pause,PX_FALSE);
				}
				break;
			}
		}
		break;
	case SOUNDLAB_MAIN_STATUS_RECORDING:
		{
			px_int readsize;
			while (PX_TRUE)
			{
	
				readsize=PX_AudioCaptureReadEx(pMain->recordCache+pMain->recorderWCursorByte,1024,1024);//1k
				if (!readsize)
				{
					break;
				}
				pMain->recorderWCursorByte+=readsize;
				if (pMain->recorderWCursorByte>=sizeof(pMain->recordCache))
				{
					SoundLab_Main_RecordFinish(pMain);
					break;
				}
			}

			if (pMain->recorderWCursorByte>PX_COUNTOF(pMain->TimeDomainDataVertical)*2)
			{
				px_short *pData=(px_short *)(pMain->recordCache+pMain->recorderWCursorByte-PX_COUNTOF(pMain->TimeDomainDataVertical)*2);

				PX_Object_CoordinatesSetHorizontalMin(pMain->Map_TimeDomain,0);
				PX_Object_CoordinatesSetHorizontalMax(pMain->Map_TimeDomain,PX_COUNTOF(pMain->TimeDomainDataVertical));

				for (i=0;i<PX_COUNTOF(pMain->TimeDomainDataVertical);i++)
				{
					pMain->TimeDomainDataHorizontal[i]=i;
					pMain->TimeDomainDataVertical[i]=pData[i]/32768.0;
				}
			}
			

			
		}
		break;

		case SOUNDLAB_MAIN_STATUS_ANN:
			{
				px_int i;
				px_int readsize;
				px_short data[PX_SOUNDLAB_WINDOW_WIDTH];
				px_double ddata[PX_SOUNDLAB_WINDOW_WIDTH];
				px_double sum,max;
				px_int startepoch;
				px_double Threshold=1024;
				PX_MFCC_FEATURE feature;

				if (pMain->mfccsamplecount<SOUNDLAB_TRAIN_MFCC_ARRAY_COUNT)
				{
					//sampling data
					while (pMain->mfccsamplecount<SOUNDLAB_TRAIN_MFCC_ARRAY_COUNT)
					{
						pMain->bTrainRun=PX_FALSE;

						//sample
						readsize=PX_AudioCaptureReadEx(data,PX_COUNTOF(data)*2,PX_COUNTOF(data)*2);//1024 samples
						if (!readsize)
						{
							break;
						}

						sum=0;
						for (i=0;i<PX_COUNTOF(data);i++)
						{
							sum+=PX_ABS(data[i]);
							ddata[i]=data[i];
						}

						if (sum/PX_COUNTOF(data)<Threshold)
						{
							continue;
						}


						PX_MFCCParse(&pMain->mfcc,ddata,&feature);
						feature.factor[0]=0;

						max=0;

						for (i=0;i<32;i++)
						{
							feature.factor[i]*=feature.factor[i];
						}

						for (i=0;i<32;i++)
						{
							if (PX_ABS(feature.factor[i])>max)
							{
								max=PX_ABS(feature.factor[i]);
							}
						}
						for (i=0;i<32;i++)
						{
							feature.factor[i]/=max;
						}

						for (i=1;i<=SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT;i++)
						{
							pMain->mfccsample[pMain->mfccsamplecount].mfcc_factor[i-1]=feature.factor[i];
						}

						pMain->mfccsamplecount++;
					}

					if (pMain->mfccsamplecount>=SOUNDLAB_TRAIN_MFCC_ARRAY_COUNT)
					{
						DWORD id;
						pMain->bTrainRun=PX_TRUE;
						pMain->currentAnnOffset=0;
						PX_ANNReset(&pMain->ann);

						CreateThread(NULL, 0, SoundLab_Main_AnnTrain, pMain, 0, &id);
					}
				}
				else
				{
					//training
					if (pMain->AnnEpoch<SOUBDLAB_TRAIN_TARGET_EPOCH)
					{
						if (pMain->currentAnnOffset>PX_COUNTOF(pMain->ann_time)-1)
						{
							startepoch=pMain->currentAnnOffset-PX_COUNTOF(pMain->ann_time)+1;
							//shift
							for (i=0;i<PX_COUNTOF(pMain->ann_time)-1;i++)
							{
								pMain->ann_time[i]=startepoch+i;
								pMain->trainloss[i]=pMain->trainloss[i+1];
								pMain->testloss[i]=pMain->testloss[i+1];
							}
							pMain->ann_time[i]=startepoch+i;
							pMain->trainloss[i]=pMain->currenttrainloss;
							pMain->testloss[i]=pMain->currenttestloss;

							PX_Object_CoordinatesGetCoordinateData(pMain->Map_Ann,0)->Size=PX_COUNTOF(pMain->ann_time);
							PX_Object_CoordinatesGetCoordinateData(pMain->Map_Ann,1)->Size=PX_COUNTOF(pMain->ann_time);

							PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Ann,startepoch);
							PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Ann,startepoch+PX_COUNTOF(pMain->ann_time)-1);

						}
						else
						{
							pMain->trainloss[pMain->currentAnnOffset]=pMain->currenttrainloss;;
							pMain->testloss[pMain->currentAnnOffset]=pMain->currenttestloss;

							PX_Object_CoordinatesGetCoordinateData(pMain->Map_Ann,0)->Size=pMain->currentAnnOffset+1;
							PX_Object_CoordinatesGetCoordinateData(pMain->Map_Ann,1)->Size=pMain->currentAnnOffset+1;

							PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Ann,0);
							PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Ann,PX_COUNTOF(pMain->ann_time));

						}
						pMain->currentAnnOffset++;
					}
					else
					{
						//test
						if (pMain->EnterTestMode==PX_FALSE)
						{
							//first enter
							pMain->EnterTestMode=PX_TRUE;
							for (i=0;i<PX_COUNTOF(pMain->ann_time);i++)
							{
								pMain->ann_time[i]=i;
								pMain->trainloss[i]=0;
								pMain->testloss[i]=0;
							}
							PX_Object_CoordinatesSetHorizontalMin(pMain->Map_Ann,0);
							PX_Object_CoordinatesSetHorizontalMax(pMain->Map_Ann,PX_COUNTOF(pMain->ann_time)-1);
						}
						else
						{
							//Grab data
							while (PX_TRUE)
							{
								px_double test[SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT];
								px_double _out[2];
								//sample
								readsize=PX_AudioCaptureReadEx(data,PX_COUNTOF(data)*2,PX_COUNTOF(data)*2);//1024 samples
								if (!readsize)
								{
									break;
								}

								sum=0;
								for (i=0;i<PX_COUNTOF(data);i++)
								{
									sum+=PX_ABS(data[i]);
									ddata[i]=data[i];
								}

								if (sum/PX_COUNTOF(data)<Threshold)
								{
									continue;
								}

								PX_MFCCParse(&pMain->mfcc,ddata,&feature);
								feature.factor[0]=0;

								max=0;

								for (i=0;i<32;i++)
								{
									feature.factor[i]*=feature.factor[i];
								}

								for (i=0;i<32;i++)
								{
									if (PX_ABS(feature.factor[i])>max)
									{
										max=PX_ABS(feature.factor[i]);
									}
								}
								for (i=0;i<32;i++)
								{
									feature.factor[i]/=max;
								}

								for (i=1;i<=SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT;i++)
								{
									test[i-1]=feature.factor[i];
								}

								PX_ANNForward(&pMain->ann,test);
								PX_ANNGetOutput(&pMain->ann,_out);

								if (pMain->TestIndex>PX_COUNTOF(pMain->testloss)-1)
								{
									pMain->TestIndex=0;
								}
								pMain->testloss[pMain->TestIndex]=((0.99999-_out[0])*(0.99999-_out[0])+((0.00001-_out[1])*(0.00001-_out[1])))/2;
								pMain->TestIndex++;
							}
						}
					}
					
				}
			}
			break;
	}
	

}
static px_void SoundLab_Main_HideAll(SoundLab_Main *pMain)
{
	PX_ObjectSetVisible(pMain->Map_Spectrum,PX_FALSE);
	PX_ObjectSetVisible(pMain->Map_Cepstrum,PX_FALSE);
	PX_ObjectSetVisible(pMain->SpectrumCursor,PX_FALSE);
	PX_ObjectSetVisible(pMain->Map_Adapt,PX_FALSE);
	PX_ObjectSetVisible(pMain->Map_Ann,PX_FALSE);
	PX_ObjectSetVisible(pMain->Map_TimeDomain,PX_FALSE);
	PX_ObjectSetVisible(pMain->Map_PreviewTimeDomain,PX_FALSE);
}
px_void SoundLab_Main_ExecuteRender(px_surface *psurface,SoundLab_Main *pMain,px_dword elpased)
{
	px_double pt_per_pix;
	px_int winW,oft;
	PX_Object_Coordinates *pcd;
	px_char Text[64];
	static px_int aniOffset=0,time=0;
	//////////////////////////////////////////////////////////////////////////
	//Menu Frame
	//
	PX_GeoDrawBorder(psurface,50,680,pMain->Ins->runtime.width-50,pMain->Ins->runtime.height-32,2,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR));

	if (pMain->displayMode!=SOUNDLAB_MAIN_DISPLAY_RECORDING)
	{
		//render TimeDomain window
		pt_per_pix=(PX_SOUNDLAB_DEFAULT_TIMEDOMAIN_SIZE*pMain->TimeDomainDurationCount)/PX_Object_CoordinatesGetCoordinateWidth(pMain->Map_TimeDomain);
		winW=(px_int)(PX_SOUNDLAB_WINDOW_WIDTH/pt_per_pix);
		if (winW==0)
		{
			winW=1;
		}
		pcd=PX_Object_GetCoordinates(pMain->Map_TimeDomain);

		PX_GeoDrawRect(psurface,\
			(px_int)pMain->Map_TimeDomain->x+(px_int)pMain->Map_TimeDomain->Width/2-winW/2,\
			(px_int)pMain->Map_TimeDomain->y+pcd->TopSpacer+1,
			(px_int)pMain->Map_TimeDomain->x+(px_int)pMain->Map_TimeDomain->Width/2+winW/2,\
			(px_int)pMain->Map_TimeDomain->y+pcd->TopSpacer+PX_Object_CoordinatesGetCoordinateHeight(pMain->Map_TimeDomain)-1,
			PX_COLOR(128,255,128,96)
			);
	}

	PX_SurfaceClear(&pMain->tex_Spectrum_renderTarget,0,0,pMain->tex_Spectrum_renderTarget.width-1,pMain->tex_Spectrum_renderTarget.height-1,PX_COLOR(255,0,0,0));
	SoundLab_Main_HideAll(pMain);

	//Render STFT Spectrum
	switch (pMain->displayMode)
	{
	case SOUNDLAB_MAIN_DISPLAY_STFT:
		{

			px_int i;
			px_dword offset;
			px_double dframe[PX_SOUNDLAB_WINDOW_WIDTH];
			px_short *PCM16;
			px_double max;
			PX_MFCC_FEATURE mfccfeature;
			offset=pMain->offset;
			PX_memset(&mfccfeature,0,sizeof(mfccfeature));
			if (offset>0)
			{
				if((px_int)offset<pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2)
				{
					PCM16=(px_short *)(pMain->SoundData.buffer+offset);
					//Sample
					for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
					{
						dframe[i]=PCM16[i*2];//2bytes
					}
					PX_MFCCParse(&pMain->mfcc,dframe,&mfccfeature);
				}
			}
			max=0;
			for (i=1;i<PX_MFCC_DCT_FACTOR_SIZE;i++)
			{
				if (PX_ABS(mfccfeature.factor[i])>max)
				{
					max=PX_ABS(mfccfeature.factor[i]);
				}
			}

			if (max==0)
			{
				max=1;
			}


			if (pMain->SoundData.buffer)
			{
				oft=pMain->tex_Spectrum_renderTarget.width/2;
				oft-=pMain->Ins->soundplay.Sounds[0].offset/2/2/(PX_SOUNDLAB_WINDOW_WIDTH/4);
				PX_TextureRender(&pMain->tex_Spectrum_renderTarget,&pMain->tex_Spectrum,oft,0,PX_TEXTURERENDER_REFPOINT_LEFTTOP,PX_NULL);
			}
			PX_GeoDrawLine(&pMain->tex_Spectrum_renderTarget,pMain->tex_Spectrum_renderTarget.width/2,0,pMain->tex_Spectrum_renderTarget.width/2,pMain->tex_Spectrum_renderTarget.height-1,1,PX_COLOR(128,255,192,255));
			
			
			for (i=1;i<PX_MFCC_DCT_FACTOR_SIZE;i++)
			{
				px_double w=PX_ABS(mfccfeature.factor[i])/max;
				PX_GeoDrawRect(&pMain->tex_Spectrum_renderTarget,-16+i*16,0,-16+i*16+16,16,PX_COLOR((px_uchar)(255*w),(px_uchar)(255*w),(px_uchar)(255*w),(px_uchar)(255*(1-w))));
			}

			PX_TextureRender(psurface,&pMain->tex_Spectrum_renderTarget,64,8,PX_TEXTURERENDER_REFPOINT_LEFTTOP,PX_NULL);
			PX_FontDrawText(psurface,18,10,"MFCC:",PX_COLOR(255,255,168,192),PX_FONT_ALIGN_XLEFT);
			PX_ObjectSetVisible(pMain->SpectrumCursor,PX_TRUE);
			PX_ObjectSetVisible(pMain->Map_TimeDomain,PX_TRUE);
		}
		break;
	case SOUNDLAB_MAIN_DISPLAY_SPECTRUM:
		{
			px_int i;
			px_dword offset;
			px_complex frame[PX_SOUNDLAB_WINDOW_WIDTH];
			px_double window[PX_SOUNDLAB_WINDOW_WIDTH];
			px_double power=0,Phase=0;
			px_short *PCM16;

			offset=pMain->offset;

			if (offset<0)
			{
				power=0;
				PX_memset(pMain->SpectrumX,0,sizeof(pMain->SpectrumX));
				PX_memset(pMain->SpectrumY,0,sizeof(pMain->SpectrumY));
			}
			else if((px_int)offset>pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2)
			{
				power=0;
				PX_memset(pMain->SpectrumX,0,sizeof(pMain->SpectrumX));
				PX_memset(pMain->SpectrumY,0,sizeof(pMain->SpectrumY));
			}
			else
			{
				PCM16=(px_short *)(pMain->SoundData.buffer+offset);
				PX_WindowFunction_hamming(window,PX_SOUNDLAB_WINDOW_WIDTH);
				//Sample
				for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
				{
					frame[i].re=PCM16[i*2];//2bytes
					frame[i].im=0;
					frame[i].re*=window[i];
				}
				PX_FFT(frame,frame,PX_SOUNDLAB_WINDOW_WIDTH);

				for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH/2;i++)
				{
					pMain->SpectrumX[i]=i*(44100.0/PX_SOUNDLAB_WINDOW_WIDTH);
					pMain->SpectrumY[i]=PX_sqrtd(frame[i].re*frame[i].re+frame[i].im*frame[i].im)/pMain->MaxPower;
				}
				//power=PX_sqrtd(frame[sy].re*frame[sy].re+frame[sy].im*frame[sy].im);
				//Phase=PX_atan2(frame[sy].im,frame[sy].re);
			}
			PX_Object_CoordinatesGetCoordinateData(pMain->Map_Spectrum,0)->Size=PX_SOUNDLAB_WINDOW_WIDTH/2;
			PX_ObjectSetVisible(pMain->Map_Spectrum,PX_TRUE);
			PX_ObjectSetVisible(pMain->Map_TimeDomain,PX_TRUE);
		}
		break;
	case SOUNDLAB_MAIN_DISPLAY_CEPSTRUM:
		{
			px_int i;
			px_dword offset;
			px_double max;
			px_complex frame[PX_SOUNDLAB_WINDOW_WIDTH];
			px_double window[PX_SOUNDLAB_WINDOW_WIDTH];
			px_double power=0,Phase=0;
			px_short *PCM16;

			offset=pMain->offset;

			if (offset<0)
			{
				power=0;
				PX_memset(pMain->CepstrumX,0,sizeof(pMain->CepstrumX));
				PX_memset(pMain->CepstrumY,0,sizeof(pMain->CepstrumY));
			}
			else if((px_int)offset>pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2)
			{
				power=0;
				PX_memset(pMain->CepstrumX,0,sizeof(pMain->CepstrumX));
				PX_memset(pMain->CepstrumY,0,sizeof(pMain->CepstrumY));
			}
			else
			{
				PCM16=(px_short *)(pMain->SoundData.buffer+offset);
				PX_WindowFunction_hamming(window,PX_SOUNDLAB_WINDOW_WIDTH);
				//Sample
				for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
				{
					frame[i].re=PCM16[i*2];//2bytes
					frame[i].im=0;
					frame[i].re*=window[i];
				}

				PX_Cepstrum(frame,frame,PX_SOUNDLAB_WINDOW_WIDTH,PX_CEPTRUM_TYPE_REAL);
				max=0.000000000001;
				for (i=55;i<PX_SOUNDLAB_WINDOW_WIDTH/2;i++)
				{
					if (frame[i].re>max)
					{
						max=frame[i].re;
					}
				}
				pMain->CepstrumX[0]=0;
				pMain->CepstrumY[0]=0;
				for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH/2;i++)
				{
					if (i<55)
					{
						pMain->CepstrumX[i]=i;
						pMain->CepstrumY[i]=0;
					}
					else
					{
						pMain->CepstrumX[i]=i;
						pMain->CepstrumY[i]=frame[i].re/max/1.5;
					}
				}
			}

			PX_Object_CoordinatesGetCoordinateData(pMain->Map_Cepstrum,0)->Size=PX_SOUNDLAB_WINDOW_WIDTH;
			PX_ObjectSetVisible(pMain->Map_Cepstrum,PX_TRUE);
			PX_ObjectSetVisible(pMain->Map_TimeDomain,PX_TRUE);
		}
		break;
	case SOUNDLAB_MAIN_DISPLAY_ADAPT:
		{
			px_int i;
			px_double max=0;
			px_double PitchShift,TimeScale;
			px_complex Frame[PX_SOUNDLAB_WINDOW_WIDTH];
			px_double window[PX_SOUNDLAB_WINDOW_WIDTH];
			px_double Filter[PX_SOUNDLAB_WINDOW_WIDTH];
			px_double resample[PX_SOUNDLAB_WINDOW_WIDTH*2];
			px_short *PCM;

			//Sample Data
			PCM=(px_short *)(pMain->sourcePCM+pMain->offset);
			if (pMain->offset>0&&pMain->offset<pMain->sourcePCMSize-2*2*PX_COUNTOF(pMain->SnapshotSampleData))
			{
				for (i=0;i<PX_COUNTOF(pMain->SnapshotSampleData);i++)
				{
					pMain->SnapshotSampleData[i]=PCM[i*2];
				}
			}
			else
			{
				PX_memset(pMain->SnapshotSampleData,0,sizeof(pMain->SnapshotSampleData));
			}

			//UI
			if (pMain->bFilterMode)
			{
				PX_ObjectSetVisible(pMain->FilterEditor,PX_TRUE);
				PX_Object_CoordinatesShowHelpLine(pMain->Map_Adapt,PX_FALSE);
				PX_Object_GetCoordinates(pMain->Map_Adapt)->LeftTitleShow=PX_FALSE;
				PX_Object_GetCoordinates(pMain->Map_Adapt)->HorizontalShow=PX_FALSE;
				PX_Object_GetCoordinates(pMain->Map_Adapt)->ShowGuides=PX_FALSE;
			}
			else
			{
				PX_ObjectSetVisible(pMain->FilterEditor,PX_FALSE);
				PX_Object_CoordinatesShowHelpLine(pMain->Map_Adapt,PX_TRUE);
				PX_Object_GetCoordinates(pMain->Map_Adapt)->LeftTitleShow=PX_TRUE;
				PX_Object_GetCoordinates(pMain->Map_Adapt)->HorizontalShow=PX_TRUE;
				PX_Object_GetCoordinates(pMain->Map_Adapt)->ShowGuides=PX_TRUE;
			}


			PitchShift=(PX_Object_SliderBarGetValue(pMain->SliderBar_PitchShift)-50.0)/100;
			
			if(PitchShift==0)
			{
				PitchShift=1;
			}
			else if (PitchShift>0)
			{
				PitchShift=1+PitchShift/5*(2.75)*10;
			}
			else
			{
				PitchShift=-PitchShift/5*(2.75)*10;
				PitchShift=1/(1+PitchShift);
			}

			TimeScale=(PX_Object_SliderBarGetValue(pMain->SliderBar_TimeScale)-50.0)/100;

			if(TimeScale==0)
			{
				TimeScale=1;
			}
			else if (TimeScale>0)
			{
				TimeScale=1+TimeScale/5*(2.75)*10;
			}
			else
			{
				TimeScale=-TimeScale/5*(2.75)*10;
				TimeScale=1/(1+TimeScale);
			}
			
			PX_memset(Filter,0,sizeof(Filter));
			PX_Object_FilterEditorMapData(pMain->FilterEditor,Filter,200);
			

			PX_WindowFunction_sinc(window,PX_SOUNDLAB_WINDOW_WIDTH);
			//source spectrum
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re=pMain->SnapshotSampleData[i]*window[i];
				Frame[i].im=0;
			}
			PX_FFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);

			max=0;
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re=PX_sqrtd(Frame[i].re*Frame[i].re+Frame[i].im+Frame[i].im)/PX_SOUNDLAB_WINDOW_WIDTH;
				if (Frame[i].re>max)
				{
					max=Frame[i].re;
				}
				Frame[i].im=0;
			}

			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				pMain->SpectrumX[i]=44100.0/PX_SOUNDLAB_WINDOW_WIDTH*i;
				pMain->SpectrumY[i]=Frame[i].re/max;
			}

			//resample
			PX_LinearInterpolationResample(pMain->SnapshotSampleData,resample,(px_int)((PX_SOUNDLAB_WINDOW_WIDTH*PitchShift)*2),PX_SOUNDLAB_WINDOW_WIDTH*2);

			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re=resample[i]*window[i];
				Frame[i].im=0;
			}
			PX_FFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);

			max=0;
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re=PX_sqrtd(Frame[i].re*Frame[i].re+Frame[i].im+Frame[i].im)/PX_SOUNDLAB_WINDOW_WIDTH*Filter[i];
				if (Frame[i].re>max)
				{
					max=Frame[i].re;
				}
				Frame[i].im=0;
			}

			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				pMain->PreviewSpectrumX[i]=44100.0/PX_SOUNDLAB_WINDOW_WIDTH*i;
				pMain->PreviewSpectrumY[i]=Frame[i].re/max;
			}


			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				pMain->PreviewTimeDomainDataHorizontal[i]=i;
				pMain->PreviewTimeDomainDataVertical[i]=0;
			}

			//rebuild time domain
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re=resample[i]/32768.0*window[i];
				Frame[i].im=0;
			}
			PX_FFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re*=Filter[i];
				Frame[i].im*=Filter[i];
			}
			PX_FT_Symmetry(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);
			PX_IFFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);

			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH/2;i++)
			{
				pMain->PreviewTimeDomainDataVertical[i]=Frame[i+PX_SOUNDLAB_WINDOW_WIDTH/2].re*window[i+PX_SOUNDLAB_WINDOW_WIDTH/2];
			}

			//resample2
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re=resample[i+PX_SOUNDLAB_WINDOW_WIDTH/2]/32768.0*window[i];
				Frame[i].im=0;
			}
			PX_FFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re*=Filter[i];
				Frame[i].im*=Filter[i];
			}
			PX_FT_Symmetry(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);
			PX_IFFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);

			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH/2;i++)
			{
				pMain->PreviewTimeDomainDataVertical[i]+=Frame[i].re*window[i];
			}

			for (;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				pMain->PreviewTimeDomainDataVertical[i]=Frame[i].re*window[i];
			}

			//resample3
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re=resample[i+PX_SOUNDLAB_WINDOW_WIDTH]/32768.0*window[i];
				Frame[i].im=0;
			}
			PX_FFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);
			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
			{
				Frame[i].re*=Filter[i];
				Frame[i].im*=Filter[i];
			}
			PX_FT_Symmetry(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);
			PX_IFFT(Frame,Frame,PX_SOUNDLAB_WINDOW_WIDTH);

			for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH/2;i++)
			{
				pMain->PreviewTimeDomainDataVertical[i+PX_SOUNDLAB_WINDOW_WIDTH/2]+=Frame[i].re*window[i];
			}


			PX_ObjectSetVisible(pMain->Map_PreviewTimeDomain,PX_TRUE);
			PX_ObjectSetVisible(pMain->Map_Adapt,PX_TRUE);

			PX_sprintf1(Text,sizeof(Text),"PitchShift:%1.2",PX_STRINGFORMAT_FLOAT((px_float)PitchShift));
			PX_FontDrawText(psurface,785,24,Text,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR),PX_FONT_ALIGN_XRIGHT);

			PX_sprintf1(Text,sizeof(Text),"TimeShift:%1.2",PX_STRINGFORMAT_FLOAT((px_float)TimeScale));
			PX_FontDrawText(psurface,785,50,Text,PX_COLOR(SOUNDLAB_DEFAULT_FRAME_COLOR),PX_FONT_ALIGN_XRIGHT);




		}
		break;
		case SOUNDLAB_MAIN_DISPLAY_RECORDING:
			{
				px_char content[48];
				px_word wcontent[sizeof(content)*2]={0};

				PX_TextureRenderEx(psurface,&pMain->tex_recording,pMain->Ins->runtime.width/2,pMain->Ins->runtime.height/2-120,PX_TEXTURERENDER_REFPOINT_CENTER,PX_NULL,1.0,(px_float)pMain->rotAnimationAngle);
				pMain->rotAnimationAngle+=elpased/2000.0*360;

				PX_sprintf1(content,sizeof(content),"Recording (%1.2% memories used.)",PX_STRINGFORMAT_FLOAT(pMain->recorderWCursorByte*1.0f/sizeof(pMain->RecorderCache)));
				PX_FontModule_atow(content,wcontent);

				PX_FontModuleDrawText(psurface,pMain->Ins->runtime.width/2,pMain->Ins->runtime.height/2+25,wcontent,PX_COLOR(255,0,0,0),&pMain->fm,PX_FONT_ALIGN_XCENTER);
				PX_ObjectSetVisible(pMain->Map_TimeDomain,PX_TRUE);
			}
			break;
		case SOUNDLAB_MAIN_DISPLAY_ANN:
			{
				if (pMain->mfccsamplecount<SOUNDLAB_TRAIN_MFCC_ARRAY_COUNT)
				{
					//No data
					px_char content[48];
					px_word wcontent[sizeof(content)*2]={0};

					PX_sprintf1(content,sizeof(content),"Sampling:%1.2%",PX_STRINGFORMAT_FLOAT(pMain->mfccsamplecount*100.0f/SOUNDLAB_TRAIN_MFCC_ARRAY_COUNT));
					PX_FontModule_atow(content,wcontent);
					PX_FontModuleDrawText(psurface,pMain->Ins->runtime.width/2,pMain->Ins->runtime.height/2+25,wcontent,PX_COLOR(255,0,0,0),&pMain->fm,PX_FONT_ALIGN_XCENTER);

					PX_TextureRender(psurface,&pMain->tex_sampling,pMain->Ins->runtime.width/2,pMain->Ins->runtime.height/2-100,PX_TEXTURERENDER_REFPOINT_CENTER,PX_NULL);
					PX_ObjectSetVisible(pMain->Map_Ann,PX_FALSE);
				}
				else
				{
					PX_ObjectSetVisible(pMain->Map_Ann,PX_TRUE);
					if (pMain->AnnEpoch>=SOUBDLAB_TRAIN_TARGET_EPOCH)
					{
						//end training
						px_char content[16];
						px_word wcontent[sizeof(content)*2]={0};

						if (pMain->AnnSucceeded)
						{
							PX_sprintf1(content,sizeof(content),"Succeeded",PX_STRINGFORMAT_INT(0));
							PX_FontModule_atow(content,wcontent);

							PX_FontModuleDrawText(psurface,1000,60,wcontent,PX_COLOR(255,0,255,0),&pMain->fm,PX_FONT_ALIGN_XCENTER);
						}
						else
						{
							PX_sprintf1(content,sizeof(content),"Failed",PX_STRINGFORMAT_INT(0));
							PX_FontModule_atow(content,wcontent);

							PX_FontModuleDrawText(psurface,1000,60,wcontent,PX_COLOR(255,255,0,0),&pMain->fm,PX_FONT_ALIGN_XCENTER);
						}
						

					}
					else
					{
						//Training
						px_char content[48];
						px_word wcontent[sizeof(content)*2]={0};

						PX_sprintf1(content,sizeof(content),"epoch:%1",PX_STRINGFORMAT_INT(pMain->AnnEpoch));
						PX_FontModule_atow(content,wcontent);
						
						PX_FontModuleDrawText(psurface,1000,60,wcontent,PX_COLOR(255,128,255,192),&pMain->fm,PX_FONT_ALIGN_XCENTER);
					}
					
				}
				PX_ObjectSetVisible(pMain->Map_TimeDomain,PX_TRUE);
				
			}
			break;
	}

	

	PX_ObjectRender(psurface,pMain->root,elpased);


	if (pMain->FilePath[0])
	{
		PX_FontDrawText(psurface,72,690,pMain->FilePath,PX_COLOR(255,0,255,0),PX_FONT_ALIGN_XLEFT);
		PX_FontDrawText(psurface,72,710,"SampleRate:44100 HZ",PX_COLOR(255,0,255,0),PX_FONT_ALIGN_XLEFT);
		PX_sprintf1(Text,sizeof(Text),"Window Type:Hamming - %1 - 75% Overlap",PX_STRINGFORMAT_INT(PX_SOUNDLAB_WINDOW_WIDTH));
		PX_FontDrawText(psurface,72,730,Text,PX_COLOR(255,0,255,0),PX_FONT_ALIGN_XLEFT);
	}
	else
	{
		if (pMain->displayMode!=SOUNDLAB_MAIN_DISPLAY_RECORDING)
		{
			px_int d;
			time+=elpased;
			d=time%1500;
			if (d>750)
			{
				d=1500-d;
			}
			PX_FontDrawText(psurface,336+(d)*(d)/2/10000,720,"Input Audio from file or microphone ---->",PX_COLOR(255,0,255,0),PX_FONT_ALIGN_XLEFT);

			PX_GeoDrawBorder(psurface,693,685,696+147,760,2,PX_COLOR(255,0,255,0));
		}
	}
}

px_void SoundLab_Main_Render(px_surface *psurface,SoundLab_Main *pMain,px_dword elpased)
{
	if (pMain->messagebox.show)
	{
		PX_MessageBoxRender(psurface,&pMain->messagebox,elpased);
	}
	else
	{
		SoundLab_Main_ExecuteRender(psurface,pMain,elpased);
	}
}

px_void SoundLab_Main_PostEvent(SoundLab_Main *pMain,PX_Object_Event e)
{
	if (pMain->messagebox.show)
	{
		PX_MessageBoxPostEvent(&pMain->messagebox,e);
	}
	else
	{
		PX_ObjectPostEvent(pMain->root,e);
	}
}

px_void  SoundLab_SpectrumCursorRender(px_surface *psurface,PX_Object *pObject,px_uint elpased)
{
	px_int i,sy,pitchHz;
	px_dword offset;
	SoundLab_SpectrumCursor *pCursor=(SoundLab_SpectrumCursor *)pObject->pObject;
	px_char text[64];
	px_complex frame[PX_SOUNDLAB_WINDOW_WIDTH]={0},pitchFrame[PX_SOUNDLAB_WINDOW_WIDTH]={0};
	px_double window[PX_SOUNDLAB_WINDOW_WIDTH];
	px_int hz;
	px_double power=0,Phase=0;
	px_short *PCM16;

	sy=(px_int)(pObject->Height-pCursor->yOffset-1);
	hz=44100/PX_SOUNDLAB_WINDOW_WIDTH*(sy);
	PX_WindowFunction_hamming(window,PX_SOUNDLAB_WINDOW_WIDTH);

	
	if (pCursor->bshow)
	{
		if (!pCursor->pMain->SoundData.buffer)
		{
			power=0;
		}
		else
		{
			offset=pCursor->pMain->offset+(pCursor->xOffset-pCursor->pMain->tex_Spectrum_renderTarget.width/2)*PX_SOUNDLAB_WINDOW_WIDTH;

			if (offset<0)
			{
				power=0;
			}
			else if((px_int)offset>pCursor->pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2)
			{
				power=0;
			}
			else
			{
				PCM16=(px_short *)(pCursor->pMain->SoundData.buffer+offset);

				//Sample
				for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
				{
					frame[i].re=PCM16[i*2];//2bytes
					frame[i].im=0;
					frame[i].re*=window[i];
					pitchFrame[i]=frame[i];
				}
				PX_FFT(frame,frame,PX_SOUNDLAB_WINDOW_WIDTH);
				power=PX_sqrtd(frame[sy].re*frame[sy].re+frame[sy].im*frame[sy].im)/PX_SOUNDLAB_WINDOW_WIDTH*2;
				Phase=PX_atan2(frame[sy].re,-frame[sy].im);
			}
		}
		pitchHz=PX_PitchEstimation(pitchFrame,PX_SOUNDLAB_WINDOW_WIDTH,44100);

		PX_GeoDrawRect(psurface,64+pCursor->xOffset+16,pCursor->yOffset-16,64+pCursor->xOffset+16+148,pCursor->yOffset+54,PX_COLOR(192,64,96,255));
		PX_GeoDrawBorder(psurface,64+pCursor->xOffset+16,pCursor->yOffset-16,64+pCursor->xOffset+16+148,pCursor->yOffset+54,1,PX_COLOR(255,255,96,255));
		PX_sprintf1(text,sizeof(text),"%1 Hz",PX_STRINGFORMAT_INT(hz));
		PX_FontDrawText(psurface,64+pCursor->xOffset+24,pCursor->yOffset-15,text,PX_COLOR(255,255,192,255),PX_FONT_ALIGN_XLEFT);
		PX_sprintf1(text,sizeof(text),"%1 Amplitude",PX_STRINGFORMAT_INT((px_int)power));
		PX_FontDrawText(psurface,64+pCursor->xOffset+24,pCursor->yOffset+2,text,PX_COLOR(255,255,192,255),PX_FONT_ALIGN_XLEFT);
		PX_sprintf1(text,sizeof(text),"%1 Phase",PX_STRINGFORMAT_FLOAT((px_float)Phase));
		PX_FontDrawText(psurface,64+pCursor->xOffset+24,pCursor->yOffset+20,text,PX_COLOR(255,255,192,255),PX_FONT_ALIGN_XLEFT);
		PX_sprintf1(text,sizeof(text),"Pitch:%1 Hz",PX_STRINGFORMAT_INT(pitchHz));
		PX_FontDrawText(psurface,64+pCursor->xOffset+24,pCursor->yOffset+38,text,PX_COLOR(255,255,192,255),PX_FONT_ALIGN_XLEFT);
	}
	
	//Pitch
	PX_memset(pitchFrame,0,sizeof(pitchFrame));

	offset=pCursor->pMain->offset;

	if (offset>0&&((px_int)offset<pCursor->pMain->SoundData.size-PX_SOUNDLAB_WINDOW_WIDTH*2*2))
	{
		PCM16=(px_short *)(pCursor->pMain->SoundData.buffer+offset);
		//Sample
		for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH;i++)
		{
			pitchFrame[i].re=PCM16[i*2];//2bytes
			pitchFrame[i].im=0;
			pitchFrame[i].re*=window[i];
		}
	}
	

	pitchHz=PX_PitchEstimation(pitchFrame,PX_SOUNDLAB_WINDOW_WIDTH,44100);
	if (pitchHz)
	{
		sy=pitchHz/(44100/PX_SOUNDLAB_WINDOW_WIDTH);
		for (i=1;pitchHz*i<=3300&&sy*i<pCursor->pMain->tex_Spectrum_renderTarget.height;i++)
		{
			PX_GeoDrawSolidCircle(psurface,54,8+pCursor->pMain->tex_Spectrum_renderTarget.height-1-sy*i,6,PX_COLOR(255,192,64,255));
			PX_GeoDrawLine(psurface,64,8+pCursor->pMain->tex_Spectrum_renderTarget.height-1-sy*i,pCursor->pMain->Ins->runtime.width-64,8+pCursor->pMain->tex_Spectrum_renderTarget.height-1-sy*i,1,PX_COLOR(128,255,128,0));
		}
	}
	PX_sprintf1(text,sizeof(text),"Pitch:%1 Hz",PX_STRINGFORMAT_INT(pitchHz));
	PX_FontDrawText(psurface,pCursor->pMain->Ins->runtime.width-64-128,8+16,text,PX_COLOR(255,255,192,255),PX_FONT_ALIGN_XLEFT);


}

PX_Object* SoundLab_SpectrumCursorCreate(px_memorypool *mp,PX_Object *Parent,SoundLab_Main *pMain)
{
	SoundLab_SpectrumCursor Cursor;
	Cursor.pMain=pMain;
	Cursor.xOffset=0;
	Cursor.yOffset=0;
	Cursor.bshow=PX_FALSE;
	return PX_ObjectCreateEx(mp,Parent,64,8,0,(px_float)pMain->tex_Spectrum_renderTarget.width,(px_float)pMain->tex_Spectrum_renderTarget.height,0,0,PX_NULL,SoundLab_SpectrumCursorRender,PX_NULL,&Cursor,sizeof(Cursor));
}

