# Configuración del entorno de desarrollo — Windows

Guía paso a paso para dejar Antigravity listo para compilar MoodEngine usando el toolchain de Visual Studio 2026 sin salir del editor.

---

## 1. Verificar que Visual Studio 2026 está instalado correctamente

Abrí el menú de inicio y buscá **"Developer Command Prompt for VS 2026"**. Si aparece, el toolchain está instalado.

Abrilo y corré estos comandos uno por uno:

```
cl
cmake --version
git --version
```

**Salida esperada:**

- `cl` → algo tipo `Microsoft (R) C/C++ Optimizing Compiler Version 19.4x...`
- `cmake --version` → `3.24` o superior
- `git --version` → cualquier versión reciente

Si alguno falla, volvé al instalador de Visual Studio y verificá que esté marcado el workload **"Desarrollo para el escritorio con C++"** con los componentes:

- Herramientas de compilación de C++
- Herramientas de CMake en C++ para Windows
- SDK de Windows 11

---

## 2. Averiguar el nombre del generador de CMake para VS 2026

Desde el Developer Command Prompt, corré:

```
cmake --help
```

Al final aparece una lista de generadores. Buscá la línea parecida a:

```
* Visual Studio 18 2026 [arch] = Generates Visual Studio 2026 project files.
```

**Copiá el texto exacto** (por ejemplo `Visual Studio 18 2026`). Si es diferente de `Visual Studio 17 2022`, hay que actualizar `CMakePresets.json` en la raíz del proyecto reemplazando las dos ocurrencias de `"Visual Studio 17 2022"` por el nombre nuevo.

---

## 3. Configurar Antigravity para usar el Developer Prompt como terminal integrado

Esto es lo mágico: cada vez que abras una terminal dentro de Antigravity, las variables del compilador MSVC ya estarán cargadas — podrás correr `cl`, `cmake`, `ninja` directamente sin abrir ventanas separadas.

### Paso A — Localizar la ruta de instalación de Visual Studio

En el Developer Command Prompt corré:

```
echo %VSINSTALLDIR%
```

Esto imprime algo como:

```
C:\Program Files\Microsoft Visual Studio\2026\Community\
```

**Guardá esa ruta**, la vamos a usar en el siguiente paso. Reemplaza `Community` por `Professional` o `Enterprise` si instalaste una edición distinta.

### Paso B — Editar la configuración de Antigravity

1. Abrí Antigravity.
2. Abrí la paleta de comandos con `Ctrl+Shift+P`.
3. Escribí **"Preferences: Open User Settings (JSON)"** y dale Enter. Se abre `settings.json`.
4. Pegá este bloque dentro del objeto JSON (si el archivo ya tiene contenido, agregalo como un campo más; cuidá las comas):

```json
"terminal.integrated.profiles.windows": {
    "VS 2026 Developer": {
        "source": "PowerShell",
        "args": [
            "-NoExit",
            "-Command",
            "& { Import-Module 'C:\\Program Files\\Microsoft Visual Studio\\2026\\Community\\Common7\\Tools\\Microsoft.VisualStudio.DevShell.dll'; Enter-VsDevShell -VsInstallPath 'C:\\Program Files\\Microsoft Visual Studio\\2026\\Community' -SkipAutomaticLocation -DevCmdArguments '-arch=x64' }"
        ],
        "icon": "tools"
    }
},
"terminal.integrated.defaultProfile.windows": "VS 2026 Developer"
```

> Si la ruta que te dio `echo %VSINSTALLDIR%` en el paso A es distinta, reemplazá las dos ocurrencias de `C:\\Program Files\\Microsoft Visual Studio\\2026\\Community` por la tuya. **Usá doble barra invertida `\\` en JSON**, no barra simple.

5. Guardá el archivo (`Ctrl+S`).

### Paso C — Probar que funciona

1. En Antigravity abrí una terminal nueva con ``Ctrl+Ñ`` (o `Ctrl+` ` según teclado) o **Terminal > New Terminal**.
2. La terminal debería abrirse con un mensaje tipo `** Visual Studio 2026 Developer PowerShell v18.x.x **`.
3. Corré:

```
cl
cmake --version
```

Ambos deben responder sin errores.

Si salió bien, **ya no vas a necesitar salir de Antigravity** para compilar. Todas las terminales que abras ahí cargan automáticamente el entorno MSVC.

---

## 4. Primera compilación de MoodEngine

Desde una terminal integrada en Antigravity, parado en la raíz del repo:

```
cmake --preset windows-msvc-debug
```

Esto descarga SDL2, ImGui, spdlog y GLM la primera vez (puede tardar 2-5 minutos) y genera la solución de Visual Studio bajo `build/debug/`.

Luego:

```
cmake --build build/debug --config Debug
```

Y para ejecutar:

```
./build/debug/Debug/MoodEditor.exe
```

---

## 5. Problemas comunes

**"cl no se reconoce como comando":** la terminal no cargó el entorno MSVC. Cerrala y abrí una nueva verificando que diga "Visual Studio 2026 Developer PowerShell" arriba.

**"CMake Error: Could not find generator":** el nombre del generador en `CMakePresets.json` no coincide con lo que tenés instalado. Volvé al paso 2.

**La descarga de dependencias falla:** verificá tu conexión y que Git esté en PATH (`git --version`).

**Antivirus bloquea la compilación:** agregá la carpeta `build/` y la del repo a las exclusiones de Windows Defender. CMake escribe muchos archivos temporales que pueden ser escaneados innecesariamente.

---

## 6. Atajos útiles dentro de Antigravity

- `Ctrl+Shift+P` → paleta de comandos (todo pasa por aquí)
- `Ctrl+Ñ` o ``Ctrl+` `` → abrir/ocultar terminal integrada
- `Ctrl+P` → buscar archivo por nombre
- `Ctrl+Shift+F` → buscar en todo el repo
- `F5` → debug (requiere configurar `launch.json`, se hará en Hito 2 si hace falta)
