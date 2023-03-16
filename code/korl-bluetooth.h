#pragma once
#include "korl-interface-platform-bluetooth.h"
korl_internal void korl_bluetooth_initialize(void);
korl_internal KORL_FUNCTION_korl_bluetooth_query(korl_bluetooth_query);
korl_internal KORL_FUNCTION_korl_bluetooth_connect(korl_bluetooth_connect);
korl_internal KORL_FUNCTION_korl_bluetooth_disconnect(korl_bluetooth_disconnect);
korl_internal KORL_FUNCTION_korl_bluetooth_read(korl_bluetooth_read);