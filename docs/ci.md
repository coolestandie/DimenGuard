# Continuous integration

The CI definition is prepared in `.github/workflows/ci.yml`. It has **not been run on GitHub
as part of this local preparation**. A committed workflow or a successful local build is not
evidence of passing remote checks. Record actual run results after the repository is published
with authorization and GitHub Actions is enabled.

## Checks

| Job | Build and checks | What it does not establish |
| --- | --- | --- |
| Windows x64 plugin and tests | Runs `scripts/build.ps1 -BuildDirectory build/ci` against the default pinned upstream SDK, builds the plugin and runs CTest. | Compatibility with the custom fork or successful Bedrock gameplay protection. |
| Linux core and services | Builds with Clang 18 and libc++ on Ubuntu 24.04, with the Endstone adapter disabled, then runs the offline tests. | A Linux plugin artifact, Endstone ABI compatibility or server startup. |

Both jobs use fresh hosted runners and separate `build/ci` directories. Windows uses the
project's existing build script without `EndstoneSource`, so it cannot accidentally pick the
developer's custom-fork checkout. Linux deliberately sets `DIMENGUARD_BUILD_PLUGIN=OFF`;
it exercises the engine-independent domain, services, storage and presentation helpers only.
Neither job runs benchmarks as timing-based pass/fail checks.

## Verified runner prerequisites

The official [Windows Server 2022 image inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md)
lists Visual Studio's x64 C++ tools, `VC.Llvm.Clang`, `VC.Llvm.ClangToolset` and
`VC.CMake.Project` components. The Windows job verifies the required components and actual
bundled Clang/CMake/Ninja executables before invoking the build script. It checks the bundled
CMake version, since that is what the script uses, rather than relying on an unrelated CMake
installation on `PATH`.

The official [Ubuntu 24.04 image inventory](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Readme.md)
lists Clang 18.1.3 and CMake 3.31.6 at the time this definition was prepared. libc++ is not
assumed to be preinstalled: the Linux job installs the available Ubuntu packages
[`libc++-18-dev`](https://packages.ubuntu.com/noble/amd64/libdevel/libc%2B%2B-18-dev),
[`libc++abi-18-dev`](https://packages.ubuntu.com/noble/libc%2B%2Babi-18-dev) and
[`ninja-build`](https://packages.ubuntu.com/noble/ninja-build) on its disposable runner.
It logs compiler/package versions and passes `-stdlib=libc++` consistently to the C++ build.
The project's `cmake_minimum_required(VERSION 3.29)` rejects an insufficient Linux CMake.

These inventories were checked on 2026-09-08. Hosted image labels and package patch versions
can change; the workflow is not a fully frozen toolchain image. Inspect the actual tool versions
in each run when diagnosing a failure. No packages were installed on the local development
machine while preparing this definition.

## Action pin and permissions

The sole external action is `actions/checkout`, pinned to the full commit
`3d3c42e5aac5ba805825da76410c181273ba90b1`. Its
[official commit](https://github.com/actions/checkout/commit/3d3c42e5aac5ba805825da76410c181273ba90b1)
corresponds to [v7.0.1](https://github.com/actions/checkout/releases/tag/v7.0.1), verified against
the upstream release and action definition. It is not a mutable major-version tag. Before
updating it, verify the replacement release and full SHA in the official repository.

The workflow uses only `push` and ordinary `pull_request` events. It does not use
`pull_request_target`, custom credentials, deployment environments or repository secrets.
The token has only `contents: read`, and checkout uses `persist-credentials: false`, following
the [checkout permission and credential options](https://github.com/actions/checkout/blob/3d3c42e5aac5ba805825da76410c181273ba90b1/README.md).
New runs cancel superseded runs for the same branch or pull request; job timeouts bound stalled
builds. No caches are restored from another build.

There is no release packaging, artifact upload, Git push, public release creation, deployment
or server launch. Job logs and check results follow the repository's visibility settings; the
workflow itself does not make a private repository public. Dependencies are fetched during
configuration using the versions and hashes in the project's CMake definitions.

## Interpreting results

Review failures at the first failing step: tool/image checks, dependency download, compilation
and tests are different causes. CTest prints failing test output into the job log. The jobs do
not suppress failures or use `continue-on-error`.

Passing both jobs would establish those particular offline builds and tests only. Release
preparation still requires the approved license, third-party notices, exact artifact provenance
and the acceptance process in [releasing.md](releasing.md). The gameplay checklist in
[testing.md](testing.md) and limitations in [event-coverage.md](event-coverage.md) remain separate.
Do not mark the preview stable based solely on a green CI badge.
