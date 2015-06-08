#include "AudioRecord.h"
#include "jnilogger.h"
#include <stdlib.h>
/* Size of the recording buffer queue */
#define NB_BUFFERS_IN_QUEUE 1

/* Explicitly requesting SL_IID_ANDROIDSIMPLEBUFFERQUEUE and SL_IID_ANDROIDCONFIGURATION
 * on the AudioRecorder object */
#define NUM_EXPLICIT_INTERFACES_FOR_RECORDER 2

/* Size of the recording buffer queue */
#define NB_BUFFERS_IN_QUEUE 1
/* Size of each buffer in the queue */
#define BUFFER_SIZE_IN_SAMPLES 8192
#define BUFFER_SIZE_IN_BYTES   (2 * BUFFER_SIZE_IN_SAMPLES)

/* Local storage for Audio data */
int8_t pcmData[NB_BUFFERS_IN_QUEUE * BUFFER_SIZE_IN_BYTES];

/* Callback for recording buffer queue events */
void recCallback(SLRecordItf caller, void *pContext, SLuint32 event) {
	if (SL_RECORDEVENT_HEADATNEWPOS & event) {
		SLmillisecond pMsec = 0;
		(*caller)->GetPosition(caller, &pMsec);
		LOGI("SL_RECORDEVENT_HEADATNEWPOS current position=%ums\n", pMsec);
	}

	if (SL_RECORDEVENT_HEADATMARKER & event) {
		SLmillisecond pMsec = 0;
		(*caller)->GetPosition(caller, &pMsec);
 		LOGI("SL_RECORDEVENT_HEADATMARKER current position=%ums\n", pMsec);
	}
}

/* Callback for recording buffer queue events */
void recBufferQueueCallback(SLAndroidSimpleBufferQueueItf queueItf, void *pContext) {

	CallbackCntxt *pCntxt = (CallbackCntxt*) pContext;

	/* Save the recorded data  */
	fwrite(pCntxt->pDataBase, BUFFER_SIZE_IN_BYTES, 1, pCntxt->pfile);

	/* Increase data pointer by buffer size */
	pCntxt->pData += BUFFER_SIZE_IN_BYTES;

	if (pCntxt->pData >= pCntxt->pDataBase + (NB_BUFFERS_IN_QUEUE * BUFFER_SIZE_IN_BYTES)) {
		pCntxt->pData = pCntxt->pDataBase;
	}

	(*queueItf)->Enqueue(queueItf, pCntxt->pDataBase, BUFFER_SIZE_IN_BYTES);

	SLAndroidSimpleBufferQueueState recQueueState;
	(*queueItf)->GetState(queueItf, &recQueueState);

}

AudioRecord::AudioRecord(const char *fileName, SLDataFormat_PCM pcm):
        engineObject(NULL),engineEngine(NULL),recorderObject(NULL),
        recordItf(NULL),recBuffQueueItf(NULL),configItf(NULL){
    SLEngineOption EngineOption[] = {
                {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
        };
    SLresult result;
    memset(&ctx,0, sizeof(CallbackCntxt));
    if(!fileName){
        LOGE("FileName NULL");
        return;
    }
    ctx.pfile = fopen(fileName, "w");
    if(!(ctx.pfile)){
        LOGE("Open file error");
        return;
    }
    result = slCreateEngine(&engineObject, 1, EngineOption, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);

     /* Realizing the SL Engine in synchronous mode. */
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);

    /* setup the data source*/
        SLDataLocator_IODevice ioDevice = {
                SL_DATALOCATOR_IODEVICE,
                SL_IODEVICE_AUDIOINPUT,
                SL_DEFAULTDEVICEID_AUDIOINPUT,
                NULL
        };

        SLDataSource recSource = {&ioDevice, NULL};

        SLDataLocator_AndroidSimpleBufferQueue recBufferQueue = {
                SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                NB_BUFFERS_IN_QUEUE
        };

        SLDataSink dataSink = { &recBufferQueue, &pcm };
        SLInterfaceID iids[NUM_EXPLICIT_INTERFACES_FOR_RECORDER] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION};
        SLboolean required[NUM_EXPLICIT_INTERFACES_FOR_RECORDER] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

        /* Create the audio recorder */
        result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject , &recSource, &dataSink,
                NUM_EXPLICIT_INTERFACES_FOR_RECORDER, iids, required);
        assert(SL_RESULT_SUCCESS == result);


        /* get the android configuration interface*/
        result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDCONFIGURATION, &configItf);
        assert(SL_RESULT_SUCCESS == result);

        /* Realize the recorder in synchronous mode. */
        result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
        assert(SL_RESULT_SUCCESS == result);

        /* Get the buffer queue interface which was explicitly requested */
        result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, (void*) &recBuffQueueItf);
        assert(SL_RESULT_SUCCESS == result);


        /* get the record interface */
        result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recordItf);
        assert(SL_RESULT_SUCCESS == result);


        /* Set up the recorder callback to get events during the recording */
        result = (*recordItf)->SetMarkerPosition(recordItf, 2000);
        assert(SL_RESULT_SUCCESS == result);

        result = (*recordItf)->SetPositionUpdatePeriod(recordItf, 500);
        assert(SL_RESULT_SUCCESS == result);

        result = (*recordItf)->SetCallbackEventsMask(recordItf,
                    SL_RECORDEVENT_HEADATMARKER | SL_RECORDEVENT_HEADATNEWPOS);
        assert(SL_RESULT_SUCCESS == result);

        result = (*recordItf)->RegisterCallback(recordItf, recCallback, NULL);
        assert(SL_RESULT_SUCCESS == result);

        /* Initialize the callback and its context for the recording buffer queue */

        ctx.pDataBase = (int8_t*) &pcmData;
        ctx.pData = ctx.pDataBase;
        ctx.size = sizeof(pcmData);
        result = (*recBuffQueueItf)->RegisterCallback(recBuffQueueItf, recBufferQueueCallback, &ctx);
        assert(SL_RESULT_SUCCESS == result);

        /* Enqueue buffers to map the region of memory allocated to store the recorded data */
        LOGI("Enqueueing buffer ");
        for (int i = 0; i < NB_BUFFERS_IN_QUEUE; i++) {
            LOGI("%d ", i);
            result = (*recBuffQueueItf)->Enqueue(recBuffQueueItf, ctx.pData, BUFFER_SIZE_IN_BYTES);
            assert(SL_RESULT_SUCCESS == result);
            ctx.pData += BUFFER_SIZE_IN_BYTES;
        }
        ctx.pData = ctx.pDataBase;
}

AudioRecord::~AudioRecord(){
    release();
}

void AudioRecord::start(){
    SLresult result;
    if (recordItf != NULL) {
        result = (*recordItf)->SetRecordState(recordItf, SL_RECORDSTATE_RECORDING);
        assert(SL_RESULT_SUCCESS == result);
        LOGI("Starting to record");
    }
}
void AudioRecord::pause(){
    SLresult result;
    if (recordItf != NULL) {
        result = (*recordItf)->SetRecordState(recordItf, SL_RECORDSTATE_PAUSED);
        assert(SL_RESULT_SUCCESS == result);
        LOGI("pause to record");
    }
}
void AudioRecord::stop(){
    if (recordItf != NULL) {
        SLresult result = (*recordItf)->SetRecordState(recordItf, SL_RECORDSTATE_STOPPED);
        assert(SL_RESULT_SUCCESS == result);
        LOGI("Stop to record");
    }
}

void AudioRecord::release(){
    //destroy recorder object , and invlidate all associated interfaces
    if (recorderObject != NULL) {
        (*recorderObject)->Destroy(recorderObject);
        recorderObject = NULL;
        recordItf = NULL;
        recBuffQueueItf = NULL;
        configItf = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }

    //colse the file
    if ((ctx.pfile != NULL)) {
        fclose(ctx.pfile);
        ctx.pfile == NULL;
    }
    LOGI("release record");
}
