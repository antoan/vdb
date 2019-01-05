struct ui_icon_t
{
    GLuint handle;
    int width,height;
};

namespace uistuff
{
    static bool escape_eaten;
    static bool sketch_mode_active;
    static bool ruler_mode_active;
    static ui_icon_t icon_camera;

    static void CameraToolBar(frame_settings_t *fs);
    static void ExitDialog();
    static void WindowSizeDialog();
    static void FramegrabDialog();
}

static void uistuff::CameraToolBar(frame_settings_t *fs)
{
    using namespace uistuff;
    if (!icon_camera.handle)
    {
        int width,height,channels;
        unsigned char *data = stbi_load_from_memory(
            (const unsigned char*)icon_camera_data, icon_camera_size,
            &width, &height, &channels, 4);
        assert(data);
        GLuint handle = TexImage2D(data, width, height, GL_RGBA, GL_UNSIGNED_BYTE);
        free(data);

        icon_camera.handle = handle;
        icon_camera.width = width;
        icon_camera.height = height;
    }
    assert(icon_camera.handle);

    ImGui::PushID("vdbCameraToolBar");
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::SetNextWindowPos(ImVec2((float)(vdbGetWindowWidth() - 60.0f), 0.0f));
    ImGui::SetNextWindowSize(ImVec2(60.0f, -1.0f));
    ImGui::Begin("", NULL, flags);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
    static ImVec4 tint_col = ImVec4(1,1,1, 0.4f);
    if (ImGui::ImageButton(
            (void*)(intptr_t)icon_camera.handle,
            ImVec2(38.0f, 38.0f),
            ImVec2(0,0),
            ImVec2(1,1),
            -1,
            ImVec4(0,0,0,0),
            tint_col))
    {
        ImGui::OpenPopup("Built-in camera");
    }
    if (ImGui::IsItemActive()) tint_col = ImVec4(1,1,1, 1.0f);
    else if (ImGui::IsItemHovered()) tint_col = ImVec4(1,1,1, 0.7f);
    else tint_col = ImVec4(1,1,1, 0.4f);
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupModal("Built-in camera", NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::Text("Camera:");
        ImGui::RadioButton("Disabled", &fs->camera_type, VDB_CAMERA_USER); ImGui::SameLine();
        ImGui::RadioButton("Planar", &fs->camera_type, VDB_CAMERA_2D); ImGui::SameLine();
        ImGui::RadioButton("Trackball", &fs->camera_type, VDB_CAMERA_TRACKBALL); ImGui::SameLine();
        ImGui::RadioButton("Turntable", &fs->camera_type, VDB_CAMERA_TURNTABLE);
        ImGui::Text("Grid:");
        ImGui::Checkbox("Show grid", &fs->grid_visible); ImGui::SameLine();
        ImGui::DragFloat("Scale##grid", &fs->grid_scale);
        ImGui::Checkbox("Show cube", &fs->cube_visible); ImGui::SameLine();
        ImGui::DragFloat("Scale##cube", &fs->cube_scale);
        ImGui::Text("Floor:");
        ImGui::RadioButton("XY", &fs->camera_floor, VDB_FLOOR_XY); ImGui::SameLine();
        ImGui::RadioButton("XZ", &fs->camera_floor, VDB_FLOOR_XZ); ImGui::SameLine();
        ImGui::RadioButton("YZ", &fs->camera_floor, VDB_FLOOR_YZ);

        if (ImGui::Button("OK##camera settings", ImVec2(60,0)) || keys::pressed[SDL_SCANCODE_RETURN])
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##camera settings", ImVec2(60,0)) || keys::pressed[SDL_SCANCODE_ESCAPE])
        {
            escape_eaten = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::End();
    ImGui::PopID();
}

static void uistuff::ExitDialog()
{
    bool escape = keys::pressed[SDL_SCANCODE_ESCAPE];
    if (escape && !uistuff::escape_eaten && settings.never_ask_on_exit)
    {
        window::should_quit = true;
        return;
    }
    if (escape && !uistuff::escape_eaten && !settings.never_ask_on_exit)
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
            uistuff::escape_eaten = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void uistuff::WindowSizeDialog()
{
    if (VDB_HOTKEY_WINDOW_SIZE)
    {
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

        if (ImGui::Button("OK", ImVec2(120,0)) || keys::pressed[SDL_SCANCODE_RETURN])
        {
            window::SetSize(width, height, topmost);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120,0)))
        {
            ImGui::CloseCurrentPopup();
        }
        if (keys::pressed[SDL_SCANCODE_ESCAPE])
        {
            ImGui::CloseCurrentPopup();
            uistuff::escape_eaten = true;
        }
        ImGui::EndPopup();
    }
}

static void uistuff::FramegrabDialog()
{
    using namespace ImGui;
    bool enter_button = keys::pressed[SDL_SCANCODE_RETURN];
    bool escape_button = keys::pressed[SDL_SCANCODE_ESCAPE];
    if (VDB_HOTKEY_FRAMEGRAB)
    {
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
        ShowHelpMarker("Take a single screenshot. Put a %d in the filename to use the counter for successive screenshots.");
        SameLine();
        RadioButton("Sequence", &mode, mode_sequence);
        SameLine();
        ShowHelpMarker("Record a video of images in succession (e.g. output0000.png, output0001.png, ... etc.). Put a %d in the filename to get frame numbers. Use %0nd to left-pad with n zeroes.");
        SameLine();
        RadioButton("ffmpeg", &mode, mode_ffmpeg);
        SameLine();
        ShowHelpMarker("Record a video with raw frames piped directly to ffmpeg, and save the output in the format specified by your filename extension (e.g. mp4). This option can be quicker as it avoids writing to the disk.\nMake sure the 'ffmpeg' executable is visible from the terminal you launched this program in.");

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
            ShowHelpMarker("Enable this to continue the image filename number suffix from the last screenshot captured (in this program session).");
            if (!do_continue)
            {
                SameLine();
                PushItemWidth(100.0f);
                InputInt("Start from", &start_from);
            }
            if (Button("OK", ImVec2(120,0)) || enter_button)
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
            ShowHelpMarker("0 for unlimited. To stop the recording at any time, press the same hotkey you used to open this dialog (CTRL+S by default).");

            Checkbox("Continue from last frame", &do_continue);
            SameLine();
            ShowHelpMarker("Enable this to continue the image filename number suffix from the last image sequence that was recording (in this program session).");
            if (!do_continue)
            {
                SameLine();
                PushItemWidth(100.0f);
                InputInt("Start from", &start_from);
            }

            if (Button("Start", ImVec2(120,0)) || enter_button)
            {
                framegrab_options_t opt = {0};
                opt.filename = filename;
                opt.alpha_channel = alpha;
                opt.draw_cursor = draw_cursor;
                opt.draw_imgui = draw_imgui;
                opt.video_frame_cap = frame_cap;
                opt.reset_counter = !do_continue;
                framegrab::RecordImageSequence(opt);
            }
            SameLine();
            ShowHelpMarker("Press ESCAPE or CTRL+S to stop.");
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
            ShowHelpMarker("0 for unlimited. To stop the recording at any time, press the same hotkey you used to open this dialog (CTRL+S by default).");
            SliderInt("Quality (lower is better)", &crf, 1, 51);
            InputFloat("Framerate", &framerate);

            if (Button("Start", ImVec2(120,0)) || enter_button)
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
            }
            SameLine();
            ShowHelpMarker("Press ESCAPE or CTRL+S to stop.");
            SameLine();
            if (Button("Cancel", ImVec2(120,0)))
            {
                CloseCurrentPopup();
            }
        }

        if (escape_button)
        {
            CloseCurrentPopup();
            uistuff::escape_eaten = true;
        }
        EndPopup();
    }
}
