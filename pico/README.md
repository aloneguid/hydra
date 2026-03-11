Experimental, purely wireless Hydra firmware with not even a single wire (other than power or battery). Work in progress.

## Wi-Fi config

Define WIFI_SSID and WIFI_PASSWORD in `secrets.h` (.gitignore'd):

```cpp
#pragma once

#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."
```

## Todo

- app state should contain list of devices, and status.shtml should return json doc of devices instead of count.