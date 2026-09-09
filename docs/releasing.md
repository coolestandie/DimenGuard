# Preparing a release candidate

DimenGuard currently targets a public **preview**, not gameplay-verified stable protection.
The packaging script only assembles a local candidate. It does not publish a repository or
release, select a license, deploy a DLL, modify plugin data or start a server.

## Acceptance gate

- Commit the intended source and documentation using the project identity. Tracked changes
  must be clean; ignored local planning and agent notes do not affect packaging.
- Build from that checkout in a separate release build directory and pass the offline tests.
  Record the compiler, configuration, exact plugin commit and Endstone SDK commit.
- Review [event coverage](event-coverage.md) and retain every unsupported or partially supported
  path in the public documentation. Offline checks do not establish Bedrock event coverage.
- Run the optional offline benchmark if comparing scale behavior. Keep the measured results
  and machine details, without presenting them as a server tick guarantee.
- Obtain the author's explicit license choice and provide its complete, approved text. Review
  any applicable third-party redistribution requirements before distributing the binary. The
  script does not invent a license or determine whether its contents are legally sufficient.
- Check [third-party notices](../THIRD_PARTY_NOTICES.md) against the actual SDK and dependency
  sources resolved for this build. Preserve the complete applicable texts; update the inventory
  when changing dependencies, using a source override or distributing additional tools.
- Prepare a candidate with the script below. Packaging success is not an acceptance result.
- Perform and record the checklist in [testing.md](testing.md) on the intended runtime with
  the exact candidate DLL. Native parsing or startup success alone does not pass this gate.

Gameplay/client acceptance for the current implementation remains **pending**. A stable release
requires recorded results for permissions, owners/members/outsiders, boundaries, overlapping
regions, dimensions, persistence/recovery, actual world and inventory state, client completion,
and relevant interactions with other plugins. Unsupported paths remain documented limitations,
even after those tests pass. Do not relabel an untested candidate as stable.

## Keep SDK variants separate

| Package variant | Required build | Compatibility meaning |
| --- | --- | --- |
| `UpstreamPinned` | A fresh build with the default pinned SDK and no `EndstoneSource` override. | Targets the unmodified upstream headers at the CMake pin; runtime acceptance is still required. |
| `CustomFork` | A separate build using an explicit `EndstoneSource` checkout. | Targets that fork's headers and ABI only; never substitute it for the upstream DLL. |

The local chunk-development fork changes public virtual interfaces. Its DLL and an upstream
DLL are not interchangeable merely because both report Endstone API `0.12`. Do not mix those
artifacts in the same build directory or distribute the custom-fork build as a generic upstream
release. Exported SDK archives supplied through `EndstoneSource` are deliberately not accepted
as `UpstreamPinned` packages: use a fresh default SDK build for the public candidate.

The build script refuses to reuse a configured directory when its cached SDK selection differs
from the request. Omitting `EndstoneSource` means the pinned upstream SDK; it does not mean
"reuse whichever SDK is cached." Switching from a fork to upstream, upstream to a fork or one
SDK path to another requires a separate fresh directory. Nothing is deleted or reset. The
same source-selection guard applies to core-only builds, although they do not build the adapter.
Relative `BuildDirectory` and `EndstoneSource` paths are resolved from the project root.

`EndstoneRevision` is the full 40-character commit actually used to build the DLL, not a version
label or an arbitrary current checkout revision. `CompatibilityLabel` is an explicit lowercase
filename-safe label for the intended runtime, such as `endstone-0-12-bds-1-26-45-1`.
It identifies a claimed target; it does not claim that gameplay tests have passed.

The script checks the CMake source directory, plugin configuration, release build type, generated
version against the current project version, SDK variant and upstream pin. It validates the
Windows x64 DLL header without loading the plugin. It cannot prove that a previously built DLL
was compiled from today's clean checkout or that SDK files were unmodified: the recorded SDK
revision is a **caller attestation**.
Rebuild from a clean, verified SDK immediately before packaging. Do not reuse a stale DLL.

## Local packaging

First choose and commit the license, third-party notices and public documentation. No license
has been selected by this guide. `-LicenseFile` is mandatory and the file must already exist and
contain the approved text; it is copied as `LICENSE` without changing its contents.

Build the public candidate in a new directory under the ignored `build/` tree:

```powershell
./scripts/build.ps1 -BuildDirectory build/public
```

Read the current full SDK pin from `CMakeLists.txt`. Replace the placeholders below with that
revision, the actual target label and the approved license path. The example revision placeholder
is intentionally not a usable commit:

```powershell
$release = @{
    BuildDirectory = 'build/public'
    EndstoneRevision = '<full-40-character-SDK-commit>'
    CompatibilityLabel = 'endstone-0-12-bds-1-26-45-1'
    SdkVariant = 'UpstreamPinned'
    LicenseFile = 'LICENSE'
    Candidate = 'rc1'
}
./scripts/package.ps1 @release -ValidateOnly
./scripts/package.ps1 @release
```

`-ValidateOnly` runs structural/input validation without creating `dist`, copying files, writing
an archive or loading the DLL. It still requires the built DLL, approved license, clean tracked
source and committed release documentation, including third-party notices and the benchmark
guide. It can be used as a read-only preflight after a build. The script does not run tests or
create evidence of gameplay acceptance.

For a fork-specific candidate, build into a different directory with `-EndstoneSource`, then
provide `-SdkVariant CustomFork`, that checkout's exact commit and an unmistakable fork-specific
compatibility label. Keep the upstream and fork candidates separate throughout testing.
For example, use `-BuildDirectory build/fork -EndstoneSource 'C:\path\to\endstone-fork'`;
repeat the same SDK option on subsequent builds of that directory. If `build/public` or
`build/fork` already has a different cached source, choose a new child directory instead of
clearing or repurposing its cache.

The output is a ZIP and a matching `.zip.sha256` file under the repository's `dist` directory.
Names include the generated project version, candidate number, platform, SDK variant, target
label and short plugin commit. Existing output is never overwritten; use the next candidate
number after rebuilding. Packaging refuses tracked staged or unstaged changes, including
documentation changes. Untracked or ignored files are not evidence of a reproducible build;
use a clean release checkout and do not compile uncommitted source into the candidate.

## Package contents and checksums

Only the explicit release allowlist is copied:

```text
endstone_dimenguard.dll
LICENSE
THIRD_PARTY_NOTICES.md
README.md
docs/*.md                  (tracked Markdown files, including subdirectories)
benchmarks/README.md
release.json
SHA256SUMS
```

`release.json` records the generated version, candidate, plugin and SDK commits, target label,
build configuration, SDK variant, UTC packaging time and pending gameplay acceptance. It does
not embed local build paths. `SHA256SUMS` covers each payload file except itself; the adjacent
`.zip.sha256` covers the complete archive, including that inner manifest. SHA256 detects a
mismatch but is not a publisher signature or proof of trusted provenance. The third-party
notices and benchmark guide are included and checksummed, keeping the corresponding README
links usable inside the extracted package. Benchmark/test executables are not included.

No region database, journals, configuration, world, logs, server files, build dependencies or
local planning notes are included. Files are assembled in a uniquely created staging directory
under `dist`; cleanup validates that exact owned child path before removing it. Existing release
artifacts are preserved if a later step fails. Check the reported error before retrying.

## Installation, upgrades and recovery

1. Confirm that the package's exact SDK variant and intended runtime match the target server.
   Verify its archive SHA256 before extracting it to a temporary folder, not over server data.
2. Stop the server normally. Back up the current DLL and the complete DimenGuard data folder
   while it is stopped, and preserve a restorable world backup for acceptance testing.
3. Replace only the plugin DLL with the candidate. Do not overwrite the database or other
   plugin data with an empty template. The release package intentionally contains none.
4. Start the server only as an explicit deployment step, inspect startup and storage logs, and
   perform the recorded acceptance checks. Keep the backup until verification is complete.
5. If rollback is needed, stop the server and restore a matching DLL/data backup. Never assume
   an older plugin can read a newer schema. Keep the failed candidate and logs for diagnosis.

These are operator instructions, not actions performed by the packaging script. Publishing a
public repository or uploading release assets requires a separate explicit request.
