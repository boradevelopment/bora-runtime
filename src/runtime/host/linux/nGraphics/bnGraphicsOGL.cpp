#ifdef BORA_HAS_OPENGL
#include "bnGraphicsOGL.h"
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "nWindow/linuxAbstracts.h"
#include <X11/Xlib.h>
#include <wayland-egl.h>
#if defined(Always)
#undef Always
#endif
#if defined(None)
#undef None
#endif

// todo?
void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
                                const GLchar* message, const void* userParam)
{
    auto logger = LogManager::instance().getLogger("bora.window.graphics.opengl");

    LOG_ERROR(logger) << "GL CALLBACK: " << (type == GL_DEBUG_TYPE_ERROR ? "**ERROR**" : "")
        << " type = 0x" << type
        << ", severity = 0x" << severity
        << ", message = " << message;
}

bnGraphicsOGL::bnGraphicsOGL(SysHandle& handle, IGraphicsDeviceConfig& config) : bnGraphicsOGLCommon(handle, config) {

}

bool bnGraphicsOGL::Init()
{
    device = std::make_unique<DeviceOGL>();
    context = std::make_unique<DeviceContextOGL>();

    EGLDisplay display = nullptr;
    if (windowHandle->type == DisplayServerType::Wayland)
    {
       display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, (void*)windowHandle->wayland.display, nullptr);
    } else if (windowHandle->type == DisplayServerType::X11)
    {
        display = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, (void*)windowHandle->x11.display, nullptr);
    }
    if (!display) return false;

    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        return false;
    }

    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, // Or EGL_OPENGL_ES2_BIT
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfigs;
    EGLConfig eglConfig;
    bool result = eglChooseConfig(display, attribs, &eglConfig, 1, &numConfigs) && numConfigs > 0;
    if (!result) return false;

    EGLSurface surface = nullptr;
    if (windowHandle->type == DisplayServerType::X11)
    {
        surface = eglCreateWindowSurface(display, eglConfig, (EGLNativeWindowType)windowHandle->x11.window, nullptr);
    } else if (windowHandle->type == DisplayServerType::Wayland)
    {
        wl_egl_window* egl_window = wl_egl_window_create(windowHandle->wayland.surface, width, height);
        surface = eglCreateWindowSurface(display, eglConfig, (EGLNativeWindowType)egl_window, nullptr);
    }

    if (!surface) return false;
    device->SetHandle(surface, display);

    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
        EGL_NONE
    };
    eglBindAPI(EGL_OPENGL_API);
    EGLContext renderContext = eglCreateContext(display, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (renderContext == EGL_NO_CONTEXT) {
        EGLint error = eglGetError();
        //todo: Handle failure
    }
    context->renderContext = renderContext;
    context->display = display;
    if (!eglMakeCurrent(display, surface, surface, renderContext)) {
        return false;
    }

    if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress)) {
        return false;
    }
    if (GLAD_GL_VERSION_4_3) {
        auto logger = LogManager::instance().getLogger("bora.window.graphics.opengl");

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(MessageCallback,  nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        LOG_INFO(logger) << "[OpenGL] Debug output callback successfully registered.\n";
    }
    return true;
}

void bnGraphicsOGL::Shutdown()
{
    bnGraphicsOGL::Shutdown();
    if (context->renderContext) {
        context->Release();
    }
    if (device->Get()) {
        device->Release();
    }
}

void bnGraphicsOGL::Present()
{
    glFinish();
    eglSwapInterval(device->display, config.vsync ? 1 : 0);

    bnGraphicsOGLCommon::Present();

    eglSwapBuffers(device->display, device->surface);
}
#endif