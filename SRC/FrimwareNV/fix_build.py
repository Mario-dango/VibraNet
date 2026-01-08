import os
from SCons.Script import Import

Import("env")

# Obtenemos la ruta donde se descargó el SDK v3.4
FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-esp8266-rtos-sdk")

# Le decimos al compilador dónde buscar el archivo .ld correcto
# En el SDK v3.4 está en: components/esp8266/ld/
env.Prepend(LIBPATH=[os.path.join(FRAMEWORK_DIR, "components", "esp8266", "ld")])

print(f"--> FIX: Agregada ruta de linker: {os.path.join(FRAMEWORK_DIR, 'components', 'esp8266', 'ld')}")