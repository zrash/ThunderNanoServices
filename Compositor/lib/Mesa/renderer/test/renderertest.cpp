/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Metrological B.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MODULE_NAME
#define MODULE_NAME CompositorRenderTest
#endif

#include <core/core.h>
#include <localtracer/localtracer.h>
#include <messaging/messaging.h>

#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <IBuffer.h>
#include <IRenderer.h>
#include <Transformation.h>

#include <drm_fourcc.h>

MODULE_NAME_DECLARATION(BUILD_REFERENCE)

using namespace WPEFramework;

#define fourcc_mod_get_vendor(modifier) \
    (((modifier) >> 56) & 0xff)

#define fourcc_mod_get_code(modifier) \
    ((modifier)&0x00ffffffffffffffULL)

namespace {
const std::vector<std::string> Parse(const std::string& input)
{
    std::istringstream iss(input);
    return std::vector<std::string>{ std::istream_iterator<std::string>{ iss }, std::istream_iterator<std::string>{} };
}

/* simple stringification operator to make errorcodes human readable */
#define CASE_TO_STRING(value) \
    case value:         \
        return #value;

static const char* DrmVendorString(uint64_t modifier)
{
    switch (fourcc_mod_get_vendor(modifier)) {
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_NONE)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_INTEL)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_AMD)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_NVIDIA)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_SAMSUNG)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_QCOM)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_VIVANTE)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_BROADCOM)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_ARM)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_ALLWINNER)
        CASE_TO_STRING(DRM_FORMAT_MOD_VENDOR_AMLOGIC)
    default:
        return "Unknown";
    }
}
#undef CASE_TO_STRING

void PrintFormat(const string& preamble, const Compositor::PixelFormat& format)
{
    std::stringstream line;

    line << preamble << " fourcc \'"
         << char(format.Type() & 0xff) << char((format.Type() >> 8) & 0xff)
         << char((format.Type() >> 16) & 0xff) << char((format.Type() >> 24) & 0xff)
         << "\'"
         << " modifiers: [ " << std::endl;

    for (auto& modifier : format.Modifiers()) {
        line << "  " << DrmVendorString(modifier) << ": code 0x" << std::hex << std::setw(7) << std::setfill('0') << fourcc_mod_get_code(modifier) << std::endl;
    }

    line << "]";

    TRACE_GLOBAL(Trace::Information, ("%s", line.str().c_str()));
}

const Compositor::Color gray = { 0.5, 0.5, 0.5, 1.0 };
const Compositor::Color red = { 1.0, 0.0, 0.0, 1.0 };
const Compositor::Color green = { 0.0, 1.0, 0.0, 1.0 };
const Compositor::Color blue = { 0.0, 0.0, 1.0, 1.0 };
const Compositor::Color white = { 1.0, 1.0, 1.0, 1.0 };
}

int main(int argc, const char* argv[])
{
    const std::array<Compositor::Color, 3> Colors = { red, green, blue };

    std::string ConnectorId;

    if (argc == 1) {
        ConnectorId = "card0-HDMI-A-2";
    } else {
        ConnectorId = argv[1];
    }

    Messaging::MessageUnit::flush flushMode;
    flushMode = Messaging::MessageUnit::flush::FLUSH;

    Messaging::LocalTracer& tracer = Messaging::LocalTracer::Open();

    const char* executableName(Core::FileNameOnly(argv[0]));

    {
        Messaging::ConsolePrinter printer(true);

        tracer.Callback(&printer);

        const std::vector<string> modules = {
            "CompositorRenderTest",
            "CompositorBuffer",
            "CompositorBackend",
            "CompositorRenderer",
        };

        for (auto module : modules) {
            tracer.EnableMessage(module, "", true);
        }

        TRACE_GLOBAL(Trace::Information, ("%s - build: %s", executableName, __TIMESTAMP__));

        uint64_t mods[1] = { DRM_FORMAT_MOD_LINEAR };
        Compositor::PixelFormat format(DRM_FORMAT_ARGB8888, (sizeof(mods) / sizeof(mods[0])), mods);

        Core::ProxyType<Exchange::ICompositionBuffer> framebuffer = Compositor::Connector(ConnectorId, Exchange::IComposition::ScreenResolution::ScreenResolution_1080p, format, false);

        Core::ProxyType<Compositor::IRenderer> renderer = Compositor::IRenderer::Instance(framebuffer->Identifier());

        // Add a buffer to render on
        TRACE_GLOBAL(Trace::Information, ("Render Bind"));
        renderer->Bind(framebuffer);

        constexpr uint8_t nRenderCycles(10);

        for (uint8_t i(0); i <= nRenderCycles; i++) {
            TRACE_GLOBAL(Trace::Information, ("Render Begin"));
            renderer->Begin(1920, 1080);

            TRACE_GLOBAL(Trace::Information, ("Render Clear"));
            renderer->Clear(white);

            // TODO make one object of this...
            Compositor::Matrix quad1;
            Compositor::Transformation::Projection(quad1, 60, 120, Compositor::Transformation::TRANSFORM_NORMAL);

            constexpr uint8_t nRenderQuads(10);

            for (uint8_t i(0); i <= nRenderQuads; i++){     
                TRACE_GLOBAL(Trace::Information, ("Render Quadrangle %d", i));
                renderer->Quadrangle(Colors.at(i % (Colors.size() - 1)), quad1);
                Compositor::Transformation::Translate(quad1, 30, 60);
            }

            TRACE_GLOBAL(Trace::Information, ("Render End"));
            renderer->End();

            framebuffer->Render();

            sleep(1);
        }

        renderer->Unbind();

        framebuffer.Release();
        renderer.Release();
    }

    TRACE_GLOBAL(Trace::Information, ("Exiting %s.... ", executableName));

    tracer.Close();
    Core::Singleton::Dispose();

    return 0;
}