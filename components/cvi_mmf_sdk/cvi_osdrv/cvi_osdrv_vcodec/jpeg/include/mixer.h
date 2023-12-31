
#ifndef _MIXER_H_
#define _MIXER_H_

#include "jpuconfig.h"

typedef struct {
	int Format;
	int Index;
	jpu_buffer_t vbY;
	jpu_buffer_t vbCb;
	jpu_buffer_t vbCr;
	int strideY;
	int strideC;
} FRAME_BUF;

#define MAX_DISPLAY_WIDTH 1920
#define MAX_DISPLAY_HEIGHT 1088

#if defined(__cplusplus)
extern "C" {
#endif

int AllocateFrameBuffer(int instIdx, int format, int width, int height,
			int frameBufNum, int pack);
void FreeFrameBuffer(int instIdx);
FRAME_BUF *GetFrameBuffer(int instIdx, int index);
int GetFrameBufBase(int instIdx);
int GetFrameBufAllocSize(int instIdx);
void ClearFrameBuffer(int instIdx, int index);
FRAME_BUF *FindFrameBuffer(int instIdx, PhysicalAddress addrY);
#if defined(__cplusplus)
}
#endif

#endif //#ifndef _MIXER_H_
