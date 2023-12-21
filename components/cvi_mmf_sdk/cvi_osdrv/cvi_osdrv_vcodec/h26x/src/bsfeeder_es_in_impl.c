/*
 * Copyright Cvitek Technologies Inc.
 *
 * Created Time: Aug, 2020
 */
#include "errno.h"

#include "vpuapifunc.h"
#include "main_helper.h"
#include "malloc.h"

#define MAX_FEEDING_SIZE 0x400000 /* 4MBytes */

typedef struct struct_ReaderConext {
	Int32 feedingSize;
	void *feedingAddr;
	BOOL eos;
} ReaderContext;

void *BSFeederEsIn_Create(void)
{
	ReaderContext *context = NULL;

	context = (ReaderContext *)calloc(1, sizeof(ReaderContext));
	if (context == NULL) {
		CVI_VC_ERR("failed to allocate memory\n");
		return NULL;
	}

	context->feedingSize = 0;
	context->eos = FALSE;

	return (void *)context;
}

BOOL BSFeederEsIn_Destroy(void *feeder)
{
	ReaderContext *context = (ReaderContext *)feeder;

	if (context == NULL) {
		CVI_VC_ERR("Null handle\n");
		return FALSE;
	}

	free(context);

	return TRUE;
}

Int32 BSFeederEsIn_Act(void *feeder, BSChunk *chunk)
{
	ReaderContext *context = (ReaderContext *)feeder;

	if (context == NULL) {
		CVI_VC_ERR("Null handle\n");
		return FALSE;
	}

	if (context->eos == TRUE) {
		return 0;
	}

	if (chunk->size == 0) {
		context->eos = TRUE;
	}

	CVI_VC_TRACE("size = %d\n", chunk->size);

	return chunk->size;
}

BOOL BSFeederEsIn_Rewind(void)
{
	return TRUE;
}
