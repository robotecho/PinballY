// This file is part of PinballY
// Copyright 2018 Michael J Roberts | GPL v3 or later | NO WARRANTY
//
#include "stdafx.h"
#include <dshow.h>
#include "DShowAudioPlayer.h"
#include "PrivateWindowMessages.h"
#include "../Utilities/LogError.h"

// Include the library where the DirectShow IIDs are defined
#pragma comment(lib, "strmiids.lib") 


DShowAudioPlayer::DShowAudioPlayer(HWND hwndEvent) : 
	AudioVideoPlayer(NULL, hwndEvent, true)
{
}

DShowAudioPlayer::~DShowAudioPlayer()
{
}

void DShowAudioPlayer::SetLooping(bool looping)
{
	this->looping = looping;
}

bool DShowAudioPlayer::Error(HRESULT hr, ErrorHandler &eh, const TCHAR *where)
{
	WindowsErrorMessage winErr(hr);
	eh.SysError(LoadStringT(IDS_ERR_AUDIOPLAYERSYSERR),
			MsgFmt(_T("Opening audio file %s: %s: %s"), path.c_str(), where, winErr.Get()));
		return false;
}

bool DShowAudioPlayer::Open(const WCHAR *path, ErrorHandler &eh)
{
	// remember the file path
	this->path = WideToTSTRING(path);

	// create the graph manager
	RefPtr<IGraphBuilder> pGraph;
	HRESULT hr;
	if (!SUCCEEDED(hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&pGraph)))
		return Error(hr, eh, _T("Creating filter graph"));

	// query interfaces
	if (!SUCCEEDED(hr = pGraph->QueryInterface(&pControl)))
		return Error(hr, eh, _T("Querying media control interface"));
	if (!SUCCEEDED(hr = pGraph->QueryInterface(&pEventEx)))
		return Error(hr, eh, _T("Querying media event interface"));
	if (!SUCCEEDED(hr = pGraph->QueryInterface(&pBasicAudio)))
		return Error(hr, eh, _T("Querying basic audio interface"));
	if (!SUCCEEDED(hr = pGraph->QueryInterface(&pSeek)))
		return Error(hr, eh, _T("Querying seek interface"));

	// set up the event callback
	pEventEx->SetNotifyWindow(reinterpret_cast<OAHWND>(hwndEvent), DSMsgOnEvent, reinterpret_cast<LONG_PTR>(this));

	// render the file
	if (!SUCCEEDED(hr = pGraph->RenderFile(path, NULL)))
		return Error(hr, eh, _T("Rendering file"));

	// success
	return true;
}

void DShowAudioPlayer::OnEvent()
{
	// process events until the queue is empty
	long eventCode;
	LONG_PTR lParam1, lParam2;
	while (SUCCEEDED(pEventEx->GetEvent(&eventCode, &lParam1, &lParam2, 0)))
	{
		// check what we have
		switch (eventCode)
		{
		case EC_COMPLETE:
			// notify the event window that playback is finished
			PostMessage(hwndEvent, looping ? AVPMsgLoopNeeded : AVPMsgEndOfPresentation, static_cast<WPARAM>(cookie), 0);
			break;
		}

		// clean up the event parameters
		pEventEx->FreeEventParams(eventCode, lParam1, lParam2);
	}
}

bool DShowAudioPlayer::Play(ErrorHandler &eh)
{
	// start playback
	if (HRESULT hr; !SUCCEEDED(hr = pControl->Run()))
		return Error(hr, eh, _T("IMediaControl::Run"));

	// flag that playback is running
	playing = true;

	// success
	return true;
}

bool DShowAudioPlayer::Stop(ErrorHandler &eh)
{
	// stop playback
	if (HRESULT hr; !SUCCEEDED(hr = pControl->Stop()))
		return Error(hr, eh, _T("IMediaControl::Stop"));

	// flag that playback is no longer running
	playing = false;

	// success
	return true;
}

bool DShowAudioPlayer::Replay(ErrorHandler &eh)
{
	// stop playback
	if (!Stop(eh))
		return false;

	// seek to the start
	LONGLONG cur = 0;
	if (HRESULT hr; !SUCCEEDED(hr = pSeek->SetPositions(&cur, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning)))
		return Error(hr, eh, _T("IMediaSeek::SetPositions"));

	// resume/restart playback
	return Play(eh);
}

void DShowAudioPlayer::Mute(bool mute)
{
	// mute
	pBasicAudio->put_Volume(mute ? -10000 : vol);
	muted = mute;
}

void DShowAudioPlayer::SetVolume(int pct)
{
	// override muting
	if (muted)
		muted = false;

	// figure the new volume, converting from our 0%-100% scale to 
	// DShow's -100db to 0db scale
	vol = (pct - 100) * 100;

	// set the new volume in the underlying interface
	pBasicAudio->put_Volume(vol);
}

void DShowAudioPlayer::Shutdown()
{
	Stop(SilentErrorHandler());
}

