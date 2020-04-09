#include "config.h"

#ifdef VDB_DEBUG
#include "glad/glad_3_1_debug.c"
#else
#include "glad/glad_3_1_release.c"
#endif

#ifdef _WIN32
#undef WIN32_LEAN_AND_MEAN // defined by glad
#endif

#include <SDL.h>

// Dear ImGui
#ifdef VDB_DEBUG
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM <opengl_debug.h>
#else
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM <opengl.h>
#endif
#include "imgui/imgui.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_widgets.cpp"
#include "imgui/imgui_impl_sdl.cpp"
#include "imgui/imgui_impl_opengl3.cpp"

#if VDB_IMGUI_FREETYPE==1
#include "freetype.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "vdb.h"
#include "matrix.h"
#include "keys.h"
#include "settings.h"
#include "colormap.h"
#include "style.h"
#include "mouse.h"
#include "window.h"
#include "matrix_stack.h"
#include "camera.h"
#include "shader.h"
#include "image.h"
#include "framebuffer.h"
#include "render_target.h"
#include "framegrab.h"
#include "transform.h"
#include "immediate.h"
#include "immediate_util.h"
#include "render_scaler.h"
#include "log.h"
#include "ui.h"
#include "ruler.h"
#include "widgets.h"
#include "hints.h"
#include "data/noto_sans_regular.h"

const char *GLErrorCodeString(GLenum error)
{
         if (error == GL_INVALID_ENUM)                  return "GL_INVALID_ENUM";
    else if (error == GL_INVALID_VALUE)                 return "GL_INVALID_VALUE";
    else if (error == GL_INVALID_OPERATION)             return "GL_INVALID_OPERATION";
    else if (error == GL_INVALID_FRAMEBUFFER_OPERATION) return "GL_INVALID_FRAMEBUFFER_OPERATION";
    else if (error == GL_OUT_OF_MEMORY)                 return "GL_OUT_OF_MEMORY";
    return "Not an error";
}

#define CheckGLError() { \
    GLenum error = glGetError(); \
    if (error != GL_NO_ERROR) { \
        fprintf(stderr, "OpenGL error in file %s line %d: (0x%x) %s\n", __FILE__, __LINE__, error, GLErrorCodeString(error)); \
        exit(EXIT_FAILURE); \
    } \
}

namespace vdb
{
    static bool initialized;
    static bool is_first_frame;
    static bool is_different_label;
    static bool want_step_once;
    static bool want_step_over;
    static int loaded_font_size;
    static frame_settings_t *frame_settings;
}

bool vdbIsFirstFrame()
{
    return vdb::is_first_frame;
}

bool vdbIsDifferentLabel()
{
    return vdb::is_different_label;
}

static void InitializeIfNotAlready()
{
    if (!vdb::initialized)
    {
        vdb::initialized = true;

        settings.LoadOrDefault(VDB_SETTINGS_FILENAME);
        window_settings_t ws = settings.window;
        window::CreateContext(ws.x, ws.y, ws.width, ws.height);
        CheckGLError();

        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForOpenGL(window::sdl_window, window::sdl_gl_context);
        ImGui_ImplOpenGL3_Init("#version 150");
        ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowBorderSize = 0.0f;
    }
}

void vdbDetachContext()
{
    window::DetachContext();
}

void vdbMakeContextCurrent()
{
    InitializeIfNotAlready();
    window::EnsureContextIsCurrent();
}

void vdbStepOnce()
{
    vdb::want_step_once = true;
}

void vdbStepOver()
{
    vdb::want_step_over = true;
}

void vdbAutoStep(bool enabled)
{
    ui::auto_step = enabled;
}

// void    vdbScreenshot(float crop_x, float crop_y, float crop_w, float crop_h,
//                       const char *filename);
// void vdbScreenshot()
// {
//     GLenum format = opt.alpha_channel ? GL_RGBA : GL_RGB;
//     int channels = opt.alpha_channel ? 4 : 3;
//     int width = window::framebuffer_width;
//     int height = window::framebuffer_height;
//     unsigned char *data = (unsigned char*)malloc(width*height*channels);
//     glPixelStorei(GL_PACK_ALIGNMENT, 1);
//     glReadBuffer(GL_BACK);
//     glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

//     if (!opt.draw_imgui)
//     {
//         ImGui::Render();
//         ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//     }

//     framegrab::SaveFrame(data, width, height, channels, format);
// }

static frame_settings_t *GetFrameSettings()
{
    return vdb::frame_settings;
}

bool vdbBeginBreak(const char *label)
{
    static const char *skip_label = NULL;
    static const char *prev_label = NULL;
    static bool is_first_frame = true;
    vdb::is_first_frame = is_first_frame;
    vdb::is_different_label = label != prev_label;
    prev_label = label; // todo: strdup
    if (skip_label == label)
        return false;
    is_first_frame = false; // todo: first frame detection is janky.
                            // consider e.g. a for loop with single-stepping

    if (!vdb::initialized)
        InitializeIfNotAlready();

    assert(vdb::initialized);

    if (!window::visible)
        window::ShowWindow();

    int new_font_size = (int)(settings.font_size*settings.dpi_scale/100.0f);
    if (vdb::loaded_font_size != new_font_size)
    {
        //
        // Load from compressed binary included as header file
        //
        const char *data = (const char*)noto_sans_regular_compressed_data;
        const unsigned int sizeof_data = noto_sans_regular_compressed_size;

        vdb::loaded_font_size = new_font_size;
        ImGui_ImplOpenGL3_DestroyDeviceObjects();
        ImGui::GetIO().Fonts->Clear();
        ui::regular_font = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(data, sizeof_data, (float)new_font_size);

        int big_font_size = 2*new_font_size;
        ImFontConfig config;
        config.MergeMode = false;
        ImFontGlyphRangesBuilder builder;
        builder.AddText("0123456789,.-+");
        static ImVector<ImWchar> glyph_ranges; // this must persist until call to GetTexData
        builder.BuildRanges(&glyph_ranges);
        ui::big_font = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(data, sizeof_data, (float)big_font_size, &config, glyph_ranges.Data);
        #endif

        // This should be called before ImGui::GetTexDataAsRGBA32 (e.g. inside ImGui_ImplSdlGL3_CreateFontsTexture)
        #if VDB_IMGUI_FREETYPE==1
        #if VDB_IMGUI_FREETYPE_DYNAMIC==1
        if (TryLoadFreetype())
            ImGuiFreeType::BuildFontAtlas(ImGui::GetIO().Fonts, 0);
        #else
        ImGuiFreeType::BuildFontAtlas(ImGui::GetIO().Fonts, 0);
        #endif
    }

    if (vdbIsFirstFrame())
    {
        // see if we loaded a frame settings matching this frame's label
        frame_settings_t *fs = NULL;
        for (int i = 0; i < settings.num_frames; i++)
        {
            if (strcmp(settings.frames[i].name, label) == 0)
            {
                fs = settings.frames + i;
                break;
            }
        }

        // otherwise add a new one (but only if there's space)
        if (!fs)
        {
            if (settings.num_frames < MAX_FRAME_SETTINGS)
                fs = settings.frames + (settings.num_frames++);
            else
                fs = new frame_settings_t; // will not be saved
            fs->name = strdup(label);
            DefaultFrameSettings(fs);
        }

        assert(fs);

        vdb::frame_settings = fs;
    }

    window::EnsureContextIsCurrent();

    if (settings.can_idle && !ui::auto_step)
        window::WaitEvents();
    else
        window::PollEvents();

    window::SetNumSettleFrames(3); // ImGui requires 2-3 frames for e.g. button clicks to settle
    // Subsequent functions in VDB may also require a minimum number of frames to settle,
    // e.g. render scaling with 4x upsampling requires 16 frames.

    if (VDB_SAVE_SETTINGS_REGULARLY)
    {
        static int save_settings_counter = VDB_SAVE_SETTINGS_PERIOD;
        save_settings_counter--;
        if (save_settings_counter <= 0)
        {
            save_settings_counter += VDB_SAVE_SETTINGS_PERIOD;
            settings.Save(VDB_SETTINGS_FILENAME);
        }
    }

    bool should_step_once = false;
    should_step_once |= keys::pressed[VDB_KEY_F10];
    should_step_once |= vdb::want_step_once;
    if (ui::auto_step)
    {
        static int t = 0;
        t++;
        if (t > settings.auto_step_delay_ms*60/1000)
        {
            should_step_once |= true;
            t = 0;
        }
    }

    if (should_step_once)
    {
        vdb::want_step_once = false;
        settings.Save(VDB_SETTINGS_FILENAME);
        is_first_frame = true;
        return false;
    }
    if (keys::pressed[VDB_KEY_F5] || vdb::want_step_over)
    {
        vdb::want_step_over = false;
        settings.Save(VDB_SETTINGS_FILENAME);
        is_first_frame = true;
        skip_label = label;
        return false;
    }
    if (window::should_quit)
    {
        settings.Save(VDB_SETTINGS_FILENAME);
        window::Close();
        exit(0);
    }

    hints::BeginFrame();
    transform::BeginFrame();
    mouse::BeginFrame();
    immediate_util::BeginFrame();
    immediate::BeginFrame();
    colormap::BeginFrame();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window::sdl_window);
    ImGui::NewFrame();

    widgets::BeginFrame();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Assuming user uploads images that are one-byte packed

    // Note: ui is checked at BeginFrame instead of EndFrame because we want it to have
    // priority over ImGui panels created by the user.
    ui::escape_eaten = false;
    ruler::BeginFrame();

    vdb_style_t style = GetStyle();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearDepth(1.0f);
    glClearColor(style.clear.x, style.clear.y, style.clear.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    if (vdb::frame_settings->render_scaler.down > 0)
    {
        int n_down = vdb::frame_settings->render_scaler.down;
        int n_up = vdb::frame_settings->render_scaler.up;
        int w = window::framebuffer_width >> n_down;
        int h = window::framebuffer_height >> n_down;
        render_scaler::Begin(w, h, n_up);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0f);
        glClearColor(style.clear.x, style.clear.y, style.clear.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
    }

    immediate::SetRenderOffsetNDC(vdbGetRenderOffset());

    if (vdb::frame_settings->camera.type != VDB_CUSTOM)
    {
        frame_settings_t *fs = vdb::frame_settings;
        if      (fs->camera.type == VDB_TRACKBALL) vdbCameraTrackball();
        else if (fs->camera.type == VDB_TURNTABLE) vdbCameraTurntable();
        else                                              vdbCamera2D();

        if (fs->camera.type != VDB_PLANAR)
        {
            vdbDepthTest(true);
            vdbDepthWrite(true);
            vdbPerspective(fs->camera.projection.y_fov,
                fs->camera.projection.min_depth,
                fs->camera.projection.max_depth);
        }

        // We do PushMatrix to save current state for drawing grid in vdbEndBreak

        // pre-permutation transform
        // the built-in cameras assume that y axis is up
        vdbPushMatrix();
        vdbOrientation up = *GetCameraUp();
        if      (up == VDB_Z_UP)   vdbMultMatrix(vdbInitMat4(0,1,0,0, 0,0,1,0, 1,0,0,0, 0,0,0,1).data);
        else if (up == VDB_X_UP)   vdbMultMatrix(vdbInitMat4(0,0,1,0, 1,0,0,0, 0,1,0,0, 0,0,0,1).data);
        else if (up == VDB_Z_DOWN) vdbMultMatrix(vdbInitMat4(0,1,0,0, 0,0,-1,0, -1,0,0,0, 0,0,0,1).data);
        else if (up == VDB_Y_DOWN) vdbMultMatrix(vdbInitMat4(1,0,0,0, 0,-1,0,0, 0,0,-1,0, 0,0,0,1).data);
        else if (up == VDB_X_DOWN) vdbMultMatrix(vdbInitMat4(0,0,1,0, -1,0,0,0, 0,-1,0,0, 0,0,0,1).data);

        // pre-scaling transform
        vdbPushMatrix();
        vdbMultMatrix(vdbMatScale(1.0f/fs->grid.grid_scale, 1.0f/fs->grid.grid_scale, 1.0f/fs->grid.grid_scale).data);
    }

    CheckGLError();

    return true;
}

void vdbEndBreak()
{
    frame_settings_t *fs = vdb::frame_settings;

    if (render_scaler::has_begun)
        render_scaler::End();

    ruler::EndFrame();

    immediate::DefaultState();

    {
        vdb_style_t style = GetStyle();

        if (fs->camera.type == VDB_TRACKBALL ||
            fs->camera.type == VDB_TURNTABLE)
        {
            vdbDepthTest(true);
            vdbPopMatrix(); // pre-scaling

            // draw colored XYZ axes
            if (fs->grid.grid_visible)
            {
                float t = 10.0f;
                vdbLineWidth(1.0f);
                vdbBeginLines();
                vdbOrientation up = *GetCameraUp();
                if (up != VDB_X_UP && up != VDB_X_DOWN)
                {
                    vdbColor(style.x_axis, 0.0f);            vdbVertex(-t, 0.0f, 0.0f);
                    vdbColor(style.x_axis, style.neg_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.x_axis, style.pos_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.x_axis, 0.0f);            vdbVertex(+t, 0.0f, 0.0f);
                }

                if (up != VDB_Y_UP && up != VDB_Y_DOWN)
                {
                    vdbColor(style.y_axis, 0.0f);            vdbVertex(0.0f, -t, 0.0f);
                    vdbColor(style.y_axis, style.neg_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.y_axis, style.pos_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.y_axis, 0.0f);            vdbVertex(0.0f, +t, 0.0f);
                }

                if (up != VDB_Z_UP && up != VDB_Z_DOWN)
                {
                    vdbColor(style.z_axis, 0.0f);            vdbVertex(0.0f, 0.0f, -t);
                    vdbColor(style.z_axis, style.neg_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.z_axis, style.pos_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.z_axis, 0.0f);            vdbVertex(0.0f, 0.0f, +t);
                }
                vdbEnd();
            }

            vdbPopMatrix(); // pre-permutation

            if (fs->grid.cube_visible)
            {
                vdbLineWidth(1.0f);
                vdbColor(style.cube, 0.3f);
                vdbLineCube(1.0f, 1.0f, 1.0f);
            }

            // draw major and minor grid lines
            if (fs->grid.grid_visible)
            {
                vdbLineWidth(1.0f);
                // todo: this should be a draw-list
                vdbPushMatrix();
                vdbMultMatrix(vdbMatScale(10.0f, 10.0f, 10.0f).data);
                vdbBeginLines();
                for (int major = -9; major <= +9; major++)
                {
                    for (int minor = 1; minor <= 9; minor++)
                    {
                        float t = major/10.0f + minor/100.0f;
                        float alpha = style.minor_alpha*(1.0f - fabsf(t));
                        alpha *= alpha;
                        vdbColor(style.grid, 0.00f); vdbVertex(t, 0.0f, -1.0f);
                        vdbColor(style.grid, alpha); vdbVertex(t, 0.0f, 0.0f);
                        vdbColor(style.grid, alpha); vdbVertex(t, 0.0f, 0.0f);
                        vdbColor(style.grid, 0.00f); vdbVertex(t, 0.0f, +1.0f);

                        vdbColor(style.grid, 0.00f); vdbVertex(-1.0f, 0.0f, t);
                        vdbColor(style.grid, alpha); vdbVertex(0.0f, 0.0f, t);
                        vdbColor(style.grid, alpha); vdbVertex(0.0f, 0.0f, t);
                        vdbColor(style.grid, 0.00f); vdbVertex(+1.0f, 0.0f, t);
                    }

                    if (major == 0)
                        continue;
                    float t = major/10.0f;
                    float alpha = style.major_alpha*(1.0f - fabsf(t));
                    alpha *= alpha;
                    vdbColor(style.grid, 0.00f); vdbVertex(t, 0.0f, -1.0f);
                    vdbColor(style.grid, alpha); vdbVertex(t, 0.0f, 0.0f);
                    vdbColor(style.grid, alpha); vdbVertex(t, 0.0f, 0.0f);
                    vdbColor(style.grid, 0.00f); vdbVertex(t, 0.0f, +1.0f);

                    vdbColor(style.grid, 0.00f); vdbVertex(-1.0f, 0.0f, t);
                    vdbColor(style.grid, alpha); vdbVertex(0.0f, 0.0f, t);
                    vdbColor(style.grid, alpha); vdbVertex(0.0f, 0.0f, t);
                    vdbColor(style.grid, 0.00f); vdbVertex(+1.0f, 0.0f, t);
                }
                vdbEnd();
                vdbPopMatrix();
            }
            vdbDepthTest(false);
        }
        else if (vdb::frame_settings->camera.type == VDB_PLANAR)
        {
            vdbPopMatrix(); // pre-scaling

            // draw colored XYZ axes
            if (fs->grid.grid_visible)
            {
                float t = 10.0f;
                vdbLineWidth(1.0f);
                vdbBeginLines();
                vdbOrientation up = *GetCameraUp();
                if (up != VDB_Z_UP && up != VDB_Z_DOWN)
                {
                    vdbColor(style.x_axis, 0.0f);            vdbVertex(-t, 0.0f, 0.0f);
                    vdbColor(style.x_axis, style.neg_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.x_axis, 1.0f);            vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.x_axis, 0.0f);            vdbVertex(+t, 0.0f, 0.0f);
                }
                if (up != VDB_X_UP && up != VDB_X_DOWN)
                {
                    vdbColor(style.y_axis, 0.0f);            vdbVertex(0.0f, -t, 0.0f);
                    vdbColor(style.y_axis, style.neg_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.y_axis, 1.0f);            vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.y_axis, 0.0f);            vdbVertex(0.0f, +t, 0.0f);
                }
                if (up != VDB_Y_UP && up != VDB_Y_DOWN)
                {
                    vdbColor(style.z_axis, 0.0f);            vdbVertex(0.0f, 0.0f, -t);
                    vdbColor(style.z_axis, style.neg_alpha); vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.z_axis, 1.0f);            vdbVertex(0.0f, 0.0f, 0.0f);
                    vdbColor(style.z_axis, 0.0f);            vdbVertex(0.0f, 0.0f, +t);
                }
                vdbEnd();
            }

            vdbPopMatrix(); // pre-permutation

            // draw major and minor grid lines
            if (fs->grid.grid_visible)
            {
                vdbLineWidth(1.0f);
                // todo: this should be a draw-list
                vdbPushMatrix();
                vdbMultMatrix(vdbMatScale(10.0f, 10.0f, 10.0f).data);
                vdbBeginLines();
                for (int major = -9; major <= +9; major++)
                {
                    for (int minor = 1; minor <= 9; minor++)
                    {
                        float t = major/10.0f + minor/100.0f;
                        float alpha = style.minor_alpha*(1.0f - fabsf(t));
                        alpha *= alpha;
                        vdbColor(style.grid, 0.00f); vdbVertex(t, -1.0f);
                        vdbColor(style.grid, alpha); vdbVertex(t, +0.0f);
                        vdbColor(style.grid, alpha); vdbVertex(t, +0.0f);
                        vdbColor(style.grid, 0.00f); vdbVertex(t, +1.0f);

                        vdbColor(style.grid, 0.00f); vdbVertex(-1.0f, t);
                        vdbColor(style.grid, alpha); vdbVertex(+0.0f, t);
                        vdbColor(style.grid, alpha); vdbVertex(+0.0f, t);
                        vdbColor(style.grid, 0.00f); vdbVertex(+1.0f, t);
                    }

                    if (major == 0)
                        continue;
                    float t = major/10.0f;
                    float alpha = style.major_alpha*(1.0f - fabsf(t));
                    alpha *= alpha;
                    vdbColor(style.grid, 0.00f); vdbVertex(t, -1.0f);
                    vdbColor(style.grid, alpha); vdbVertex(t, +0.0f);
                    vdbColor(style.grid, alpha); vdbVertex(t, +0.0f);
                    vdbColor(style.grid, 0.00f); vdbVertex(t, +1.0f);

                    vdbColor(style.grid, 0.00f); vdbVertex(-1.0f, t);
                    vdbColor(style.grid, alpha); vdbVertex(+0.0f, t);
                    vdbColor(style.grid, alpha); vdbVertex(+0.0f, t);
                    vdbColor(style.grid, 0.00f); vdbVertex(+1.0f, t);
                }
                vdbEnd();
                vdbPopMatrix();
            }
        }
    }

    widgets::EndFrame();

    if (framegrab::active)
    {
        if (!ui::escape_eaten && keys::pressed[VDB_KEY_ESCAPE])
        {
            framegrab::StopRecording();
            ui::escape_eaten = true;
        }
        else if (VDB_HOTKEY_FRAMEGRAB)
        {
            framegrab::StopRecording();
        }
    }

    if (framegrab::active)
    {
        framegrab_options_t opt = framegrab::options;

        if (opt.draw_imgui)
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        GLenum format = opt.alpha_channel ? GL_RGBA : GL_RGB;
        int channels = opt.alpha_channel ? 4 : 3;
        int width = window::framebuffer_width;
        int height = window::framebuffer_height;
        unsigned char *data = (unsigned char*)malloc(width*height*channels);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

        if (!opt.draw_imgui)
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        framegrab::SaveFrame(data, width, height, channels, format);

        free(data);

        window::DontWaitNextFrameEvents();
    }
    else
    {
        ui::MainMenuBar(vdb::frame_settings);
        ui::ShowLogWindows();
        ui::WindowSizeDialog();
        ui::FramegrabDialog();
        ui::ExitDialog();
        ruler::DrawOverlay();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // If any key is down, do not allow window to go idle
    for (int i = 0; i < VDB_NUM_KEYS; i++)
    {
        if (keys::down[i])
        {
            window::DontWaitNextFrameEvents();
            break;
        }
    }

    window::SwapBuffers(1.0f/60.0f);
    CheckGLError();
}
