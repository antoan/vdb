namespace hints
{
    static float view_scale;           static bool view_scale_pending;
    static bool show_grid;             static bool show_grid_pending;
    static vdbCameraType camera_type;  static bool camera_type_pending;
    static vdbOrientation orientation; static bool orientation_pending;
    static vdbKey camera_key;          static bool camera_key_pending;
}

static void ApplyHints()
{
    using namespace hints;
    if (view_scale_pending)
    {
        GetFrameSettings()->grid.grid_scale = view_scale;
        GetFrameSettings()->grid.dirty = true;
        view_scale_pending = false;
    }
    if (show_grid_pending)
    {
        GetFrameSettings()->grid.grid_visible = show_grid;
        GetFrameSettings()->grid.dirty = true;
        show_grid_pending = false;
    }
    if (camera_type_pending)
    {
        GetFrameSettings()->camera.type = camera_type;
        GetFrameSettings()->camera.dirty = true;
        camera_type_pending = false;
    }
    if (orientation_pending)
    {
        // obs! this must be applied after we apply the new camera type!
        *GetCameraUp() = orientation;
        orientation_pending = false;
    }
    if (camera_key_pending)
    {
        GetFrameSettings()->camera.key = camera_key;
        camera_key_pending = false;
    }
}

void vdbHint(vdbHintKey key, float value)
{
    if (key == VDB_VIEW_SCALE)
    {
        hints::view_scale = value;
        hints::view_scale_pending = true;
    }
}

void vdbHint(vdbHintKey key, bool value)
{
    if (key == VDB_SHOW_GRID)
    {
        hints::show_grid = value;
        hints::show_grid_pending = true;
    }
}

void vdbHint(vdbHintKey key, int value)
{
    if (key == VDB_CAMERA_TYPE &&
        (value == VDB_PLANAR ||
         value == VDB_TRACKBALL ||
         value == VDB_TURNTABLE))
    {
        hints::camera_type = value;
        hints::camera_type_pending = true;
    }
    else if (key == VDB_ORIENTATION &&
            (value == VDB_Y_UP || value == VDB_Y_DOWN ||
             value == VDB_X_UP || value == VDB_X_DOWN ||
             value == VDB_Z_UP || value == VDB_Z_DOWN))
    {
        hints::orientation = value;
        hints::orientation_pending = true;
    }
    else if (key == VDB_CAMERA_KEY &&
        (value >= 0 && value < VDB_NUM_KEYS))
    {
        hints::camera_key = value;
        hints::camera_key_pending = true;
    }
    else if (key == VDB_THEME && (value == VDB_DARK_THEME || value == VDB_BRIGHT_THEME))
    {
        settings.global_theme = value;
    }
}
