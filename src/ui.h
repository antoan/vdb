#include "sketch.h"

namespace ui
{
    static bool escape_eaten;
    static bool sketch_mode_active;
    static bool ruler_mode_active;
    static bool window_size_dialog_should_open;
    static bool take_screenshot_should_open;
    static bool record_video_should_open;
    static bool save_logs_should_open;
    static bool hide_logs;

    static ImFont *regular_font;
    static ImFont *big_font;

    // note: this is in window coordinates (ImGui coordinates)
    // note: this may change from frame to frame!
    static float main_menu_bar_height = 0.0f;

    static bool auto_step;

    enum { query_buffer_size = 1024 };
    struct log_window_t
    {
        int counter;
        bool open;
        char label[query_buffer_size];
        char query_buffer[query_buffer_size];
        bool plot_as_histogram;
        log_window_t *next;
    };

    namespace log_windows
    {
        static int counter;
        static log_window_t *first;
    }

    static void MainMenuBar(frame_settings_t *fs);
    static void ExitDialog();
    static void WindowSizeDialog();
    static void FramegrabDialog();
    static void SketchBeginFrame();
    static void SketchEndFrame();
    static void NewLogWindow();
    static void ShowLogWindow(log_window_t *window);
    static void ShowLogWindows();
}

namespace ImGui
{
    // Helper to display a little (?) mark which shows a tooltip when hovered.
    // (copied from imgui_demo.cpp)
    static void ShowHelpMarker(const char* desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
}

static void ui::NewLogWindow()
{
    log_window_t *new_window = (log_window_t*)calloc(1, sizeof(log_window_t));
    new_window->counter = log_windows::counter++;
    new_window->open = true;
    sprintf(new_window->label, "Log %d", new_window->counter);

    if (!log_windows::first)
    {
        log_windows::first = new_window;
    }
    else
    {
        log_window_t *window = log_windows::first;
        log_window_t *prev = NULL;
        while (window)
        {
            prev = window;
            window = window->next;
        }
        prev->next = new_window;
    }
}

static void ui::ShowLogWindow(log_window_t *window)
{
    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
    static char window_title[1024];
    sprintf(window_title, "%s###%s", window->query_buffer, window->label);
    if (!ImGui::Begin(window_title, &window->open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
        ImGui::InputText("##query", window->query_buffer, query_buffer_size);
        ImGui::PopItemWidth();
    }

    log_t *l = logs.Find(window->query_buffer);

    ImVec2 plot_area_size = ImGui::GetContentRegionAvail();

    if (l && l->type == log_type_scalar)
    {
        float scale_min = FLT_MAX;
        float scale_max = FLT_MAX;
        const char *label = "##plot";
        const float *values = &l->data[0];
        int values_count = (int)l->data.size();
        int values_offset = 0;
        const char *overlay = NULL;

        if (values_count == 1)
        {
            static char buffer[1024];
            sprintf(buffer, "%g", values[0]);
            ImGui::PushFont(ui::big_font);
            ImVec2 text_size = ImGui::CalcTextSize(buffer);
            ImGui::SetCursorPosX(plot_area_size.x*0.5f - text_size.x*0.5f);
            ImGui::Text(buffer);
            ImGui::PopFont();
        }
        else
        {
            if (window->plot_as_histogram)
                ImGui::PlotHistogram(label, values, values_count, values_offset, overlay, scale_min, scale_max, plot_area_size);
            else
                ImGui::PlotLines(label, values, values_count, values_offset, overlay, scale_min, scale_max, plot_area_size);
            if (ImGui::IsItemClicked())
                window->plot_as_histogram = !window->plot_as_histogram;
        }
    }
    else if (l && l->type == log_type_matrix)
    {
        int rows = l->rows;
        int cols = l->columns;
        float *data = &l->data[0];
        for (int row = 0; row < rows; row++)
        for (int col = 0; col < cols; col++)
        {
            ImGui::Text("%8.4f", data[row + col*rows]);
            if (col < cols-1)
                ImGui::SameLine();
        }
    }

    ImGui::End();
}

static void ui::ShowLogWindows()
{
    if (!hide_logs)
    {
        log_window_t *window = log_windows::first;
        log_window_t *prev = NULL;
        while (window)
        {
            ShowLogWindow(window);
            if (!window->open)
            {
                log_window_t *next = window->next;
                free(window);
                if (!prev) log_windows::first = next;
                else prev->next = next;
                window = next;
            }
            else
            {
                prev = window;
                window = window->next;
            }
        }
    }

    using namespace ImGui;
    bool enter_button = keys::pressed[VDB_KEY_RETURN];
    bool escape_button = keys::pressed[VDB_KEY_ESCAPE];
    if (save_logs_should_open)
    {
        save_logs_should_open = false;
        OpenPopup("Save logs##popup");
        CaptureKeyboardFromApp(true);
    }
    if (BeginPopupModal("Save logs##popup", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char filename[1024];
        if (IsWindowAppearing())
            SetKeyboardFocusHere();
        InputText("Filename", filename, sizeof(filename));

        if (Button("OK [Enter]", ImVec2(120,0)) || enter_button)
        {
            logs.Dump(filename);
            CloseCurrentPopup();
        }
        SameLine();
        if (Button("Cancel", ImVec2(120,0)))
        {
            CloseCurrentPopup();
        }

        if (escape_button)
        {
            CloseCurrentPopup();
            ui::escape_eaten = true;
        }
        EndPopup();
    }
}

static void ui::MainMenuBar(frame_settings_t *fs)
{
    using namespace ui;

    if (VDB_HOTKEY_LOGS_WINDOW)
        NewLogWindow();

    if (VDB_HOTKEY_AUTO_STEP)
        auto_step = !auto_step;

    // hide/show menu
    if (VDB_HOTKEY_TOGGLE_MENU)
        settings.show_main_menu = !settings.show_main_menu;
    if (!settings.show_main_menu)
    {
        main_menu_bar_height = 0.0f;
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::BeginMainMenuBar();
    main_menu_bar_height = ImGui::GetWindowHeight();
    if (ImGui::BeginMenu("Camera"))
    {
        #define ITEM(s, e) if (ImGui::MenuItem(s, NULL, fs->camera.type == e)) { fs->camera.type = e; fs->camera.dirty = true; }
        ITEM("Disabled", VDB_CUSTOM);
        // ImGui::SameLine(); ImGui::ShowHelpMarker("The built-in camera is disabled. All projection and matrix transforms are controlled through your API calls.");
        ITEM("Planar", VDB_PLANAR);
        // ImGui::SameLine(); ImGui::ShowHelpMarker("A 2D camera (left: pan, right: rotate, wheel: zoom).");
        ITEM("Trackball", VDB_TRACKBALL);
        // ImGui::SameLine(); ImGui::ShowHelpMarker("A 3D camera (left: rotate, WASD: move, wheel: zoom).");
        ITEM("Turntable", VDB_TURNTABLE);
        // ImGui::SameLine(); ImGui::ShowHelpMarker("A 3D camera (left: rotate, wheel: zoom).");
        #undef ITEM
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Grid"))
    {
        if (ImGui::MenuItem("Show grid", NULL, &fs->grid.grid_visible)) fs->grid.dirty = true;
        if (ImGui::MenuItem("Show cube", NULL, &fs->grid.cube_visible)) fs->grid.dirty = true;
        ImGui::SameLine(); ImGui::ShowHelpMarker("Draw a unit cube (from -0.5 to +0.5 in each axis).");
        ImGui::PushItemWidth(60.0f);
        if (ImGui::DragFloat("Major div.", &fs->grid.grid_scale, 0.1f, 0.0f,0.0f,"%.3f", 2.0f)) fs->grid.dirty = true;
        ImGui::PopItemWidth();
        ImGui::SameLine(); ImGui::ShowHelpMarker("The length (in your units) between the major grid lines (the brighter ones).");
        ImGui::Text("Up: ");
        vdbOrientation *up = GetCameraUp();
        bool *dirty = GetCameraDirty();
        if (ImGui::RadioButton("+Z", up, VDB_Z_UP)) *dirty = true; ImGui::SameLine();
        if (ImGui::RadioButton("+Y", up, VDB_Y_UP)) *dirty = true; ImGui::SameLine();
        if (ImGui::RadioButton("+X", up, VDB_X_UP)) *dirty = true; ImGui::SameLine();
        if (ImGui::RadioButton("-Z", up, VDB_Z_DOWN)) *dirty = true; ImGui::SameLine();
        if (ImGui::RadioButton("-Y", up, VDB_Y_DOWN)) *dirty = true; ImGui::SameLine();
        if (ImGui::RadioButton("-X", up, VDB_X_DOWN)) *dirty = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Settings"))
    {
        ImGui::MenuItem("Show menu", "Alt+M", &settings.show_main_menu);
        ImGui::MenuItem("Window size", "Alt+W", &window_size_dialog_should_open);
        ImGui::MenuItem("Never ask on exit", NULL, &settings.never_ask_on_exit);
        ImGui::MenuItem("Can idle", NULL, &settings.can_idle);

        if (ImGui::MenuItem("Bright theme", NULL, settings.global_theme==VDB_BRIGHT_THEME))
        {
            if (settings.global_theme==VDB_BRIGHT_THEME)
                settings.global_theme = VDB_DARK_THEME;
            else
                settings.global_theme = VDB_BRIGHT_THEME;
        }

        if (ImGui::BeginMenu("Auto step delay"))
        {
            if (ImGui::MenuItem("0 ms",   NULL, settings.auto_step_delay_ms==0))    settings.auto_step_delay_ms = 0;
            if (ImGui::MenuItem("100 ms", NULL, settings.auto_step_delay_ms==100))  settings.auto_step_delay_ms = 100;
            if (ImGui::MenuItem("200 ms", NULL, settings.auto_step_delay_ms==200))  settings.auto_step_delay_ms = 200;
            if (ImGui::MenuItem("300 ms", NULL, settings.auto_step_delay_ms==300))  settings.auto_step_delay_ms = 300;
            if (ImGui::MenuItem("500 ms", NULL, settings.auto_step_delay_ms==500))  settings.auto_step_delay_ms = 500;
            if (ImGui::MenuItem("1 sec" , NULL, settings.auto_step_delay_ms==1000)) settings.auto_step_delay_ms = 1000;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Font"))
        {
            if (ImGui::BeginMenu("DPI scale"))
            {
                if (ImGui::MenuItem("100%", NULL, settings.dpi_scale==100)) settings.dpi_scale = 100;
                if (ImGui::MenuItem("125%", NULL, settings.dpi_scale==125)) settings.dpi_scale = 125;
                if (ImGui::MenuItem("150%", NULL, settings.dpi_scale==150)) settings.dpi_scale = 150;
                if (ImGui::MenuItem("175%", NULL, settings.dpi_scale==175)) settings.dpi_scale = 175;
                if (ImGui::MenuItem("200%", NULL, settings.dpi_scale==200)) settings.dpi_scale = 200;
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Larger")) settings.font_size += 1;
            if (ImGui::MenuItem("Smaller") && settings.font_size > 1) settings.font_size -= 1;
            ImGui::Separator();
            if (ImGui::MenuItem("Reset")) settings.font_size = (int)VDB_DEFAULT_FONT_SIZE;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Render scale"))
        {
            #define ITEM(label, _down, _up) \
                if (ImGui::MenuItem(label, NULL, fs->render_scaler.down==_down && fs->render_scaler.up==_up)) { \
                    fs->render_scaler.down = _down; \
                    fs->render_scaler.up = _up; \
                    fs->render_scaler.dirty = true; \
                }
            ITEM("1/1", 0, 0);
            ITEM("1/2", 1, 0);
            ITEM("1/4", 2, 0);
            ITEM("1/8", 3, 0);
            ImGui::Separator();
            ITEM("2/2", 1, 1);
            ITEM("2/4", 2, 1);
            ITEM("2/8", 3, 1);
            ImGui::Separator();
            ITEM("4/4", 2, 2);
            ITEM("4/8", 3, 2);
            ImGui::Separator();
            ITEM("8/8", 3, 3);
            #undef ITEM
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools"))
    {
        if (ImGui::MenuItem("Take screenshot", "Alt+S")) take_screenshot_should_open = true;
        if (ImGui::MenuItem("Record video", "Alt+S")) record_video_should_open = true;
        ImGui::MenuItem("Ruler", "Alt+R", &ruler_mode_active);
        ImGui::MenuItem("Draw", "Alt+D", &sketch_mode_active);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Logs"))
    {
        if (ImGui::MenuItem("New log window", "Alt+L")) NewLogWindow();
        if (ImGui::MenuItem("Save logs", NULL)) save_logs_should_open = true;
        ImGui::MenuItem("Hide logs", NULL, &hide_logs);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (auto_step)
    {
        if (ImGui::MenuItem("Running...")) auto_step = false;
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
        if (ImGui::MenuItem("Paused")) auto_step = true;
        ImGui::PopStyleColor();
    }
    ImGui::EndMainMenuBar();
    ImGui::PopStyleVar();
}

static void ui::ExitDialog()
{
    bool escape = keys::pressed[VDB_KEY_ESCAPE];
    if (escape && !ui::escape_eaten && settings.never_ask_on_exit)
    {
        window::should_quit = true;
        return;
    }
    if (escape && !ui::escape_eaten && !settings.never_ask_on_exit)
    {
        ImGui::OpenPopup("Do you want to exit?##popup_exit");
    }
    if (ImGui::BeginPopupModal("Do you want to exit?##popup_exit", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::Button("Yes"))
        {
            window::should_quit = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Never ask me again", &settings.never_ask_on_exit);
        if (escape && !ImGui::IsWindowAppearing())
        {
            ui::escape_eaten = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void ui::WindowSizeDialog()
{
    if (window_size_dialog_should_open || (VDB_HOTKEY_WINDOW_SIZE))
    {
        window_size_dialog_should_open = false;
        ImGui::OpenPopup("Set window size##popup");
        ImGui::CaptureKeyboardFromApp(true);
    }
    if (ImGui::BeginPopupModal("Set window size##popup", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static int width = 0, height = 0;
        if (ImGui::IsWindowAppearing())
        {
            width = window::window_width;
            height = window::window_height;
        }

        static bool topmost = false;
        ImGui::InputInt("Width", &width);
        ImGui::InputInt("Height", &height);
        ImGui::Separator();
        ImGui::Checkbox("Topmost", &topmost);

        if (ImGui::Button("OK", ImVec2(120,0)) || keys::pressed[VDB_KEY_RETURN])
        {
            window::SetSize(width, height, topmost);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120,0)))
        {
            ImGui::CloseCurrentPopup();
        }
        if (keys::pressed[VDB_KEY_ESCAPE])
        {
            ImGui::CloseCurrentPopup();
            ui::escape_eaten = true;
        }
        ImGui::EndPopup();
    }
}

static void ui::FramegrabDialog()
{
    using namespace ImGui;
    bool enter_button = keys::pressed[VDB_KEY_RETURN];
    bool escape_button = keys::pressed[VDB_KEY_ESCAPE];
    if (take_screenshot_should_open ||
        record_video_should_open ||
        (VDB_HOTKEY_FRAMEGRAB))
    {
        take_screenshot_should_open = false;
        record_video_should_open = false;
        OpenPopup("Take screenshot##popup");
        CaptureKeyboardFromApp(true);
    }
    if (BeginPopupModal("Take screenshot##popup", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char filename[1024] = { 's','c','r','e','e','n','s','h','o','t','%','0','4','d','.','p','n','g',0 };
        if (IsWindowAppearing())
            SetKeyboardFocusHere();
        InputText("Filename", filename, sizeof(filename));

        static bool alpha = false;
        static int mode = 0;
        const int mode_single = 0;
        const int mode_sequence = 1;
        const int mode_ffmpeg = 2;
        static bool draw_imgui = false;
        static bool draw_cursor = false;
        RadioButton("Screenshot", &mode, mode_single);
        SameLine();
        ImGui::ShowHelpMarker("Take a single screenshot. Put a %d in the filename to use the counter for successive screenshots.");
        SameLine();
        RadioButton("Sequence", &mode, mode_sequence);
        SameLine();
        ImGui::ShowHelpMarker("Record a video of images in succession (e.g. output0000.png, output0001.png, ... etc.). Put a %d in the filename to get frame numbers. Use %0nd to left-pad with n zeroes.");
        SameLine();
        RadioButton("ffmpeg", &mode, mode_ffmpeg);
        SameLine();
        ImGui::ShowHelpMarker("Record a video with raw frames piped directly to ffmpeg, and save the output in the format specified by your filename extension (e.g. mp4). This option can be quicker as it avoids writing to the disk.\nMake sure the 'ffmpeg' executable is visible from the terminal you launched this program in.");

        Checkbox("Alpha (32bpp)", &alpha);
        SameLine();
        Checkbox("Draw GUI", &draw_imgui);
        SameLine();
        Checkbox("Draw cursor", &draw_cursor);

        if (mode == mode_single)
        {
            static bool do_continue = true;
            static int start_from = 0;
            Checkbox("Continue counting", &do_continue);
            SameLine();
            ImGui::ShowHelpMarker("Enable this to continue the image filename number suffix from the last screenshot captured (in this program session).");
            if (!do_continue)
            {
                SameLine();
                PushItemWidth(100.0f);
                InputInt("Start from", &start_from);
            }
            if (Button("OK [Enter]", ImVec2(120,0)) || enter_button)
            {
                framegrab_options_t opt = {0};
                opt.filename = filename;
                opt.reset_counter = !do_continue;
                opt.start_from = start_from;
                opt.draw_imgui = draw_imgui;
                opt.draw_cursor = draw_cursor;
                opt.alpha_channel = alpha;
                framegrab::TakeScreenshot(opt);
                CloseCurrentPopup();
            }
            SameLine();
            if (Button("Cancel", ImVec2(120,0)))
            {
                CloseCurrentPopup();
            }
        }
        else if (mode == mode_sequence)
        {
            static bool do_continue = false;
            static int start_from = 0;
            static int frame_cap = 0;
            InputInt("Number of frames", &frame_cap);
            SameLine();
            ImGui::ShowHelpMarker("0 for unlimited. To stop the recording at any time, press the same hotkey you used to open this dialog (CTRL+S by default).");

            Checkbox("Continue from last frame", &do_continue);
            SameLine();
            ImGui::ShowHelpMarker("Enable this to continue the image filename number suffix from the last image sequence that was recording (in this program session).");
            if (!do_continue)
            {
                SameLine();
                PushItemWidth(100.0f);
                InputInt("Start from", &start_from);
            }

            if (Button("Start [Enter]", ImVec2(120,0)) || enter_button)
            {
                framegrab_options_t opt = {0};
                opt.filename = filename;
                opt.alpha_channel = alpha;
                opt.draw_cursor = draw_cursor;
                opt.draw_imgui = draw_imgui;
                opt.video_frame_cap = frame_cap;
                opt.reset_counter = !do_continue;
                framegrab::RecordImageSequence(opt);
                CloseCurrentPopup();
            }
            SameLine();
            ImGui::ShowHelpMarker("Press ESCAPE or CTRL+S to stop.");
            SameLine();
            if (Button("Cancel", ImVec2(120,0)))
            {
                CloseCurrentPopup();
            }
        }
        else if (mode == mode_ffmpeg)
        {
            static int frame_cap = 0;
            static float framerate = 60;
            static int crf = 21;
            InputInt("Number of frames", &frame_cap);
            SameLine();
            ImGui::ShowHelpMarker("0 for unlimited. To stop the recording at any time, press the same hotkey you used to open this dialog (CTRL+S by default).");
            SliderInt("Quality (lower is better)", &crf, 1, 51);
            InputFloat("Framerate", &framerate);

            if (Button("Start [Enter]", ImVec2(120,0)) || enter_button)
            {
                framegrab_options_t opt = {0};
                opt.filename = filename;
                opt.alpha_channel = alpha;
                opt.draw_cursor = draw_cursor;
                opt.draw_imgui = draw_imgui;
                opt.ffmpeg_crf = crf;
                opt.ffmpeg_fps = framerate;
                opt.video_frame_cap = frame_cap;
                framegrab::RecordFFmpeg(opt);
                CloseCurrentPopup();
            }
            SameLine();
            ImGui::ShowHelpMarker("Press ESCAPE or CTRL+S to stop.");
            SameLine();
            if (Button("Cancel", ImVec2(120,0)))
            {
                CloseCurrentPopup();
            }
        }

        if (escape_button)
        {
            CloseCurrentPopup();
            ui::escape_eaten = true;
        }
        EndPopup();
    }
}

static void ui::SketchBeginFrame()
{
    if (VDB_HOTKEY_SKETCH_MODE)
        sketch_mode_active = !sketch_mode_active;

    if (sketch_mode_active)
    {
        if (keys::pressed[VDB_KEY_ESCAPE])
        {
            ui::sketch_mode_active = false;
            ui::escape_eaten = true;
        }
        bool undo = vdbIsKeyDown(VDB_KEY_LCTRL) && vdbWasKeyPressed(VDB_KEY_Z);
        bool redo = vdbIsKeyDown(VDB_KEY_LCTRL) && vdbWasKeyPressed(VDB_KEY_Y);
        bool clear = vdbWasKeyPressed(VDB_KEY_D);
        bool click = vdbIsMouseLeftDown();
        float x = vdbGetMousePos().x;
        float y = vdbGetMousePos().y;
        sketch_mode::Update(undo, redo, clear, click, x, y);

        // force all subsequent calls to vdb{Is,Was}{Mouse,Key}{UpDownPressed} to return false
        ImGui::GetIO().WantCaptureKeyboard = true;
        ImGui::GetIO().WantCaptureMouse = true;
    }
}

static void ui::SketchEndFrame()
{
    if (!ui::sketch_mode_active)
        return;

    static float width = 2.0f;
    static float brightness = 0.0f;

    // draw sketching tool bar
    ImGui::BeginMainMenuBar();
    float h_menu = ImGui::GetFrameHeight();
    float h_rect = ImGui::GetTextLineHeight();
    {
        ImGui::Separator();
        ImGui::PushID("sketch colors");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f,-1.0f));
        ImDrawList *draw = ImGui::GetWindowDrawList();
        for (int i = 0; i < sketch_mode::num_colors; i++)
        {
            ImGui::PushID(i);
            ImGui::InvisibleButton("##", ImVec2(16.0f, h_rect));
            ImVec2 a = ImGui::GetItemRectMin();
            ImVec2 b = ImGui::GetItemRectMax();
            a.y = 0.5f*(h_menu - h_rect);
            b.y = 0.5f*(h_menu + h_rect);
            ImU32 color = sketch_mode::palette[i];
            draw->AddRectFilled(a, b, color);
            if (ImGui::IsItemClicked())
                sketch_mode::s.color_index = i;
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::PushItemWidth(64.0f);
        ImGui::SliderFloat("brightness", &brightness, -1.0f, +1.0f);
        ImGui::PopItemWidth();
        ImGui::PopID();
    }
    ImGui::EndMainMenuBar();

    int num_lines = sketch_mode::s.num_lines;
    sketch_mode_line_t *lines = sketch_mode::s.lines;
    bool is_drawing = sketch_mode::s.is_drawing;

    ImDrawList *user_draw_list = ImGui::GetOverlayDrawList();

    // draw brightness overlay
    {
        int alpha = (int)(fabsf(brightness)*255);
        ImU32 color = brightness > 0.0f ? IM_COL32(255,255,255,alpha) : IM_COL32(0,0,0,alpha);
        ImVec2 a = ImVec2(0.0f, h_menu);
        ImVec2 b = ImGui::GetIO().DisplaySize;
        user_draw_list->AddRectFilled(a, b, color);
    }

    // draw lines
    {
        int n = (is_drawing) ? num_lines+1 : num_lines;
        ImVec2 next_a;
        for (int i = 0; i < n; i++)
        {
            ImVec2 a;
            if (lines[i].connected)
            {
                assert(i > 0);
                a = next_a;
            }
            else
            {
                a.x = lines[i].x1;
                a.y = lines[i].y1;
            }
            ImVec2 b = ImVec2(lines[i].x2, lines[i].y2);
            if (i < n-1 && lines[i+1].connected)
                next_a = b;
            user_draw_list->AddLine(a, b, lines[i].color, width);
        }
    }
}
