/*
 *      Copyright (C) 2010 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#elif defined(_WIN32)
#include "system.h"
#endif

#include "OMXVideo.h"

#include "OMXStreamInfo.h"
#include "utils/log.h"
#include "linux/XMemUtils.h"

#include <sys/time.h>
#include <inttypes.h>

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "COMXVideo"

#if 0
// TODO: These are Nvidia Tegra2 dependent, need to dynamiclly find the
// right codec matched to video format.
#define OMX_H264BASE_DECODER    "OMX.Nvidia.h264.decode"
// OMX.Nvidia.h264ext.decode segfaults, not sure why.
//#define OMX_H264MAIN_DECODER  "OMX.Nvidia.h264ext.decode"
#define OMX_H264MAIN_DECODER    "OMX.Nvidia.h264.decode"
#define OMX_H264HIGH_DECODER    "OMX.Nvidia.h264ext.decode"
#define OMX_MPEG4_DECODER       "OMX.Nvidia.mp4.decode"
#define OMX_MPEG4EXT_DECODER    "OMX.Nvidia.mp4ext.decode"
#define OMX_MPEG2V_DECODER      "OMX.Nvidia.mpeg2v.decode"
#define OMX_VC1_DECODER         "OMX.Nvidia.vc1.decode"
#endif

#define OMX_VIDEO_DECODER       "OMX.broadcom.video_decode"
#define OMX_H264BASE_DECODER    OMX_VIDEO_DECODER
#define OMX_H264MAIN_DECODER    OMX_VIDEO_DECODER
#define OMX_H264HIGH_DECODER    OMX_VIDEO_DECODER
#define OMX_MPEG4_DECODER       OMX_VIDEO_DECODER
#define OMX_MSMPEG4V1_DECODER   OMX_VIDEO_DECODER
#define OMX_MSMPEG4V2_DECODER   OMX_VIDEO_DECODER
#define OMX_MSMPEG4V3_DECODER   OMX_VIDEO_DECODER
#define OMX_MPEG4EXT_DECODER    OMX_VIDEO_DECODER
#define OMX_MPEG2V_DECODER      OMX_VIDEO_DECODER
#define OMX_VC1_DECODER         OMX_VIDEO_DECODER
#define OMX_WMV3_DECODER        OMX_VIDEO_DECODER
#define OMX_VP8_DECODER         OMX_VIDEO_DECODER

COMXVideo::COMXVideo()
{
  m_is_open       = false;
  m_Pause         = false;
  m_setStartTime  = true;

  m_extradata     = NULL;
  m_extrasize     = 0;

  m_converter     = NULL;
  m_video_convert = false;
  m_video_codec_name = "";
}

COMXVideo::~COMXVideo()
{
  if (m_is_open)
    Close();
}

bool COMXVideo::Open(COMXStreamInfo &hints, OMXClock *clock)
{
  m_video_codec_name = "";
  m_codingType = OMX_VIDEO_CodingUnused;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  std::string decoder_name;

  m_decoded_width  = hints.width;
  m_decoded_height = hints.height;

  m_converter     = new CBitstreamConverter();
  m_video_convert = m_converter->Open(hints.codec, (uint8_t *)hints.extradata, hints.extrasize, false);

  if(m_video_convert)
  {
    if(m_converter->GetExtraData() != NULL && m_converter->GetExtraSize() > 0)
    {
      m_extrasize = m_converter->GetExtraSize();
      m_extradata = (uint8_t *)malloc(m_extrasize);
      memcpy(m_extradata, m_converter->GetExtraData(), m_converter->GetExtraSize());
    }
  }
  else
  {
    if(hints.extrasize > 0 && hints.extradata != NULL)
    {
      m_extrasize = hints.extrasize;
      m_extradata = (uint8_t *)malloc(m_extrasize);
      memcpy(m_extradata, hints.extradata, hints.extrasize);
    }
  }

  switch (hints.codec)
  {
    case CODEC_ID_H264:
    {
      if(m_extrasize < 8)
      {
        CLog::Log(LOGERROR, "COMXVideo::Open h264 extradata < 8\n");
        if(m_extradata)
          free(m_extradata);
        m_extradata = NULL;
        m_extrasize = 0;
        if(m_converter)
          delete m_converter;
        m_converter = NULL;
        return false;
      }

      bool    interlaced = true;
      int32_t m_max_ref_frames;
      uint8_t *spc = m_extradata + 6;
      uint32_t sps_size = OMX_RB16(spc);
      if (sps_size)
        m_converter->parseh264_sps(spc+3, sps_size-1, &interlaced, &m_max_ref_frames);
      if (interlaced)
      {
        CLog::Log(LOGINFO, "COMXVideo::Open h264 interlaced not supported\n");
        if(m_extradata)
          free(m_extradata);
        m_extradata = NULL;
        m_extrasize = 0;
        if(m_converter)
          delete m_converter;
        m_converter = NULL;
        return false;
      }
 
      switch(hints.profile)
      {
        case FF_PROFILE_H264_BASELINE:
          // (role name) video_decoder.avc
          // H.264 Baseline profile
          decoder_name = OMX_H264BASE_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        case FF_PROFILE_H264_MAIN:
          // (role name) video_decoder.avc
          // H.264 Main profile
          decoder_name = OMX_H264MAIN_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        case FF_PROFILE_H264_HIGH:
          // (role name) video_decoder.avc
          // H.264 Main profile
          decoder_name = OMX_H264HIGH_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        case FF_PROFILE_UNKNOWN:
          decoder_name = OMX_H264HIGH_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        default:
          return false;
          break;
      }
    }
    break;
    case CODEC_ID_MSMPEG4V1:
      // (role name) video_decoder.mpeg4
      // MPEG-4, DivX 4/5 and Xvid compatible
      decoder_name = OMX_MSMPEG4V1_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG4;
      m_video_codec_name = "omx-mpeg4";
    break;
    case CODEC_ID_MSMPEG4V2:
      // (role name) video_decoder.mpeg4
      // MPEG-4, DivX 4/5 and Xvid compatible
      decoder_name = OMX_MSMPEG4V2_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG4;
      m_video_codec_name = "omx-mpeg4";
      break;
    case CODEC_ID_MSMPEG4V3:
      // (role name) video_decoder.mpeg4
      // MPEG-4, DivX 4/5 and Xvid compatible
      decoder_name = OMX_MSMPEG4V3_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG4;
      m_video_codec_name = "omx-mpeg4";
      break;
    case CODEC_ID_MPEG4:
      // (role name) video_decoder.mpeg4
      // MPEG-4, DivX 4/5 and Xvid compatible
      decoder_name = OMX_MPEG4_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG4;
      m_video_codec_name = "omx-mpeg4";
      break;
    case CODEC_ID_MPEG2VIDEO:
      // (role name) video_decoder.mpeg2
      // MPEG-2
      decoder_name = OMX_MPEG2V_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG2;
      m_video_codec_name = "omx-mpeg2";
      break;
    case CODEC_ID_VP8:
      // (role name) video_decoder.vp8
      // VP8
      /*
      if(hints.width > 720 || hints.height > 576)
      {
        return false;
      }
      */
      decoder_name = OMX_VP8_DECODER;
      m_codingType = OMX_VIDEO_CodingVP8;
    break;
    /*
    case CODEC_ID_VC1:
      // (role name) video_decoder.vc1
      // VC-1, WMV9
      decoder_name = OMX_VC1_DECODER;
      m_codingType = OMX_VIDEO_CodingWMV;
      break;
    case CODEC_ID_WMV3:
      // (role name) video_decoder.wmv3
      //WMV3
      decoder_name = OMX_WMV3_DECODER;
      m_codingType = OMX_VIDEO_CodingWMV;
      break;
    */
    default:
      return false;
    break;
  }

  CStdString componentName = "";
  OMX_CALLBACKTYPE callbacks;

  callbacks.EventHandler      = NULL;
  callbacks.EmptyBufferDone   = NULL;
  callbacks.FillBufferDone    = NULL;

  componentName = decoder_name;
  if(!m_omx_decoder.Initialize((const CStdString)componentName, OMX_IndexParamVideoInit))
    return false;

  callbacks.EventHandler      = NULL;
  callbacks.EmptyBufferDone   = NULL;
  callbacks.FillBufferDone    = NULL;

  componentName = "OMX.broadcom.video_render";
  if(!m_omx_render.Initialize((const CStdString)componentName, OMX_IndexParamVideoInit))
    return false;

  componentName = "OMX.broadcom.video_scheduler";
  if(!m_omx_sched.Initialize((const CStdString)componentName, OMX_IndexParamVideoInit))
    return false;

  if(clock == NULL)
    return false;

  m_av_clock = clock;
  m_omx_clock = m_av_clock->GetOMXClock();

  if(m_omx_clock->GetComponent() == NULL)
  {
    m_av_clock = NULL;
    m_omx_clock = NULL;
    return false;
  }

  m_omx_tunnel_decoder.Initialize(&m_omx_decoder, m_omx_decoder.GetOutputPort(), &m_omx_sched, m_omx_sched.GetInputPort());
  m_omx_tunnel_sched.Initialize(&m_omx_sched, m_omx_sched.GetOutputPort(), &m_omx_render, m_omx_render.GetInputPort());
  m_omx_tunnel_clock.Initialize(m_omx_clock, m_omx_clock->GetInputPort() + 1, &m_omx_sched, m_omx_sched.GetOutputPort() + 1);

  omx_err = m_omx_tunnel_clock.Establish(false);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open m_omx_tunnel_clock.Establish\n");
    return false;
  }

  omx_err = m_omx_decoder.SetStateForComponent(OMX_StateIdle);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open m_omx_decoder.SetStateForComponent\n");
    return false;
  }

  OMX_VIDEO_PARAM_PORTFORMATTYPE formatType;
  OMX_INIT_STRUCTURE(formatType);
  formatType.nPortIndex = m_omx_decoder.GetInputPort();
  formatType.eCompressionFormat = m_codingType;

  /*
  if (hints.fpsscale > 0 && hints.fpsrate > 0)
  {
    float fFramerate = (float)hints.fpsrate / (float)hints.fpsscale;
    formatType.xFramerate = (int)( (float)(1<<16)*fFramerate);
  }
  */

  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamVideoPortFormat, &formatType);
  if(omx_err != OMX_ErrorNone)
    return false;
  
  OMX_PARAM_PORTDEFINITIONTYPE portParam;
  OMX_INIT_STRUCTURE(portParam);
  portParam.nPortIndex = m_omx_decoder.GetInputPort();

  omx_err = m_omx_decoder.GetParameter(OMX_IndexParamPortDefinition, &portParam);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error OMX_IndexParamPortDefinition omx_err(0x%08x)\n", omx_err);
    return false;
  }

  portParam.nPortIndex = m_omx_decoder.GetInputPort();
  portParam.nBufferCountActual = VIDEO_BUFFERS;

  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamPortDefinition, &portParam);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error OMX_IndexParamPortDefinition omx_err(0x%08x)\n", omx_err);
    return false;
  }

  // Alloc buffers for the omx input port.
  omx_err = m_omx_decoder.AllocInputBuffers();
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open AllocOMXInputBuffers\n");
    return false;
  }

  omx_err = m_omx_tunnel_decoder.Establish(false);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open m_omx_tunnel_decoder.Establish\n");
    return false;
  }

  omx_err = m_omx_decoder.SetStateForComponent(OMX_StateExecuting);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error m_omx_decoder.SetStateForComponent\n");
    return false;
  }

  omx_err = m_omx_tunnel_sched.Establish(true);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open m_omx_tunnel_sched.Establish\n");
    return false;
  }

  omx_err = m_omx_sched.SetStateForComponent(OMX_StateExecuting);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error m_omx_sched.SetStateForComponent\n");
    return false;
  }

  omx_err = m_omx_render.SetStateForComponent(OMX_StateExecuting);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error m_omx_render.SetStateForComponent\n");
    return false;
  }

  /* send decoder config */
  if(m_extrasize > 0 && m_extradata != NULL)
  {
    OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer();

    if(omx_buffer == NULL)
    {
      CLog::Log(LOGERROR, "%s::%s - buffer error 0x%08x", CLASSNAME, __func__, omx_err);
      return false;
    }

    omx_buffer->nOffset = 0;
    omx_buffer->nFilledLen = m_extrasize;
    if(omx_buffer->nFilledLen > omx_buffer->nAllocLen)
    {
      CLog::Log(LOGERROR, "%s::%s - omx_buffer->nFilledLen > omx_buffer->nAllocLen", CLASSNAME, __func__);
      return false;
    }

    memset((unsigned char *)omx_buffer->pBuffer, 0x0, omx_buffer->nAllocLen);
    memcpy((unsigned char *)omx_buffer->pBuffer, m_extradata, omx_buffer->nFilledLen);
    omx_buffer->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;
  
    omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
    if (omx_err != OMX_ErrorNone)
    {
      CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
      return false;
    }
  }

  /*
  OMX_CONFIG_DISPLAYREGIONTYPE configDisplay;
  OMX_INIT_STRUCTURE(configDisplay);
  configDisplay.nPortIndex = m_omx_render.GetInputPort();

  configDisplay.set     = OMX_DISPLAY_SET_LAYER;
  configDisplay.layer   = 1;

  omx_err = m_omx_render.SetConfig(OMX_IndexConfigDisplayRegion, &configDisplay);
  if(omx_err != OMX_ErrorNone)
    return false;
  */

  m_is_open       = true;
  m_drop_state    = false;
  m_setStartTime  = true;

  CLog::Log(LOGDEBUG,
    "%s::%s - decoder_component(0x%p), input_port(0x%x), output_port(0x%x)\n",
    CLASSNAME, __func__, m_omx_decoder.GetComponent(), m_omx_decoder.GetInputPort(), m_omx_decoder.GetOutputPort());


  return true;
}

void COMXVideo::Close()
{
  m_omx_tunnel_decoder.Flush();

  m_omx_tunnel_clock.Deestablish();
  m_omx_tunnel_sched.Deestablish();
  m_omx_tunnel_decoder.Deestablish();
  m_omx_sched.Deinitialize();

  m_omx_decoder.WaitForEvent(OMX_EventPortSettingsChanged);
  m_omx_decoder.Deinitialize();

  m_omx_render.Deinitialize();

  m_is_open       = false;

  if(m_extradata)
    free(m_extradata);
  m_extradata = NULL;
  m_extrasize = 0;

  if(m_converter)
    delete m_converter;
  m_converter = NULL;
  m_video_convert = false;
  m_video_codec_name = "";
}

void COMXVideo::SetDropState(bool bDrop)
{
  m_drop_state = bDrop;
}

unsigned int COMXVideo::GetFreeSpace()
{
  return m_omx_decoder.GetInputBufferSpace();
}

unsigned int COMXVideo::GetSize()
{
  return m_omx_decoder.GetInputBufferSize();
}

int COMXVideo::Decode(uint8_t *pData, int iSize, int64_t dts, int64_t pts)
{
  OMX_ERRORTYPE omx_err;

  if (pData || iSize > 0)
  {
    unsigned int demuxer_bytes = (unsigned int)iSize;
    uint8_t *demuxer_content = pData;

    if(m_video_convert)
    {
      m_converter->Convert(pData, iSize);
      demuxer_bytes = m_converter->GetConvertSize();
      demuxer_content = m_converter->GetConvertBuffer();
      if(!demuxer_bytes && demuxer_bytes < 1)
      {
        return false;
      }
    }

    unsigned int nSleepTime = 0;

    while(demuxer_bytes)
    {
      OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer();

      if(omx_buffer == NULL)
      {
        OMXSleep(2);
        nSleepTime += 2;
        if(nSleepTime >= 500)
        {
          CLog::Log(LOGERROR, "OMXVideo::Decode timeout\n");
          printf("COMXVideo::Decode timeout\n");
          return false;
        }
        continue;
      }
      nSleepTime = 0;

      /*
      CLog::Log(DEBUG, "COMXVideo::Video VDec : pts %lld omx_buffer 0x%08x buffer 0x%08x number %d\n", 
          pts, omx_buffer, omx_buffer->pBuffer, (int)omx_buffer->pAppPrivate);
      printf("VDec : pts %lld omx_buffer 0x%08x buffer 0x%08x number %d\n", 
          pts, omx_buffer, omx_buffer->pBuffer, (int)omx_buffer->pAppPrivate);
      */

      if(m_setStartTime)
      {
        omx_buffer->nFlags = OMX_BUFFERFLAG_STARTTIME;

        m_setStartTime = false;
      }
      else
      {
        if((uint64_t)pts == AV_NOPTS_VALUE)
        {
          omx_buffer->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
        }
        else
        {
          omx_buffer->nFlags = 0;
        }
      }

      uint64_t val = ((uint64_t)pts == AV_NOPTS_VALUE) ? 0 : pts;
#ifdef OMX_SKIP64BIT
      if((uint64_t)pts == AV_NOPTS_VALUE)
      {
        omx_buffer->nTimeStamp.nLowPart = 0;
        omx_buffer->nTimeStamp.nHighPart = 0;
      }
      else
      {
        omx_buffer->nTimeStamp.nLowPart = val & 0x00000000FFFFFFFF;
        omx_buffer->nTimeStamp.nHighPart = (val & 0xFFFFFFFF00000000) >> 32;
      }

#else
      omx_buffer->nTimeStamp = (pts == AV_NOPTS_VALUE) ? 0 : pts; // in microseconds
#endif

      omx_buffer->nFilledLen = (demuxer_bytes > omx_buffer->nAllocLen) ? omx_buffer->nAllocLen : demuxer_bytes;
      memcpy(omx_buffer->pBuffer, demuxer_content, omx_buffer->nFilledLen);

      demuxer_bytes -= omx_buffer->nFilledLen;
      demuxer_content += omx_buffer->nFilledLen;

      if(demuxer_bytes == 0)
        omx_buffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

      omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);

      if(omx_err != OMX_ErrorNone)
      {
        CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);

        printf("%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);

        return false;
      }
    }

    return true;

  }
  
  return false;
}

void COMXVideo::Reset(void)
{
  m_omx_decoder.FlushInput();
 
  m_omx_tunnel_clock.Flush();
  m_omx_tunnel_sched.Flush();
  m_omx_tunnel_decoder.Flush();

  m_setStartTime = true;
}

///////////////////////////////////////////////////////////////////////////////////////////
bool COMXVideo::Pause()
{
  if(m_omx_render.GetComponent() == NULL)
    return false;

  if(m_Pause) return true;
  m_Pause = true;

  m_omx_sched.SetStateForComponent(OMX_StatePause);
  m_omx_render.SetStateForComponent(OMX_StatePause);

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
bool COMXVideo::Resume()
{
  if(m_omx_render.GetComponent() == NULL)
    return false;

  if(!m_Pause) return true;
  m_Pause = false;

  m_omx_sched.SetStateForComponent(OMX_StateExecuting);
  m_omx_render.SetStateForComponent(OMX_StateExecuting);

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
void COMXVideo::SetVideoRect(const CRect& SrcRect, const CRect& DestRect)
{
  if(m_is_open)
    return;

  OMX_CONFIG_DISPLAYREGIONTYPE configDisplay;
  OMX_INIT_STRUCTURE(configDisplay);
  configDisplay.nPortIndex = m_omx_render.GetInputPort();

  configDisplay.set     = OMX_DISPLAY_SET_FULLSCREEN;
  configDisplay.fullscreen = OMX_FALSE;

  m_omx_render.SetConfig(OMX_IndexConfigDisplayRegion, &configDisplay);

  configDisplay.set     = OMX_DISPLAY_SET_DEST_RECT;
  configDisplay.dest_rect.x_offset  = DestRect.x1;
  configDisplay.dest_rect.y_offset  = DestRect.y1;
  configDisplay.dest_rect.width     = DestRect.Width();
  configDisplay.dest_rect.height    = DestRect.Height();

  m_omx_render.SetConfig(OMX_IndexConfigDisplayRegion, &configDisplay);

  printf("dest_rect.x_offset %d dest_rect.y_offset %d dest_rect.width %d dest_rect.height %d\n",
      configDisplay.dest_rect.x_offset, configDisplay.dest_rect.y_offset, configDisplay.dest_rect.width, configDisplay.dest_rect.height);
}
