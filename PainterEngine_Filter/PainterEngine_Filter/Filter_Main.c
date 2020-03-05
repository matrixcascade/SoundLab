#include "Filter_Main.h"


px_bool PX_FilterMain_LoadFilter(px_memorypool *mp,FilterMain_Filter *filter,const px_char *filepath)
{
	SOUNDLAB_FILTER_HEADER header;
	PX_IO_Data data;
	data=PX_LoadFileToIOData(filepath);
	if (!data.size)
	{
		return PX_FALSE;
	}
	if (data.size<sizeof(SOUNDLAB_FILTER_HEADER))
	{
		goto _ERROR;
	}
	PX_memcpy(&header,data.buffer,sizeof(header));

	if (!PX_memequ(&header.magic,"sinc",4))
	{
		goto _ERROR;
	}

	PX_memcpy(filter->filter,header.filter,sizeof(header.filter));
	filter->pitchshift=header.pitchshift;

	if (!PX_ANNImport(mp,&filter->ann,data.buffer+sizeof(header),data.size-sizeof(header)))
	{
		goto _ERROR;
	}
	PX_FreeIOData(&data);
	return PX_TRUE;

_ERROR:
	PX_FreeIOData(&data);
	return PX_FALSE;

}

px_bool PX_FilterMainInitialize(PX_Instance *Ins,Filter_Main *pfm)
{
	WIN32_FIND_DATA data;
	HANDLE fhandle;

	pfm->Activate=PX_FALSE;

	pfm->Ins=Ins;
	pfm->pitchAdapt=PX_TRUE;
	pfm->filterAdapt=PX_TRUE;
	pfm->CaptureCacheSize=0;
	PX_memset(pfm->banks,0,sizeof(pfm->banks));
	PX_TuningInitialize(&Ins->runtime.mp_game,&pfm->tuning,1.0,1.0,PX_NULL,PX_NULL,PX_NULL,PX_TUNING_WINDOW_SIZE_1024);
	PX_MFCCInitialize(&pfm->mfcc,PX_SOUNDLAB_WINDOW_WIDTH,44100,80,2000);

	if(!PX_AudioCaptureOpen(GUID_NULL,1))
	{
		MessageBox(PX_GetWindowHwnd(),"No microphone found.","Error",MB_OK);
		return PX_FALSE;
	}
	
	//load filter bank
	
	if((fhandle=FindFirstFile(FILTER_TEXT_PATH_FILTER,&data))==INVALID_HANDLE_VALUE)
	{
		MessageBox(PX_GetWindowHwnd(),"No model found.\nUse SoundLab to create a acoustic model.","Error",MB_OK);
		return PX_FALSE;
	}
	do 
	{
		px_char fullpath[MAX_PATH];
		PX_sprintf2(fullpath,sizeof(fullpath),"%1/%2",PX_STRINGFORMAT_STRING(FILTER_TEXT_FILTER_FOLDER),PX_STRINGFORMAT_STRING(data.cFileName));
		if(!PX_FilterMain_LoadFilter(&pfm->Ins->runtime.mp_game,&pfm->banks[pfm->activateCount],fullpath))
		{
			FindClose(fhandle);
			return PX_FALSE;
		}
		pfm->activateCount++;
	} while (FindNextFile(fhandle,&data));

	FindClose(fhandle);

	if (pfm->activateCount==0)
	{
		MessageBox(PX_GetWindowHwnd(),"No model found.\nUse SoundLab to create a acoustic model.","Error",MB_OK);
		return PX_FALSE;
	}
	return PX_TRUE;
}

/*
		inPCM=(px_short *)captureBuffer;
		outPCM=(px_short *)pfm->CaptureCache;
		for (i=0;i<PX_SOUNDLAB_WINDOW_WIDTH/2;i++)
		{
			outPCM[i*2]=inPCM[i];
			outPCM[i*2+1]=inPCM[i];
		}
		
		pfm->CaptureCacheSize=sizeof(captureBuffer)*2;
	*/
px_void PX_FilterMainUpdate(Filter_Main *pfm,px_dword elpased)
{
	px_int i;
	px_byte captureBuffer[PX_SOUNDLAB_WINDOW_WIDTH];//512 samples

	px_double ddata[PX_SOUNDLAB_WINDOW_WIDTH];
	PX_MFCC_FEATURE feature;
	px_double max;
	px_double _out[2];
	px_double mine,e;
	px_short *inPCM,*outPCM;
	px_int min_e_index,tuningSize;
	px_double CaptureCached[PX_SOUNDLAB_WINDOW_WIDTH*2];
	px_dword freesize;


	if (pfm->CaptureCacheSize==0)
	{
		//Capture
		if(PX_AudioCaptureReadEx(captureBuffer,sizeof(captureBuffer),sizeof(captureBuffer)))
		{
			inPCM=(px_short *)captureBuffer;

			for (i=0;i<sizeof(captureBuffer)/2;i++)
			{
				ddata[i]=(px_double)inPCM[i];
			}

			for (i=0;i<PX_COUNTOF(ddata);i++)
			{
				ddata[i]/=32768.0;
			}

			tuningSize=PX_TuningFilter(&pfm->tuning,ddata,sizeof(captureBuffer)/2,CaptureCached);

			for (i=0;i<PX_COUNTOF(ddata);i++)
			{
				ddata[i]*=32768.0;
			}


			if (tuningSize)
			{
				if (pfm->activateCount>1)
				{
					//MFCC
					PX_MFCCParse(&pfm->mfcc,ddata,&feature);
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

					mine=2;
					min_e_index=0;

					for (i=0;i<pfm->activateCount;i++)
					{
						PX_ANNForward(&pfm->banks[i].ann,feature.factor+1);
						PX_ANNGetOutput(&pfm->banks[i].ann,_out);
						e=(_out[0]-0.99999)*(_out[0]-0.99999)+(_out[1]-0.00001)*(_out[1]-0.00001);
						if (e<mine)
						{
							mine=e;
							min_e_index=i;
						}
					}
				}
				else
				{
					min_e_index=0;
				}
				
				if (pfm->pitchAdapt)
				{
					PX_TuningSetPitchShift(&pfm->tuning,pfm->banks[min_e_index].pitchshift);
				}
				else
				{
					PX_TuningSetPitchShift(&pfm->tuning,pfm->banks[0].pitchshift);
				}

				if (pfm->filterAdapt)
				{
					PX_TuningSetFilter(&pfm->tuning,pfm->banks[min_e_index].filter);
				}
				else
				{
					PX_TuningSetFilter(&pfm->tuning,pfm->banks[0].filter);
				}

				outPCM=(px_short *)pfm->CaptureCache;

				for (i=0;i<tuningSize;i++)
				{
					outPCM[i*2]=(px_short)(CaptureCached[i]*32768.0);
					outPCM[i*2+1]=(px_short)(CaptureCached[i]*32768.0);
				}
				pfm->CaptureCacheSize=tuningSize*2*2;
			}
		}
		
	}
	

	//Write tuning data
	if (pfm->CaptureCacheSize)
	{
		freesize=PX_AudioGetStandbyBufferSize();
		if (freesize>=(px_dword)pfm->CaptureCacheSize)
		{
			PX_AudioWriteBuffer(pfm->CaptureCache,pfm->CaptureCacheSize);
			//clear
			pfm->CaptureCacheSize=0;
		}
	}
	


}

