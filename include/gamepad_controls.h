#pragma once
#include <wups/config/WUPSConfigCategory.h>

void gamepad_application_start();
void gamepad_application_end();
void gamepad_config_opened(WUPSConfigCategory &root);
void gamepad_config_closed();
