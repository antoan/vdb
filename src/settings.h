#include <stdio.h>
#include <stdlib.h>

vdbHintKey VDB_CAMERA_TYPE = 0;
vdbHintKey VDB_ORIENTATION = 1;
vdbHintKey VDB_VIEW_SCALE  = 2;
vdbHintKey VDB_SHOW_GRID   = 3;
vdbHintKey VDB_CAMERA_KEY  = 4;
vdbHintKey VDB_THEME       = 5;

vdbCameraType VDB_CUSTOM    = 0;
vdbCameraType VDB_PLANAR    = 1;
vdbCameraType VDB_TRACKBALL = 2;
vdbCameraType VDB_TURNTABLE = 3;

vdbOrientation VDB_Z_UP     = 0;
vdbOrientation VDB_Y_UP     = 1;
vdbOrientation VDB_X_UP     = 2;
vdbOrientation VDB_Z_DOWN   = 3;
vdbOrientation VDB_Y_DOWN   = 4;
vdbOrientation VDB_X_DOWN   = 5;

vdbTheme VDB_DARK_THEME     = 0;
vdbTheme VDB_BRIGHT_THEME   = 1;

enum { MAX_FRAME_SETTINGS = 1024 };
enum { VDB_MAX_RENDER_SCALE_DOWN = 3 };
enum { VDB_MAX_RENDER_SCALE_UP = 3 };

struct camera_trackball_settings_t
{
    bool dirty;
    vdbMat4 R; // world to camera
    vdbVec4 T; // camera relative world in world
    float zoom;
    vdbOrientation up;
};

struct camera_turntable_settings_t
{
    bool dirty;
    float angle_x;
    float angle_y;
    float radius;
    vdbOrientation up;
};

struct camera_planar_settings_t
{
    bool dirty;
    vdbVec2 position;
    float angle;
    float zoom;
    vdbOrientation up;
};

struct projection_settings_t
{
    bool dirty;
    float y_fov;
    float min_depth;
    float max_depth;
};

struct grid_settings_t
{
    bool dirty;
    bool grid_visible;
    float grid_scale;
    bool cube_visible;
};

struct render_scaler_settings_t
{
    bool dirty;
    int down;
    int up;
};

struct camera_settings_t
{
    bool dirty;
    projection_settings_t projection;
    vdbKey key;
    vdbCameraType type;
    camera_trackball_settings_t trackball;
    camera_turntable_settings_t turntable;
    camera_planar_settings_t planar;
};

struct frame_settings_t
{
    char *name;
    camera_settings_t camera;
    render_scaler_settings_t render_scaler;
    grid_settings_t grid;
};

struct global_camera_settings_t
{
    float mouse_sensitivity;
    float scroll_sensitivity;
    float move_speed_normal;
    float move_speed_slow;
};

struct window_settings_t
{
    int x,y,width,height;
};

struct settings_t
{
    global_camera_settings_t camera;
    window_settings_t window;
    frame_settings_t frames[MAX_FRAME_SETTINGS];
    int num_frames;
    bool never_ask_on_exit;
    bool show_main_menu;
    int font_size;
    bool can_idle;
    int auto_step_delay_ms;
    int dpi_scale;
    vdbTheme global_theme;

    void LoadOrDefault(const char *filename);
    void Save(const char *filename);
};

static settings_t settings;
static frame_settings_t *GetFrameSettings(); // defined in vdb.cpp

vdbOrientation *GetCameraUp()
{
    static vdbOrientation dummy = VDB_Z_UP;
    frame_settings_t *fs = GetFrameSettings();
    if      (fs->camera.type == VDB_PLANAR)    return &fs->camera.planar.up;
    else if (fs->camera.type == VDB_TRACKBALL) return &fs->camera.trackball.up;
    else if (fs->camera.type == VDB_TURNTABLE) return &fs->camera.turntable.up;
    return &dummy;
}

bool *GetCameraDirty()
{
    static bool dummy = false;
    frame_settings_t *fs = GetFrameSettings();
    if      (fs->camera.type == VDB_PLANAR)    return &fs->camera.planar.dirty;
    else if (fs->camera.type == VDB_TRACKBALL) return &fs->camera.trackball.dirty;
    else if (fs->camera.type == VDB_TURNTABLE) return &fs->camera.turntable.dirty;
    return &dummy;
}

void DefaultFrameSettings(frame_settings_t *fs)
{
    fs->camera.dirty = false;
    fs->camera.type = VDB_CUSTOM;
    fs->camera.key = VDB_KEY_INVALID;
    fs->camera.trackball.dirty = false;
    fs->camera.trackball.R = vdbMatIdentity();
    fs->camera.trackball.T = vdbVec4(0.0f, 0.0f, 0.0f, 1.0f);
    fs->camera.trackball.zoom = 1.0f;
    fs->camera.trackball.up = VDB_Z_UP;
    fs->camera.turntable.dirty = false;
    fs->camera.turntable.angle_x = 0.0f;
    fs->camera.turntable.angle_y = 0.0f;
    fs->camera.turntable.radius = 1.0f;
    fs->camera.turntable.up = VDB_Z_UP;
    fs->camera.planar.dirty = false;
    fs->camera.planar.position.x = 0.0f;
    fs->camera.planar.position.y = 0.0f;
    fs->camera.planar.zoom = 1.0f;
    fs->camera.planar.angle = 0.0f;
    fs->camera.planar.up = VDB_Y_UP;
    fs->camera.projection.dirty = false;
    fs->camera.projection.y_fov = 0.7f;
    fs->camera.projection.min_depth = 0.1f;
    fs->camera.projection.max_depth = 50.0f;
    fs->grid.dirty = false;
    fs->grid.grid_visible = false;
    fs->grid.grid_scale = 2.0f;
    fs->grid.cube_visible = false;
    fs->render_scaler.dirty = false;
    fs->render_scaler.down = 0;
    fs->render_scaler.up = 0;
}

namespace settings_parser
{
    static bool IsAlphaNumeric(char c)
    {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9');
    }

    static void ParseBlank(const char **c)
    {
        while (**c == ' ' || **c == '\n' || **c == '\r' || **c == '\t')
            *c = *c + 1;
    }

    static void ClampInt(int *x, int x_min, int x_max)
    {
        if (*x < x_min) *x = x_min;
        if (*x > x_max) *x = x_max;
    }

    static bool ParseInt(const char **c, int *x, int x_min=0, int x_max=0)
    {
        ParseBlank(c);
        int b;
        if (1 == sscanf(*c, "%d%n", x, &b))
        {
            if (x_min != x_max)
                ClampInt(x, x_min, x_max);
            *c = *c + b;
            return true;
        }
        return false;
    }

    static bool ParseFloat(const char **c, float *x)
    {
        ParseBlank(c);
        int b;
        if (1 == sscanf(*c, "%f%n", x, &b))
        {
            *c = *c + b;
            return true;
        }
        return false;
    }

    static bool ParseFloatToInt(const char **c, int *x, int x_min, int x_max)
    {
        ParseBlank(c);
        int b;
        float f;
        if (1 == sscanf(*c, "%f%n", &f, &b))
        {
            *x = (int)f;
            ClampInt(x, x_min, x_max);
            *c = *c + b;
            return true;
        }
        return false;
    }

    static bool ParseComma(const char **c)
    {
        ParseBlank(c);
        if (**c == ',')
        {
            *c = *c + 1;
            return true;
        }
        return false;
    }

    static bool ParseString(const char **c, const char *match)
    {
        ParseBlank(c);
        const char *a = *c;
        const char *b = match;
        while (*a && *b)
        {
            if (*a != *b)
                return false;
            a++;
            b++;
        }
        if (*b) return false;
        *c = a;
        return true;
    }

    static bool ParseKey(const char **c, const char *match)
    {
        ParseBlank(c);
        if (!ParseString(c, match)) return false;
        ParseBlank(c);
        if (**c != '=') return false;
        *c = *c + 1;
        return true;
    }

    static bool ParseBool(const char **c, bool *x)
    {
        ParseBlank(c);
             if (**c == '0')              { *x = false; *c = *c + 1; return true; }
        else if (**c == '1')              { *x = true; *c = *c + 1; return true; }
        else if (ParseString(c, "False")) { *x = false; return true; }
        else if (ParseString(c, "True"))  { *x = true; return true; }
        else if (ParseString(c, "false")) { *x = false; return true; }
        else if (ParseString(c, "true"))  { *x = true; return true; }
        return false;
    }

    static bool ParseCameraType(const char **c, vdbCameraType *type)
    {
        ParseBlank(c);
        if      (ParseString(c, "disabled"))  { *type = VDB_CUSTOM; return true; }
        else if (ParseString(c, "planar"))    { *type = VDB_PLANAR; return true; }
        else if (ParseString(c, "trackball")) { *type = VDB_TRACKBALL; return true; }
        else if (ParseString(c, "turntable")) { *type = VDB_TURNTABLE; return true; }
        return false;
    }

    static bool ParseCameraUp(const char **c, vdbOrientation *up)
    {
        ParseBlank(c);
        if      (ParseString(c, "z_up"))   { *up = VDB_Z_UP; return true; }
        else if (ParseString(c, "y_up"))   { *up = VDB_Y_UP; return true; }
        else if (ParseString(c, "x_up"))   { *up = VDB_X_UP; return true; }
        else if (ParseString(c, "z_down")) { *up = VDB_Z_DOWN; return true; }
        else if (ParseString(c, "y_down")) { *up = VDB_Y_DOWN; return true; }
        else if (ParseString(c, "x_down")) { *up = VDB_X_DOWN; return true; }
        return false;
    }

    static bool ParseTheme(const char **c, vdbTheme *theme)
    {
        ParseBlank(c);
        if      (ParseString(c, "dark"))    { *theme = VDB_DARK_THEME; return true; }
        else if (ParseString(c, "bright"))  { *theme = VDB_BRIGHT_THEME; return true; }
        return false;
    }

    static bool ParseInt2(const char **c, int *x, int *y)
    {
        int tempx,tempy;
        ParseBlank(c);
        if (!ParseInt(c, &tempx)) return false;
        if (!ParseComma(c))       return false;
        if (!ParseInt(c, &tempy)) return false;
        *x = tempx;
        *y = tempy;
        return true;
    }

    static bool ParseMat4(const char **c, vdbMat4 *x)
    {
        vdbMat4 temp;
        ParseBlank(c);
        for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
        {
            if ((col > 0 || row > 0) && !ParseComma(c)) return false;
            if (!ParseFloat(c, &temp(row,col))) return false;
        }
        *x = temp;
        return true;
    }

    static bool ParseMatR(const char **c, vdbMat4 *x)
    {
        if (!ParseMat4(c, x))
            return false;
        (*x) = vdbMatOrthogonalize(*x);
        return true;
    }

    static bool ParseVec2(const char **c, vdbVec2 *x)
    {
        vdbVec2 temp;
        ParseBlank(c);
        if (!ParseFloat(c, &temp.x)) return false;
        if (!ParseComma(c))          return false;
        if (!ParseFloat(c, &temp.y)) return false;
        *x = temp;
        return true;
    }

    static bool ParseVec4(const char **c, vdbVec4 *x)
    {
        vdbVec4 temp;
        ParseBlank(c);
        if (!ParseFloat(c, &temp.x)) return false;
        if (!ParseComma(c))        return false;
        if (!ParseFloat(c, &temp.y)) return false;
        if (!ParseComma(c))        return false;
        if (!ParseFloat(c, &temp.z)) return false;
        if (!ParseComma(c))        return false;
        if (!ParseFloat(c, &temp.w)) return false;
        *x = temp;
        return true;
    }
}

void settings_t::LoadOrDefault(const char *filename)
{
    camera.mouse_sensitivity = 50.0f;
    camera.scroll_sensitivity = 5.0f;
    camera.move_speed_normal = 1.0f;
    camera.move_speed_slow = 0.5f;
    window.x = -1;
    window.y = -1;
    window.width = 1000;
    window.height = 600;
    dpi_scale = 100;
    never_ask_on_exit = false;
    show_main_menu = true;
    can_idle = false;
    num_frames = 0;
    auto_step_delay_ms = 250;
    font_size = (int)(VDB_DEFAULT_FONT_SIZE);
    global_theme = VDB_DARK_THEME;

    char *data = NULL;
    {
        FILE *f = fopen(filename, "rb");
        if (!f) return;
        if (fseek(f, 0, SEEK_END)) { fclose(f); return; }
        int len = (int)ftell(f);
        if (len <= 0)        { fclose(f); return; }
        if (fseek(f, 0, SEEK_SET)) { fclose(f); return; }
        data = (char*)malloc(len + 1);
        if (!fread(data, 1, len, f)) { fclose(f); free(data); return; }
        data[len] = '\0';
        fclose(f);
    }
    const char *_c = data;
    const char **c = &_c;

    frame_settings_t *frame = NULL;

    while (**c)
    {
        using namespace settings_parser;
        if (ParseKey(c, "[frame]"))
        {
            if (num_frames == MAX_FRAME_SETTINGS)
            {
                frame = NULL;
                fprintf(stderr, "vdb: Reached max number of stored per-block settings. You should clean up your vdb.ini file!\n");
                continue;
            }
            assert(num_frames < MAX_FRAME_SETTINGS);
            frame = frames + (num_frames++);
            const char *name_begin = *c;
            while (**c && !(**c == '\n' || **c == '\r'))
                *c = *c + 1;
            const char *name_end = *c;
            size_t len = name_end - name_begin;
            frame->name = (char*)malloc(len + 1);
            memcpy(frame->name, name_begin, len);
            frame->name[len] = '\0';
            DefaultFrameSettings(frame);
        }
        else if (frame)
        {
                 if (ParseKey(c, "camera_type"))        { ParseCameraType(c, &frame->camera.type);                 frame->camera.dirty = true; }
            else if (ParseKey(c, "turntable_angle_x"))  { ParseFloat(c,      &frame->camera.turntable.angle_x);    frame->camera.turntable.dirty = true; }
            else if (ParseKey(c, "turntable_angle_y"))  { ParseFloat(c,      &frame->camera.turntable.angle_y);    frame->camera.turntable.dirty = true; }
            else if (ParseKey(c, "turntable_radius"))   { ParseFloat(c,      &frame->camera.turntable.radius);     frame->camera.turntable.dirty = true; }
            else if (ParseKey(c, "turntable_up"))       { ParseCameraUp(c,   &frame->camera.turntable.up);         frame->camera.turntable.dirty = true; }
            else if (ParseKey(c, "planar_position"))    { ParseVec2(c,       &frame->camera.planar.position);      frame->camera.planar.dirty = true; }
            else if (ParseKey(c, "planar_zoom"))        { ParseFloat(c,      &frame->camera.planar.zoom);          frame->camera.planar.dirty = true; }
            else if (ParseKey(c, "planar_angle"))       { ParseFloat(c,      &frame->camera.planar.angle);         frame->camera.planar.dirty = true; }
            else if (ParseKey(c, "planar_up"))          { ParseCameraUp(c,   &frame->camera.planar.up);            frame->camera.planar.dirty = true; }
            else if (ParseKey(c, "trackball_R"))        { ParseMatR(c,       &frame->camera.trackball.R);          frame->camera.trackball.dirty = true; }
            else if (ParseKey(c, "trackball_T"))        { ParseVec4(c,       &frame->camera.trackball.T);          frame->camera.trackball.dirty = true; }
            else if (ParseKey(c, "trackball_zoom"))     { ParseFloat(c,      &frame->camera.trackball.zoom);       frame->camera.trackball.dirty = true; }
            else if (ParseKey(c, "trackball_up"))       { ParseCameraUp(c,   &frame->camera.trackball.up);         frame->camera.trackball.dirty = true; }
            else if (ParseKey(c, "y_fov"))              { ParseFloat(c,      &frame->camera.projection.y_fov);     frame->camera.projection.dirty = true; }
            else if (ParseKey(c, "min_depth"))          { ParseFloat(c,      &frame->camera.projection.min_depth); frame->camera.projection.dirty = true; }
            else if (ParseKey(c, "max_depth"))          { ParseFloat(c,      &frame->camera.projection.max_depth); frame->camera.projection.dirty = true; }
            else if (ParseKey(c, "grid_visible"))       { ParseBool(c,       &frame->grid.grid_visible);           frame->grid.dirty = true; }
            else if (ParseKey(c, "grid_scale"))         { ParseFloat(c,      &frame->grid.grid_scale);             frame->grid.dirty = true; }
            else if (ParseKey(c, "cube_visible"))       { ParseBool(c,       &frame->grid.cube_visible);           frame->grid.dirty = true; }
            else if (ParseKey(c, "render_scale_down"))  { ParseInt(c,        &frame->render_scaler.down, 0, VDB_MAX_RENDER_SCALE_DOWN); frame->render_scaler.dirty = true; }
            else if (ParseKey(c, "render_scale_up"))    { ParseInt(c,        &frame->render_scaler.up, 0, VDB_MAX_RENDER_SCALE_UP); frame->render_scaler.dirty = true; }
            else *c = *c + 1;
        }
        else if (ParseKey(c, "window_pos"))         ParseInt2(c,       &window.x, &window.y);
        else if (ParseKey(c, "window_size"))        ParseInt2(c,       &window.width, &window.height);
        else if (ParseKey(c, "never_ask_on_exit"))  ParseBool(c,       &never_ask_on_exit);
        else if (ParseKey(c, "show_main_menu"))     ParseBool(c,       &show_main_menu);
        else if (ParseKey(c, "mouse_sensitivity"))  ParseFloat(c,      &camera.mouse_sensitivity);
        else if (ParseKey(c, "scroll_sensitivity")) ParseFloat(c,      &camera.scroll_sensitivity);
        else if (ParseKey(c, "move_speed_normal"))  ParseFloat(c,      &camera.move_speed_normal);
        else if (ParseKey(c, "move_speed_slow"))    ParseFloat(c,      &camera.move_speed_slow);
        else if (ParseKey(c, "font_size"))          ParseFloatToInt(c, &font_size, 6, 96);
        else if (ParseKey(c, "dpi_scale"))          ParseFloatToInt(c, &dpi_scale, 100, 200);
        else if (ParseKey(c, "can_idle"))           ParseBool(c,       &can_idle);
        else if (ParseKey(c, "auto_step_delay_ms")) ParseInt(c,        &auto_step_delay_ms);
        else if (ParseKey(c, "global_theme"))       ParseTheme(c,      &global_theme);
        else *c = *c + 1;
    }

    free(data);
}

namespace settings_writer
{
    static void WriteCameraType(FILE *f, const char *key, vdbCameraType type)
    {
             if (type == VDB_CUSTOM)    fprintf(f, "%s=disabled\n", key);
        else if (type == VDB_PLANAR)    fprintf(f, "%s=planar\n", key);
        else if (type == VDB_TRACKBALL) fprintf(f, "%s=trackball\n", key);
        else if (type == VDB_TURNTABLE) fprintf(f, "%s=turntable\n", key);
        else                            fprintf(f, "%s=disabled\n", key);
    }

    static void WriteCameraUp(FILE *f, const char *key, vdbOrientation mode)
    {
        if      (mode == VDB_Z_UP)   fprintf(f, "%s=z_up\n", key);
        else if (mode == VDB_Y_UP)   fprintf(f, "%s=y_up\n", key);
        else if (mode == VDB_X_UP)   fprintf(f, "%s=x_up\n", key);
        else if (mode == VDB_Z_DOWN) fprintf(f, "%s=z_down\n", key);
        else if (mode == VDB_Y_DOWN) fprintf(f, "%s=y_down\n", key);
        else if (mode == VDB_X_DOWN) fprintf(f, "%s=x_down\n", key);
        else                         fprintf(f, "%s=z_up\n", key);
    }

    static void WriteTheme(FILE *f, const char *key, vdbTheme theme)
    {
        if      (theme == VDB_DARK_THEME)   fprintf(f, "%s=dark\n", key);
        else if (theme == VDB_BRIGHT_THEME)  fprintf(f, "%s=bright\n", key);
        else                                fprintf(f, "%s=dark\n", key);
    }

    static void WriteMat4(FILE *f, const char *key, vdbMat4 m)
    {
        fprintf(f, "%s=%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g\n",
            key,
            m(0,0), m(1,0), m(2,0), m(3,0),  // 1st column
            m(0,1), m(1,1), m(2,1), m(3,1),  // 2nd column
            m(0,2), m(1,2), m(2,2), m(3,2),  // 3rd column
            m(0,3), m(1,3), m(2,3), m(3,3)); // 4th column
    }

    static void WriteVec4(FILE *f, const char *key, vdbVec4 v)
    {
        fprintf(f, "%s=%g, %g, %g, %g\n", key, v.x, v.y, v.z, v.w);
    }
}

void settings_t::Save(const char *filename)
{
    using namespace settings_writer;
    FILE *f = fopen(filename, "wb+");
    if (!f)
    {
        fprintf(stderr, "Failed to save settings.\n");
        return;
    }
    fprintf(f, "[vdb]\n");
    fprintf(f, "window_pos=%d,%d\n", window.x, window.y);
    fprintf(f, "window_size=%d,%d\n", window.width, window.height);
    fprintf(f, "never_ask_on_exit=%d\n", never_ask_on_exit);
    fprintf(f, "show_main_menu=%d\n", show_main_menu);
    fprintf(f, "mouse_sensitivity=%g\n", camera.mouse_sensitivity);
    fprintf(f, "scroll_sensitivity=%g\n", camera.scroll_sensitivity);
    fprintf(f, "move_speed_normal=%g\n", camera.move_speed_normal);
    fprintf(f, "move_speed_slow=%g\n", camera.move_speed_slow);
    fprintf(f, "font_size=%d\n", font_size);
    fprintf(f, "dpi_scale=%d\n", dpi_scale);
    fprintf(f, "can_idle=%d\n", can_idle);
    fprintf(f, "auto_step_delay_ms=%d\n", auto_step_delay_ms);
    WriteTheme(f, "global_theme", global_theme);
    for (int i = 0; i < num_frames; i++)
    {
        frame_settings_t *frame = frames + i;
        fprintf(f, "\n[frame]=%s\n", frame->name);

        if (frame->camera.dirty)
        {
            WriteCameraType(f, "camera_type", frame->camera.type);

            if (frame->camera.planar.dirty)
            {
                fprintf(f, "planar_position=%g,%g\n", frame->camera.planar.position.x, frame->camera.planar.position.y);
                fprintf(f, "planar_zoom=%g\n", frame->camera.planar.zoom);
                fprintf(f, "planar_angle=%g\n", frame->camera.planar.angle);
                WriteCameraUp(f, "planar_up", frame->camera.planar.up);
            }

            if (frame->camera.turntable.dirty)
            {
                fprintf(f, "turntable_angle_x=%g\n", frame->camera.turntable.angle_x);
                fprintf(f, "turntable_angle_y=%g\n", frame->camera.turntable.angle_y);
                fprintf(f, "turntable_radius=%g\n", frame->camera.turntable.radius);
                WriteCameraUp(f, "turntable_up", frame->camera.turntable.up);
            }

            if (frame->camera.trackball.dirty)
            {
                WriteMat4(f, "trackball_R", frame->camera.trackball.R);
                WriteVec4(f, "trackball_T", frame->camera.trackball.T);
                fprintf(f, "trackball_zoom=%g\n", frame->camera.trackball.zoom);
                WriteCameraUp(f, "trackball_up", frame->camera.trackball.up);
            }

            if (frame->camera.projection.dirty)
            {
                fprintf(f, "y_fov=%g\n", frame->camera.projection.y_fov);
                fprintf(f, "min_depth=%g\n", frame->camera.projection.min_depth);
                fprintf(f, "max_depth=%g\n", frame->camera.projection.max_depth);
            }
        }

        if (frame->grid.dirty)
        {
            fprintf(f, "grid_visible=%d\n", frame->grid.grid_visible ? 1 : 0);
            fprintf(f, "grid_scale=%g\n", frame->grid.grid_scale);
            fprintf(f, "cube_visible=%d\n", frame->grid.cube_visible ? 1 : 0);
        }

        if (frame->render_scaler.dirty)
        {
            fprintf(f, "render_scale_down=%d\n", frame->render_scaler.down);
            fprintf(f, "render_scale_up=%d\n", frame->render_scaler.up);
        }
    }
    fclose(f);
}
