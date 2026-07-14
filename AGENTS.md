# Mandatory startup protocol

This file is intentionally written in ASCII so that its instructions remain readable before PowerShell UTF-8 handling has been initialized.

Before any inspection, inference, or modification in this repository:

1. In Windows PowerShell, initialize UTF-8 output before displaying any repository text:

   ```powershell
   [Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false)
   [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
   $OutputEncoding = [Console]::OutputEncoding
   ```

2. Never use PowerShell `Get-Content` without an explicit `-Encoding utf8` argument for repository text. For the mandatory startup documents, read raw bytes and decode them with strict UTF-8 error detection:

   ```powershell
   $Utf8Strict = [System.Text.UTF8Encoding]::new($false, $true)
   $Text = $Utf8Strict.GetString([System.IO.File]::ReadAllBytes($Path))
   ```

3. Fully read `docs/PROJECT_RULES.md`, then `tasks/README.md`, then the current task named by that index.

4. If any output is mojibake, discard it. Fix the decoding or display method and reread the original bytes before drawing conclusions or taking action. Never rewrite a file merely because the terminal displayed it incorrectly.

# Local build tool paths

Do not assume that CMake or CTest is available through `PATH` in this repository. Use the Visual Studio bundled executables explicitly:

```powershell
$CMake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$CTest = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
```

Invoke them with `& $CMake` and `& $CTest`. If either recorded path does not exist, report the environment mismatch and inspect `docs/DEVELOPMENT_SETUP.md`; do not fall back to a bare `cmake` or `ctest` command.

`docs/PROJECT_RULES.md` is the sole detailed project charter. This file is only the automatically discovered, encoding-safe entry point.
