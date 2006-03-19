#include <assert.h>

#include "utl/UtlString.h"
#include "tapi/sipXtapiExt.h"
#include "tapi/sipXtapiEvents.h"
#include "tapi/sipXtapiInternalExt.h"
#include "tapi/SipXHandleMap.h"
#include "net/Url.h"
#include "mp/MpMediaTask.h"
#include "mp/NetInTask.h"
#include "mp/MpCodec.h"
#include "mp/DmaTask.h"
#include "mp/MprFromMic.h"
#include "mp/MprToSpkr.h"
#include "mp/MpFlowGraphBase.h"
#include "mp/MprDejitter.h"
#include "mp/MprDecode.h"
#include "mp/MpDecoderBase.h"
#include "mp/MpdSipxPcma.h"
#include "mp/MpJitterBuffer.h"

#include "os/OsBeginThread.h"

#include "rtcp/RTCManager.h"
#include "net/SipUserAgent.h"
#include "net/SdpCodecFactory.h"
#include "cp/CallManager.h"
#include "ptapi/PtProvider.h"
#include "net/Url.h"
#include "net/NameValueTokenizer.h"
#include "os/OsConfigDb.h"
#include "net/SipLineMgr.h"
#include "net/SipRefreshMgr.h"
#include "net/NetMd5Codec.h"


#include <stdstring.h>


extern SipXHandleMap* gpLineHandleMap ;   // sipXtapiInternal.cpp








SIPXTAPI_API SIPX_RESULT sipxQOSDebug(SIPX_INST phInst, CStdString& txt) {
	CStdString buff;

#ifdef INCLUDE_RTCP
	IRTCPControl* ic = CRTCManager::getRTCPControl();
	IRTCPSession* sess = ic->GetFirstSession();
	while (sess) {
		buff.Format("Sess: %d ", sess->GetSessionID());
		txt += buff;
		IRTCPConnection* conn = sess->GetFirstConnection();
		while (conn) {
			txt += "Conn\n";
			CRTCPRender* render = (CRTCPRender*)((CRTCPConnection*)conn)->GetRenderInterface();
			IGetSrcDescription* statDesc;
			IGetSenderStatistics* statSender;
			IGetReceiverStatistics* statRcvr;
			IGetByeInfo* statBye;
			render->GetStatistics(&statDesc, &statSender, &statRcvr, &statBye);
			char appName [255];
			statDesc->GetAppName((unsigned char*)appName);
			unsigned long packetCount, octetCount;
			statSender->GetSenderStatistics(&packetCount, &octetCount);
			unsigned long fractionalLoss, cumulativeLoss, highestSeq, interarrivalJitter, SRtimestamp, packetDelay;
			statRcvr->GetReceiverStatistics(&fractionalLoss, &cumulativeLoss, &highestSeq, &interarrivalJitter, &SRtimestamp, &packetDelay);

			buff.Format("app=%s, packet=%d, octet=%d, loss=%d, cLoss=%d, seq=%d, jitt=%d, SR=%d, delay=%d", appName, packetCount, octetCount, fractionalLoss, cumulativeLoss, highestSeq, interarrivalJitter, SRtimestamp, packetDelay);
			txt += buff + "\n";
			conn = sess->GetNextConnection();
		}

		sess = ic->GetNextSession();
	}
#endif 

	MpMediaTask* mtask = MpMediaTask::getMediaTask(32);
	MpFlowGraphBase* flowGraph = mtask->getFocus();
	/*
	if (flowGraph) {
		for (int i=0; i < flowGraph->mResourceCnt; i++) {
			MpResource* r = flowGraph->mExecOrder[i];
			if (strstr(r->getName(), "Dejitter")) {
				MprDejitter* dejj = (MprDejitter*) r;
				buff.Format("<u>%s</u>:: ave=%d, buff=%d, pull=%d, push=%d, " 
					//"lmax=%d, lmin=%d, " 
					"disc=%d, packs=%d ", dejj->getName().data(), dejj->getAveBufferLength(), dejj->mBufferLength, dejj->mLastPulled, dejj->mLastPushed 
					//, dejj->mLatencyMax, dejj->mLatencyMin
					, dejj->mNumDiscarded, dejj->mNumPackets);
				txt += buff + "<br/>";
			} else if (strstr(r->getName(), "Decode")) {
				MprDecode* decode = (MprDecode*) r;

				for (int c=0; c < decode->mNumCurrentCodecs; c++) {
					MpDecoderBase* mpd = decode->mpCurrentCodecs[c];
					if (mpd->getInfo()->getCodecType() == SdpCodec::SDP_CODEC_PCMU || mpd->getInfo()->getCodecType() == SdpCodec::SDP_CODEC_PCMA) {
						MpdSipxPcma* pcma = (MpdSipxPcma*)mpd;
						MpJitterBuffer* jb = (MpJitterBuffer*)pcma->pJBState;
						buff.Format("Codec::%d tci=%d, pull=%d, wait=%d, under=%d, seq=%d, few=%d, many=%d, last=%d", pcma->getInfo()->getCodecType(), pcma->mTimerCountIncrement, pcma->mNextPullTimerCount, pcma->mWaitTimeInFrames, pcma->mUnderflowCount, pcma->mLastSeqNo, pcma->mTooFewPacketsInBuffer, pcma->mTooManyPacketsInBuffer, pcma->mLastReportSize);
						txt += buff + ", JB:";
						buff.Format("avail=%d, qc=%d, qi=%d, qo=%d", jb->JbPacketsAvail, jb->JbQCount, jb->JbQIn, jb->JbQOut);
						txt += buff + "<br/>";
					}
				}
			} else if (stristr(r->getName(), "FromNet")) {
				MprFromNet* fromNet = (MprFromNet*) r;
			}
		}
	}
	*/


	int rating = 0;
	sipxQOSRating(phInst, rating);
	buff.Format("Rating: <b>%d</b>", rating);
	txt += buff;

/*
MprDejitter::getAveBufferLength -> MprDejitter -> MprDecode::getMyDejitter / MprFromNet::getMyDejitter()
MpdSipxPcmu -> MpConnection::mapPayloadType  MpCodecFactory::createDecoder 
MpdSipxPcma (MpDecoderBase)
MpJitterBuffer -> MpConnection::getJBinst

MpCallFlowGraph::getConnectionPtr
*/
	return SIPX_RESULT_SUCCESS;
}





/**
The best possible rating is 1 (0 means no result!). 
The optimum is 100, more than 200 means horrible conditions!
*/
SIPXTAPI_API SIPX_RESULT sipxQOSRating(SIPX_INST phInst, int &rating) {
	rating = 0;
	/*  fractional rates for calculating the overall rating
	*/
	const int maxRateCount = 30;
	int rate[maxRateCount + 2]; 
	int rateCount = 0;
	for (int i=0; i< sizeof(rate) / sizeof(int); i++) {
		rate[i] = 0;
	}
/*
	IRTCPControl* ic = CRTCManager::getRTCPControl();
	IRTCPSession* sess = ic->GetFirstSession();
	while (sess) {
		IRTCPConnection* conn = sess->GetFirstConnection();
		while (conn) {
			CRTCPRender* render = (CRTCPRender*)((CRTCPConnection*)conn)->GetRenderInterface();
			IGetSrcDescription* statDesc;
			IGetSenderStatistics* statSender;
			IGetReceiverStatistics* statRcvr;
			IGetByeInfo* statBye;
			render->GetStatistics(&statDesc, &statSender, &statRcvr, &statBye);
			unsigned long fractionalLoss, cumulativeLoss, highestSeq, interarrivalJitter, SRtimestamp, packetDelay;
			statRcvr->GetReceiverStatistics(&fractionalLoss, &cumulativeLoss, &highestSeq, &interarrivalJitter, &SRtimestamp, &packetDelay);

			if (rateCount >= maxRateCount) break;

			rate[rateCount++] = fractionalLoss + 1; // 0 - 255 ?
			rate[rateCount++] = max(1, interarrivalJitter - 50);
            
			conn = sess->GetNextConnection();
		}

		sess = ic->GetNextSession();
	}
	*/
/*
	MpMediaTask* mtask = MpMediaTask::getMediaTask(32);
	MpFlowGraphBase* flowGraph = mtask->getFocus();
	if (flowGraph) {
		for (int i=0; i < flowGraph->mResourceCnt; i++) {
			if (rateCount >= maxRateCount) break;
			MpResource* r = flowGraph->mExecOrder[i];
			if (strstr(r->getName(), "Dejitter")) {
				MprDejitter* dejj = (MprDejitter*) r;

				rate[rateCount++] = dejj->getAveBufferLength(0) * 3;
				//rate[rateCount++] = dejj->mLatencyMax * 2;

			} else if (strstr(r->getName(), "Decode")) {
				MprDecode* decode = (MprDecode*) r;

				for (int c=0; c < decode->mNumCurrentCodecs; c++) {
					if (rateCount >= maxRateCount) break;

					MpDecoderBase* mpd = decode->mpCurrentCodecs[c];
					if (!mpd) continue;
					if (mpd->getInfo()->getCodecType() == SdpCodec::SDP_CODEC_PCMU || mpd->getInfo()->getCodecType() == SdpCodec::SDP_CODEC_PCMA) {
						MpdSipxPcma* pcma = (MpdSipxPcma*)mpd;
						MpJitterBuffer* jb = (MpJitterBuffer*)pcma->pJBState;
						rate[rateCount++] = (pcma->mUnderflowCount * 10) +1;

					}
				}

			}
		}
	}
*/
	CStdString buff, nfo = "Rating: ";
	int rated = 0;
	for (int i=0; i < rateCount; i++) {
		//buff.Format(" %d ", rate[i]);
		if (rate[i] <= 0) continue;		
		rating += rate[i];
		rated++;
		nfo+=buff;
	}
	if (rated)
		rating = rating / rated;
	/*
	buff.Format(" = %d", rating);
	nfo+=buff;
	OutputDebugString(nfo + "\n");        
	*/

	return SIPX_RESULT_SUCCESS;
}


SIPXTAPI_API void sipxStartMedia() {
	mpStartTasks();
}
SIPXTAPI_API void sipxStopMedia() {
    mpStopTasks();
}


SIPXTAPI_API SIPX_RESULT sipxLineGetByUrl(const char* szLineUrl,
                                     SIPX_LINE* phLine)

{
    assert(szLineUrl != NULL) ;
    assert(phLine != NULL) ;
	*phLine = sipxLineLookupHandleByURI(szLineUrl);
	return (phLine) ? SIPX_RESULT_SUCCESS : SIPX_RESULT_FAILURE;
}





