/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server (Audio Output)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/server/audin.h>

#include "mf_info.h"
#include "mf_rdpsnd.h"

AQRecorderState recorderState;

static const rdpsndFormat audio_formats[] =
{
	{ 0x11, 2, 22050, 1024, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 2 channels */
	{ 0x11, 1, 22050, 512, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 1 channels */
	{ 0x01, 2, 22050, 4, 16, 0, NULL }, /* PCM, 22050 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 22050, 2, 16, 0, NULL }, /* PCM, 22050 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 44100, 4, 16, 0, NULL }, /* PCM, 44100 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 44100, 2, 16, 0, NULL }, /* PCM, 44100 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 11025, 4, 16, 0, NULL }, /* PCM, 11025 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 11025, 2, 16, 0, NULL }, /* PCM, 11025 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 8000, 4, 16, 0, NULL }, /* PCM, 8000 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 8000, 2, 16, 0, NULL } /* PCM, 8000 Hz, 1 channels, 16 bits */
};

static void mf_peer_rdpsnd_activated(rdpsnd_server_context* context)
{
	printf("RDPSND Activated\n");
    
    
    
    printf("Let's create an audio queue for input!\n");
    
    OSStatus status;
    
    recorderState.dataFormat.mSampleRate = 44100.0;
    recorderState.dataFormat.mFormatID = kAudioFormatLinearPCM;
    recorderState.dataFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
    recorderState.dataFormat.mBytesPerPacket = 4;
    recorderState.dataFormat.mFramesPerPacket = 1;
    recorderState.dataFormat.mBytesPerFrame = 4;
    recorderState.dataFormat.mChannelsPerFrame = 2;
    recorderState.dataFormat.mBitsPerChannel = 16;
    
    recorderState.snd_context = context;
    
    status = AudioQueueNewInput(&recorderState.dataFormat,
                                mf_peer_rdpsnd_input_callback,
                                &recorderState,
                                NULL,
                                kCFRunLoopCommonModes,
                                0,
                                &recorderState.queue);
    
    if (status != noErr)
    {
        printf("Failed to create a new Audio Queue. Status code: %d\n", status);
    }
    
    
    UInt32 dataFormatSize = sizeof (recorderState.dataFormat);
    
    AudioQueueGetProperty(recorderState.queue,
                          kAudioConverterCurrentInputStreamDescription,
                          &recorderState.dataFormat,
                          &dataFormatSize);
    
    
    mf_rdpsnd_derive_buffer_size(recorderState.queue, &recorderState.dataFormat, 0.05, &recorderState.bufferByteSize);
    
    
    printf("Preparing a set of buffers...");
    
    for (int i = 0; i < snd_numBuffers; ++i)
    {
        AudioQueueAllocateBuffer(recorderState.queue,
                                 recorderState.bufferByteSize,
                                 &recorderState.buffers[i]);
        
        AudioQueueEnqueueBuffer(recorderState.queue,
                                recorderState.buffers[i],
                                0,
                                NULL);
    }
    
    printf("done\n");
    
    printf("recording...\n");
    
    
    
    recorderState.currentPacket = 0;
    recorderState.isRunning = true;
    
    context->SelectFormat(context, 4);
    context->SetVolume(context, 0x7FFF, 0x7FFF);
    
    AudioQueueStart (recorderState.queue, NULL);

}

BOOL mf_peer_rdpsnd_init(mfPeerContext* context)
{
    //printf("RDPSND INIT\n");
	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	context->rdpsnd->data = context;

	context->rdpsnd->server_formats = audio_formats;
	context->rdpsnd->num_server_formats = sizeof(audio_formats) / sizeof(audio_formats[0]);

	context->rdpsnd->src_format.wFormatTag = 1;
	context->rdpsnd->src_format.nChannels = 2;
	context->rdpsnd->src_format.nSamplesPerSec = 44100;
	context->rdpsnd->src_format.wBitsPerSample = 16;

	context->rdpsnd->Activated = mf_peer_rdpsnd_activated;

	context->rdpsnd->Initialize(context->rdpsnd);

	return TRUE;
}

BOOL mf_peer_rdpsnd_stop()
{
    recorderState.isRunning = false;
    AudioQueueStop(recorderState.queue, true);
    
    return TRUE;
}

void mf_peer_rdpsnd_input_callback (void                                *inUserData,
                                    AudioQueueRef                       inAQ,
                                    AudioQueueBufferRef                 inBuffer,
                                    const AudioTimeStamp                *inStartTime,
                                    UInt32                              inNumberPacketDescriptions,
                                    const AudioStreamPacketDescription  *inPacketDescs)
{
    OSStatus status;
    AQRecorderState * rState;
    rState = inUserData;
    
    
    if (inNumberPacketDescriptions == 0 && rState->dataFormat.mBytesPerPacket != 0)
    {
        inNumberPacketDescriptions = inBuffer->mAudioDataByteSize / rState->dataFormat.mBytesPerPacket;
    }
    
    
    if (rState->isRunning == 0)
    {
        return ;
    }
    
    rState->snd_context->SendSamples(rState->snd_context, inBuffer->mAudioData, inBuffer->mAudioDataByteSize/4);
    
    status = AudioQueueEnqueueBuffer(
                                     rState->queue,
                                     inBuffer,
                                     0,
                                     NULL);
    
    if (status != noErr)
    {
        printf("AudioQueueEnqueueBuffer() returned status = %d\n", status);
    }
    
}

void mf_rdpsnd_derive_buffer_size (AudioQueueRef                audioQueue,
                                   AudioStreamBasicDescription  *ASBDescription,
                                   Float64                      seconds,
                                   UInt32                       *outBufferSize)
{
    static const int maxBufferSize = 0x50000;
    
    int maxPacketSize = ASBDescription->mBytesPerPacket;
    if (maxPacketSize == 0)
    {
        UInt32 maxVBRPacketSize = sizeof(maxPacketSize);
        AudioQueueGetProperty (audioQueue,
                               kAudioQueueProperty_MaximumOutputPacketSize,
                               // in Mac OS X v10.5, instead use
                               //   kAudioConverterPropertyMaximumOutputPacketSize
                               &maxPacketSize,
                               &maxVBRPacketSize
                               );
    }
    
    Float64 numBytesForTime =
    ASBDescription->mSampleRate * maxPacketSize * seconds;
    *outBufferSize = (UInt32) (numBytesForTime < maxBufferSize ? numBytesForTime : maxBufferSize);
}

