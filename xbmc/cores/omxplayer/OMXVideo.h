#pragma once
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

#if defined(HAVE_LIBOPENMAX)

#include "OMXCore.h"
#include "OMXStreamInfo.h"

#if (HAVE_LIBOPENMAX == 2)
#include <IL/OMX_Video.h>
#else
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Index.h>
#include <OMX_Image.h>
#endif

#include "BitstreamConverter.h"

#include "OMXClock.h"

#define VIDEO_BUFFERS 20

#define CLASSNAME "COMXVideo"

class DllAvUtil;
class DllAvFormat;
class COMXVideo
{
public:
  COMXVideo();
  virtual ~COMXVideo();

  // Required overrides
  bool Open(COMXStreamInfo &hints, OMXClock *clock);
  void Close(void);
  unsigned int GetFreeSpace();
  unsigned int GetSize();
  int  Decode(uint8_t *pData, int iSize, int64_t dts, int64_t pts);
  void Reset(void);
  //bool GetPicture(DVDVideoPicture *pDvdVideoPicture);
  void SetDropState(bool bDrop);
  bool Pause();
  bool Resume();
  CStdString GetDecoderName() { return m_video_codec_name; };
protected:
  // Video format
  bool              m_drop_state;
  int               m_decoded_width;
  int               m_decoded_height;

  OMX_VIDEO_CODINGTYPE m_codingType;

  COMXCoreComponent m_omx_decoder;
  COMXCoreComponent m_omx_render;
  COMXCoreComponent m_omx_sched;
  COMXCoreComponent *m_omx_clock;
  OMXClock           *m_av_clock;

  COMXCoreTunel     m_omx_tunnel_decoder;
  COMXCoreTunel     m_omx_tunnel_clock;
  COMXCoreTunel     m_omx_tunnel_sched;
  bool              m_is_open;

  bool              m_Pause;
  bool              m_setStartTime;

  uint8_t           *m_extradata;
  int               m_extrasize;

  CBitstreamConverter   *m_converter;
  bool              m_video_convert;
  CStdString        m_video_codec_name;
};

#endif
