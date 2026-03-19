#include "app_controller.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    potato::AppController app;
    return app.Run(instance, showCommand);
}
