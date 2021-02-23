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

#pragma once

#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace Inspector {
class BackendDispatcher;
class FrontendChannel;
class FrontendRouter;
class ScreencastFrontendDispatcher;
}

namespace WebKit {

class ScreencastEncoder;
class WebPageProxy;

class InspectorScreencastAgent : public Inspector::InspectorAgentBase, public Inspector::ScreencastBackendDispatcherHandler, public CanMakeWeakPtr<InspectorScreencastAgent> {
    WTF_MAKE_NONCOPYABLE(InspectorScreencastAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorScreencastAgent(Inspector::BackendDispatcher& backendDispatcher, Inspector::FrontendRouter& frontendRouter, WebPageProxy& page);
    ~InspectorScreencastAgent() override;

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

#if USE(CAIRO)
    void didPaint(cairo_surface_t*);
#endif

    Inspector::Protocol::ErrorStringOr<String /* screencastID */> start(const String& file, int width, int height, Optional<double>&& scale) override;
    void stop(Ref<StopCallback>&&) override;


private:
#if !PLATFORM(WPE)
    void scheduleFrameEncoding();
    void encodeFrame();
#endif

    Ref<Inspector::ScreencastBackendDispatcher> m_backendDispatcher;
    WebPageProxy& m_page;
    RefPtr<ScreencastEncoder> m_encoder;
    String m_currentScreencastID;
};

} // namespace WebKit
