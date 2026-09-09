# Third-party notices

This document accompanies the Windows x64 DimenGuard plugin DLL. It preserves notices for
the bundled SQLite implementation and the Endstone API's header-only dependencies used by
the pinned build. DimenGuard's original code uses the [MIT License](LICENSE); the dependencies
listed here retain their own licenses. Include both documents when distributing the plugin.

## Dependency inventory

| Component | Verified version or revision | Integration | License |
| --- | --- | --- | --- |
| Endstone API | 0.12.0, upstream commit `46eff9f125f52eac76472d84339ead8fbf51fcd2` | Public API headers and inline/template code. | Apache License 2.0. |
| JSON for Modern C++ (nlohmann/json) | 3.12.0 | Header-only dependency of the Endstone API. | MIT; additional header attributions retained below. |
| expected-lite | 0.9.0 | Header-only `nonstd::expected` used by Endstone's `Result`. | Boost Software License 1.0. |
| SQLite | 3.50.4, amalgamation `3500400` | Statically linked through `dimenguard_sqlite`. | Public domain; upstream copyright disclaimer retained below. |

The project CMake target `dimenguard` links `dimenguard_services`, which links
`dimenguard_core` and `dimenguard_sqlite`. Endstone's `endstone::endstone` interface target
supplies `nlohmann_json::nlohmann_json` and `nonstd::expected-lite`. The inspected Windows
DLL link rule contains these project libraries and Windows system libraries, not GoogleTest.

GoogleTest 1.17.0 is used only by the separate offline test executables. Neither those
executables nor GoogleTest are shipped in the plugin ZIP, so its license is not presented as
a bundled DLL dependency here. CMake, Ninja, Git, compiler executables, development SDK files,
the Endstone server and the Bedrock server are not included in that ZIP. Windows and C/C++
runtime DLLs are supplied separately by the platform/runtime installation, not copied by the
packaging script. Reassess notices before distributing test tools, SDKs or runtime libraries.

For a custom-fork build, the exact SDK revision is recorded in `release.json`. Review that
checkout's changed files and dependency notices before distribution; this inventory does not
claim that every possible fork has the same dependencies or binary compatibility.

## Endstone API

Source: [EndstoneMC/endstone](https://github.com/EndstoneMC/endstone).
The full text below is reproduced from `LICENSE` at the pinned upstream revision.
The inspected SDK contains no separate `NOTICE` file.

Public API headers carry the following copyright notices:

```text
Copyright (c) 2023, The Endstone Project. (https://endstone.dev) All Rights Reserved.
Copyright (c) 2024, The Endstone Project. (https://endstone.dev) All Rights Reserved.
```

```text
                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright (c) 2024, The Endstone Project. (https://endstone.dev) All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```

## JSON for Modern C++ (nlohmann/json)

Source: [nlohmann/json](https://github.com/nlohmann/json/tree/v3.12.0).
The full text below is reproduced from the release archive's `LICENSE.MIT`.

```text
MIT License

Copyright (c) 2013-2025 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Additional MIT-licensed header notices retained from the same 3.12.0 release:

- `include/nlohmann/detail/conversions/to_chars.hpp` implements a modified Grisu2 reference
  implementation and carries the Florian Loitsch attribution.
- `include/nlohmann/detail/output/serializer.hpp` carries the Björn Hoehrmann attribution.
- `include/nlohmann/thirdparty/hedley/hedley.hpp` carries the Evan Nemerson attribution.

```text
SPDX-FileCopyrightText: 2009 Florian Loitsch <https://florian.loitsch.com/>
SPDX-FileCopyrightText: 2008 - 2009 Björn Hoehrmann <bjoern@hoehrmann.de>
SPDX-FileCopyrightText: 2016 - 2021 Evan Nemerson <evan@nemerson.com>
SPDX-License-Identifier: MIT
```

The supplied `include/nlohmann/detail/meta/cpp_future.hpp` also retains this attribution:

```text
SPDX-FileCopyrightText: 2018 The Abseil Authors
```

That header identifies its C++11 integer-sequence fallback as derived from Google Abseil's
`absl/utility/utility.h` at commit `10cb35e459f5ecca5b2ff107635da0bfa41011b4`, under the
Apache License 2.0 reproduced above. The fallback is excluded when `JSON_HAS_CPP_14` is
defined; the project's C++20 build uses the standard-library implementation instead. Abseil
is not separately linked or bundled as a library.

## expected-lite

Source: [martinmoene/expected-lite](https://github.com/martinmoene/expected-lite/tree/v0.9.0).
The copyright and origin notices below are retained from `include/nonstd/expected.hpp`;
the complete license is reproduced from `LICENSE.txt`.

```text
Copyright (C) 2016-2025 Martin Moene.

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

expected lite is based on:
  A proposal to add a utility class to represent expected monad
  by Vicente J. Botet Escriba and Pierre Talbot. http:://wg21.link/p0323
```

```text
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

## SQLite

Source: the official SQLite `sqlite-amalgamation-3500400.zip` release.
The local `sqlite3.h` identifies version `3.50.4` and source ID:

```text
2025-07-30 19:33:53 4d8adfb30e03f9cf27f800a2c1ba3c48fb4ca1b08b0f5ed59a4d5ecbf45e20a3
```

The following complete copyright disclaimer and blessing are retained from `sqlite3.h`
and the corresponding amalgamated source:

```text
2001-09-15

The author disclaims copyright to this source code.  In place of
a legal notice, here is a blessing:

   May you do good and not evil.
   May you find forgiveness for yourself and forgive others.
   May you share freely, never taking more than you give.
```

## Maintaining this inventory

The inspected sources are the pinned Endstone SDK's `LICENSE` and `include/CMakeLists.txt`,
nlohmann/json's `LICENSE.MIT` and annotated headers, expected-lite's `LICENSE.txt` and
`expected.hpp`, and SQLite's `sqlite3.c`/`sqlite3.h`. The generated Windows link rules
were checked to separate shipped dependencies from test-only dependencies.

When changing the SDK, dependency versions, compiler runtime strategy or package contents,
review the actual resolved include paths and link inputs again, update this document and keep
the complete applicable license texts with the package. A `find_package` result or source
override can select dependencies other than the fallback pins. Do not assume this inventory
covers such a substituted build without checking it.
