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

# Three-attempt stop and user escalation protocol

This rule applies to every operation, including network access, permission-dependent actions, tool invocation, build, test, deployment, inspection, and external application control.

1. After the same operation or the same blocking condition has failed three times, stop the current task immediately. The original attempt counts as the first attempt; therefore, make at most three total attempts, not three retries after the first failure.
2. Do not continue with more attempts, silently wait and retry, or switch to a lower-quality, less reliable, less complete, or otherwise inferior fallback approach.
3. Tell the user what failed, the relevant error or blocking condition, what was attempted in each of the three attempts, and exactly what user action, permission, credential, network change, screenshot, or other input is needed to resume with the best available approach.
4. Resume only after the user supplies the requested help or explicitly gives new instructions.
5. A materially different failure starts a new three-attempt count only when it is genuinely independent of the original blocking condition. Renaming or slightly varying the same unsuccessful action does not reset the count.

# Component build output and manual installation handoff

Whenever a FooCrate component build is prepared for user inspection or manual testing:

1. Codex must build the appropriate Release component and generate the installable `.fb2k-component` file. This is a component build output for manual import, not the formal release-packaging work tracked by the release task.
2. Codex must place the finished component file in the repository `dist` directory and report its exact path to the user.
3. Codex must verify that the component file exists and contains the expected FooCrate component DLL before handing it off. Do not create unrelated release bundles, installers, or deployment artifacts.
4. The user will import or install the component file manually. Do not install, load, copy, or deploy it into any foobar2000 instance unless the user explicitly requests that separate action.

# Prerelease and final component versioning

Use a new prerelease version for every component handed to the user for testing. Never use the previous stable version number for a new test binary, and never overwrite an existing stable component package with test content.

1. Reserve the next intended release version before building the first candidate. Name candidates with SemVer prerelease identifiers such as `1.0.3-beta.1`, then `1.0.3-beta.2`. The package filename and the FooCrate product/component version shown to the user must carry the same prerelease identifier. A numeric Windows `FileVersion` may map `1.0.3-beta.1` to `1.0.3.0`, but the visible `ProductVersion` must retain the full prerelease string.
2. Every changed or rebuilt candidate receives the next prerelease number. Do not replace or silently reuse an earlier `beta.N` package, even when the intended fix is small. Existing stable and prerelease packages are immutable evidence of the exact binary that was tested.
3. A prerelease candidate requires the normal applicable Debug and Release builds, automated tests, and the minimum package-structure verification required by the component handoff rules. A prerelease checksum file and user-facing SHA-256 are not required unless the user explicitly asks for them.
4. After the user explicitly accepts a specific prerelease candidate, the final build may use the fast finalization path only when the source is unchanged except for removing the prerelease version label and updating directly related release metadata. In that path, build the Release component, verify that the package exists and contains only the expected FooCrate DLL, then compute and report the final package SHA-256. Do not repeat Debug builds, the full automated test suite, or unrelated release audits.
5. If any functional source, dependency, build option, resource, or non-version behavior changes after prerelease acceptance, the fast finalization path is invalid. Produce the next `beta.N` candidate and obtain user acceptance again.
6. Once a final version has been produced or published, never replace it with different bits. Any later correction starts the next patch version and its own prerelease sequence.

# Screenshot checkpoint protocol

At any visual or UX checkpoint, make at most one screenshot attempt and inspect that result before doing more work.

If the requested FooCrate or foobar2000 target is obscured, absent, incomplete, or replaced by unrelated foreground content:

1. Discard that screenshot as invalid evidence.
2. Stop the current work immediately. Do not retry with another capture method and do not continue reasoning about the hidden UI.
3. Do not activate, minimize, move, resize, or otherwise disturb the foreground application in order to obtain another screenshot.
4. Tell the user exactly which window, state, menu, hover, or interaction screenshot is needed.
5. Resume only after the user supplies the requested screenshot or explicitly gives new instructions.

`docs/PROJECT_RULES.md` is the sole detailed project charter. This file is only the automatically discovered, encoding-safe entry point.
