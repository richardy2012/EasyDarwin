/*
	Copyright (c) 2013-2015 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
/*
    File:       EasyRelaySession.cpp
    Contains:   RTSP Relay Client
*/
#include "EasyRelaySession.h"
#include <stdlib.h>

/* NVSource从RTSPClient获取数据后回调给上层 */
int CALLBACK __EasyNVSourceCallBack( int _chid, int *_chPtr, int _mediatype, char *pbuf, NVS_FRAME_INFO *frameinfo)
{
	EasyRelaySession* pRelaySession = (EasyRelaySession*)_chPtr;

	if (NULL == pRelaySession)	return -1;

	if (NULL != frameinfo)
	{
		if (frameinfo->height==1088)		frameinfo->height=1080;
		else if (frameinfo->height==544)	frameinfo->height=540;
	}

	//投递到具体的EasyHLSSession进行处理
	pRelaySession->ProcessData(_chid, _mediatype, pbuf, frameinfo);

	return 0;
}

int __EasyPusher_Callback(int _id, EASY_PUSH_STATE_T _state, EASY_AV_Frame *_frame, void *_userptr)
{
    if (_state == EASY_PUSH_STATE_CONNECTING)               printf("Connecting...\n");
    else if (_state == EASY_PUSH_STATE_CONNECTED)           printf("Connected\n");
    else if (_state == EASY_PUSH_STATE_CONNECT_FAILED)      printf("Connect failed\n");
    else if (_state == EASY_PUSH_STATE_CONNECT_ABORT)       printf("Connect abort\n");
    //else if (_state == EASY_PUSH_STATE_PUSHING)             printf("P->");
    else if (_state == EASY_PUSH_STATE_DISCONNECTED)        printf("Disconnect.\n");

    return 0;
}

EasyRelaySession::EasyRelaySession(char* inURL, ClientType inClientType, const char* streamName)
:   fStreamName(NULL),
	fURL(NULL),
	fNVSHandle(NULL),
	fPusherHandle(NULL)
{
    this->SetTaskName("EasyRelaySession");

	//建立NVSourceClient
    StrPtrLen theURL(inURL);

	fURL = NEW char[::strlen(inURL) + 2];
	::strcpy(fURL, inURL);

	if (streamName != NULL)
    {
		fStreamName.Ptr = NEW char[strlen(streamName) + 1];
		::memset(fStreamName.Ptr,0,strlen(streamName) + 1);
		::memcpy(fStreamName.Ptr, streamName, strlen(streamName));
		fStreamName.Len = strlen(streamName);
		fRef.Set(fStreamName, this);
    }

	qtss_printf("\nNew Connection %s:%s\n",fStreamName.Ptr,fURL);
}

EasyRelaySession::~EasyRelaySession()
{
	qtss_printf("\nDisconnect %s:%s\n",fStreamName.Ptr,fURL);

	delete [] fStreamName.Ptr;

	qtss_printf("Disconnect complete\n");
}

SInt64 EasyRelaySession::Run()
{
    EventFlags theEvents = this->GetEvents();

	if (theEvents & Task::kKillEvent)
    {
        return -1;
    }

    return 0;
}


QTSS_Error EasyRelaySession::ProcessData(int _chid, int mediatype, char *pbuf, NVS_FRAME_INFO *frameinfo)
{
	//if(NULL == fHLSHandle) return QTSS_Unimplemented;
	//if (mediatype == MEDIA_TYPE_VIDEO)
	//{
	//	printf("Get %s Video Len:%d tm:%d rtp:%d\n",frameinfo->type==FRAMETYPE_I?"I":"P", frameinfo->length, frameinfo->timestamp_sec, frameinfo->rtptimestamp);
	//	
	//	fwrite(pbuf, 1, frameinfo->length, fTest);

	//	if(frameinfo->fps == 0) frameinfo->fps = 25;
	//	tsTimeStampMSsec += 1000/frameinfo->fps;
	//	unsigned long long llPts = tsTimeStampMSsec * 90;
	//
	//	unsigned int uiFrameType = 0;
	//	if (frameinfo->type == FRAMETYPE_I)
	//	{
	//		uiFrameType = TS_TYPE_PES_VIDEO_I_FRAME;
	//	}
	//	else if (frameinfo->type == FRAMETYPE_P)
	//	{
	//		uiFrameType = TS_TYPE_PES_VIDEO_P_FRAME;
	//	}
	//	else
	//	{
	//		return QTSS_OutOfState;
	//	}

	//	EasyHLS_VideoMux(fHLSHandle, uiFrameType, (unsigned char*)pbuf, frameinfo->length, llPts, llPts, llPts);
	//}
	//else if (mediatype == MEDIA_TYPE_AUDIO)
	//{
	//	printf("Get Audio Len:%d tm:%d rtp:%d\n", frameinfo->length, frameinfo->timestamp_sec, frameinfo->rtptimestamp);
	//	// 暂时不对音频进行处理
	//}
	//else if (mediatype == MEDIA_TYPE_EVENT)
	//{
	//	if (NULL == pbuf && NULL == frameinfo)
	//	{
	//		printf("Connecting:%s ...\n", fHLSSessionID.Ptr);
	//	}
	//	else if (NULL!=frameinfo && frameinfo->type==0xF1)
	//	{
	//		printf("Lose Packet:%s ...\n", fHLSSessionID.Ptr);
	//	}
	//}

	return QTSS_NoErr;
}

/*
	创建HLS直播Session
*/
QTSS_Error	EasyRelaySession::HLSSessionStart(char* rtspUrl)
{
	if(NULL == fNVSHandle)
	{
		//创建NVSource
		EasyNVS_Init(&fNVSHandle);

		if (NULL == fNVSHandle) return QTSS_Unimplemented;

		unsigned int mediaType = MEDIA_TYPE_VIDEO;
		//mediaType |= MEDIA_TYPE_AUDIO;	//换为NVSource, 屏蔽声音

		EasyNVS_SetCallback(fNVSHandle, __EasyNVSourceCallBack);
		EasyNVS_OpenStream(fNVSHandle, 0, rtspUrl,RTP_OVER_TCP, mediaType, 0, 0, this, 1000, 0);
	}

	if(NULL == fPusherHandle)
	{
		EASY_MEDIA_INFO_T mediainfo;
		memset(&mediainfo, 0x00, sizeof(EASY_MEDIA_INFO_T));
		mediainfo.u32VideoCodec =   0x1C;

		fPusherHandle = EasyPusher_Create();

		EasyPusher_SetEventCallback(fPusherHandle, __EasyPusher_Callback, 0, NULL);

		EasyPusher_StartStream(fPusherHandle, "127.0.0.1", 554, "live.sdp", "", "", &mediainfo, 512);
	}

	return QTSS_NoErr;
}

QTSS_Error	EasyRelaySession::HLSSessionRelease()
{
	qtss_printf("HLSSession Release....\n");
	
	//释放source
	if(fNVSHandle)
	{
		EasyNVS_CloseStream(fNVSHandle);
		EasyNVS_Deinit(&fNVSHandle);
		fNVSHandle = NULL;
	}

	//释放sink
	if(fPusherHandle)
	{
		EasyPusher_StopStream(fPusherHandle);
		EasyPusher_Release(fPusherHandle);
		fPusherHandle = 0;
	}

	return QTSS_NoErr;
}