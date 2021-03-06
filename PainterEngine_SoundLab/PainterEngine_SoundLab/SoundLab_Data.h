#ifndef SOUNDLAB_DATA_H
#define SOUNDLAB_DATA_H

#define SOUNDLAB_FEATURE_DATA_MFCC_SIZE 128
#define SOUNDLAB_WINDOW_WIDTH 1024
#define SOUNDLAB_DEFAULT_TIMEDOMAIN_SIZE 2048
#define SOUNDLAB_SOUNDANALYSIS_INDEX 0
#define SOUNDLAB_SOUNDCAPTURE_INDEX 1
#define SOUNDLAB_SOUNDCAPTURE_BLOCKSIZE 2048
#define SOUNDLAB_SOUNDCAPTURE_BLOCKCOUNT 16
#define SOUNDLAB_SOUNDCAPTURE_BLOCKWAIT  4
#define SOUNDLAB_TRAIN_MFCC_SAMPLE_FACTOR_COUNT 24
#define SOUNDLAB_TRAIN_MFCC_ARRAY_COUNT 128
#define SOUBDLAB_TRAIN_TARGET_EPOCH 10000
#define SOUNDLAB_OVERTONE_MARK_COUNT 20
#define SOUNDLAB_DEFLAUT_THRESHOLD 2048
typedef struct  
{
	px_dword magic;//sinc
	px_double pitchshift;
	px_double filter[SOUNDLAB_WINDOW_WIDTH];
	px_double fix[SOUNDLAB_WINDOW_WIDTH];
	PX_MFCC_FEATURE mfcc[SOUNDLAB_FEATURE_DATA_MFCC_SIZE];
}SOUNDLAB_FEATURE_DATA;

#endif