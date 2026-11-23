/*
 * winmain.c - Entry point of the RailNet PMS desktop application
 */
#include "gui/app.h"

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev;
    (void)cmd;
    return app_run(inst, show);
}
