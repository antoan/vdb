#define VDB_HEADER_ONLY
#include <vdb.h>

int main(int, char**)
{
    VDBB("Test");
    {
        ShowTestWindow();
    }
    VDBE();
    return 0;
}