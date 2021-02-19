/*
 * Copyright (C) 2020 Microsoft Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorScreencastAgent.h"

#include "GenericCallback.h"
#include "PageClient.h"
#include "ScreencastEncoder.h"
#include "WebPageInspectorController.h"
#include "WebPageProxy.h"
#include "WebsiteDataStore.h"
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <WebCore/NotImplemented.h>
#include <wtf/RunLoop.h>
#include <wtf/UUID.h>

#if USE(CAIRO)
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "DrawingAreaProxy.h"
#endif

namespace WebKit {

using namespace Inspector;

InspectorScreencastAgent::InspectorScreencastAgent(BackendDispatcher& backendDispatcher, Inspector::FrontendRouter& frontendRouter, WebPageProxy& page)
    : InspectorAgentBase("Screencast"_s)
    , m_backendDispatcher(ScreencastBackendDispatcher::create(backendDispatcher, this))
    , m_page(page)
{
}

InspectorScreencastAgent::~InspectorScreencastAgent()
{
}

void InspectorScreencastAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorScreencastAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    if (!m_encoder)
        return;

    // The agent may be destroyed when the callback is invoked.
    m_encoder->finish([sessionID = m_page.websiteDataStore().sessionID(), screencastID = WTFMove(m_currentScreencastID)] {
        if (WebPageInspectorController::observer())
            WebPageInspectorController::observer()->didFinishScreencast(sessionID, screencastID);
    });

    m_encoder = nullptr;
}

#if USE(CAIRO)
void InspectorScreencastAgent::didPaint(cairo_surface_t* surface)
{
    if (m_encoder)
        m_encoder->encodeFrame(surface, m_page.drawingArea()->size());
}
#endif

Inspector::Protocol::ErrorStringOr<String /* screencastID */> InspectorScreencastAgent::start(const String& file, int width, int height, Optional<double>&& scale)
{
    if (m_encoder)
        return makeUnexpected("Already recording"_s);

    if (width < 10 || width > 10000 || height < 10 || height > 10000)
        return makeUnexpected("Invalid size"_s);

    if (scale && (*scale <= 0 || *scale > 1))
        return makeUnexpected("Unsupported scale"_s);

    String errorString;
    m_encoder = ScreencastEncoder::create(errorString, file, WebCore::IntSize(width, height), WTFMove(scale));
    if (!m_encoder)
        return makeUnexpected(errorString);

    m_currentScreencastID = createCanonicalUUIDString();

#if PLATFORM(MAC)
    m_encoder->setOffsetTop(m_page.pageClient().browserToolbarHeight());
#endif
#if !PLATFORM(WPE)
    scheduleFrameEncoding();
#endif
    // Force at least one frame on WPE.
    m_page.forceRepaint([] { });

    return { { m_currentScreencastID } };
}

void InspectorScreencastAgent::stop(Ref<StopCallback>&& callback)
{
    if (!m_encoder) {
        callback->sendFailure("Not recording"_s);
        return;
    }

    // The agent may be destroyed when the callback is invoked.
    m_encoder->finish([sessionID = m_page.websiteDataStore().sessionID(), screencastID = WTFMove(m_currentScreencastID), callback = WTFMove(callback)] {
        if (WebPageInspectorController::observer())
            WebPageInspectorController::observer()->didFinishScreencast(sessionID, screencastID);
        callback->sendSuccess();
    });
    m_encoder = nullptr;
}

#if !PLATFORM(WPE)
void InspectorScreencastAgent::scheduleFrameEncoding()
{
    if (!m_encoder)
        return;

    RunLoop::main().dispatchAfter(Seconds(1.0 / ScreencastEncoder::fps), [agent = makeWeakPtr(this)]() mutable {
        if (!agent)
            return;

        agent->encodeFrame();
        agent->scheduleFrameEncoding();
    });
}
#endif

#if PLATFORM(MAC)
void InspectorScreencastAgent::encodeFrame()
{
    if (m_encoder)
        m_encoder->encodeFrame(m_page.pageClient().takeSnapshotForAutomation());
}
#endif

#if USE(CAIRO) && !PLATFORM(WPE)
void InspectorScreencastAgent::encodeFrame()
{
    if (!m_encoder)
        return;

    if (auto* drawingArea = m_page.drawingArea())
        static_cast<DrawingAreaProxyCoordinatedGraphics*>(drawingArea)->captureFrame();
}
#endif

} // namespace WebKit
