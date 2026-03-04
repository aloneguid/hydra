Experimental, purely wireless Hydra firmware with not even a single wire (other than power or battery). Work in progress.

## Wi-Fi config

Use env vars. For VS Code, edit `.vscode/cmake-kits.json`:

```json
[
    {
        "name": "Pico",
        "compilers": {
            "C": "${command:raspberry-pi-pico.getCompilerPath}",
            "CXX": "${command:raspberry-pi-pico.getCxxCompilerPath}"
        },
        "environmentVariables": {
            "PATH": "${command:raspberry-pi-pico.getEnvPath};${env:PATH}",
            "PICO_WIFI_SSID": "your ssid here",
            "PICO_WIFI_PASSWORD": "your password here"
        },
        "cmakeSettings": {
            "Python3_EXECUTABLE": "${command:raspberry-pi-pico.getPythonPath}"
        }
    }
]
```