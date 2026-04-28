#!/usr/bin/python
#
# Copyright 2024 - 2025 Khalil Estell and the libhal contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain
from conan.tools.env import VirtualBuildEnv
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy
from pathlib import Path

required_conan_version = ">=2.0.14"


class libhal_picosdk_conan(ConanFile):
    name = "libhal-picosdk"
    license = "Apache-2.0"
    homepage = "https://github.com/libhal/libhal-picosdk"
    description = (
        "Drivers that adapt Raspberry Pi Pico C/C++ SDK to libhal."
    )
    topics = (
        "arm",
        "cortex",
        "cortex-m",
        "cortex-m0",
        "cortex-m33",
        "rp2040",
        "rp2350",
    )
    settings = "compiler", "build_type", "os", "arch"

    python_requires = "libhal-bootstrap/[>=4.3.0 <5]"
    python_requires_extend = "libhal-bootstrap.library"

    options = {
        "platform": ["ANY"],
        "use_libhal_exceptions": [True, False],
        "use_picolibc": [True, False],
        "variant": [None, "ANY"],
        "board": [None, "ANY"],
        "replace_std_terminate": [True, False],
        "use_semihosting": [True, False],
        "flash_size": ["ANY"],
        "flash_clkdiv": ["ANY"],
        "rp_revision": ["ANY"],
        "use_w25q_flash": [True, False],
    }
    default_options = {
        "platform": "ANY",
        "board": None,
        "use_libhal_exceptions": False,
        "use_picolibc": True,
        "replace_std_terminate": True,
        "use_semihosting": True,
        "variant": None,
        "use_w25q_flash": False,
    }

    options_description = {
        "platform": "Specifies which platform to provide binaries and build information for",
        "use_libhal_exceptions": "Reserved for backwards compatibility. This option is currently unused and will become functional when libhal-exceptions is feature complete.",
        "use_picolibc": "Use picolibc as the libc runtime for ARM GCC. Note: ARM's LLVM fork always uses picolibc and ignores this option.",
        "replace_std_terminate": "Replace the default std::terminate handler to reduce binary size by avoiding verbose text rendering",
        "use_semihosting": "Enables semihosting support, allowing the MCU to perform host based I/O like writing to stdout or reading from files via the debug port. With LLVM from arm-toolchain, semihosting is enabled via the compiler and must be disabled via a build profile option and not this option.",
    }

    def set_version(self):
        # Use latest if not specified via command line
        if not self.version:
            self.version = "latest"

    def requirements(self):
        self.requires("libhal/[^4.18.0]", transitive_headers=True)
        self.requires("libhal-util/[^5.8.1]", transitive_headers=True)
        self.requires("ring-span-lite/[^0.7.0]", transitive_headers=True)
        self.requires("scope-lite/0.2.0")

        if (
            self.options.use_picolibc
            and self.settings.os == "baremetal"
            and self.settings.compiler == "gcc"
        ):
            CV = str(self.settings.compiler.version)

            CRT0 = "semihost" if self.options.use_semihosting else "default"
            OSLIB = "semihost" if self.options.use_semihosting else None
            self.requires(
                "prebuilt-picolibc/" + CV, options={"crt0": CRT0, "oslib": OSLIB}
            )

        self.requires("picosdk/2.2.1-alpha")
        self.tool_requires("pioasm/2.2.0")


    def _macro(self, string):
        return string.upper().replace("-", "_")

    def generate(self):
        virt = VirtualBuildEnv(self)
        virt.generate()
        tc = CMakeToolchain(self)
        tc.cache_variables["DO_NOT_BUILD_BOOT_HAL"] = True
        tc.preprocessor_definitions["PICO_STDIO_SHORT_CIRCUIT_CLIB_FUNCS"] = "0"
        tc.cache_variables["PICO_BOARD"] = self.getboard()
        if (
            self.options.flash_size
            or self.options.flash_clkdiv
            or self.options.rp_revision
        ):
            tc.cache_variables["PICO_BOARD_HEADER_DIRS"] = str(
                self.build_folder
            )
            self.generate_rp_header()
        if self.options.variant:
            tc.preprocessor_definitions[
                "LIBHAL_VARIANT_" + self._macro(str(self.options.variant))
            ] = "1"
        tc.preprocessor_definitions[
            "LIBHAL_PLATFORM_" + self._macro(str(self.options.platform))
        ] = "1"
        tc.generate()
        cmake = CMakeDeps(self)
        cmake.generate()

    def getboard(self):
        if not self.options.board:
            return "libhal_picosdk"
        return self.options.board.value

    def validate(self):
        if (
            self.options.flash_size
            or self.options.flash_clkdiv
            or self.options.rp_revision
        ):
            if not self.options.flash_size:
                raise ConanInvalidConfiguration("Flash size must be set")
            if not str(self.options.flash_clkdiv).isnumeric():
                raise ConanInvalidConfiguration(
                    "Flash clock divider is invalid value"
                )
            if self.options.rp_revision.value not in ["a1", "a2"]:
                raise ConanInvalidConfiguration("RP revision is invalid")
        if "rp2350" in str(self.options.platform):
            if not self.options.variant:
                raise ConanInvalidConfiguration("RP2350 variant not specified")
            if self.options.variant not in ["rp2350a", "rp2350b"]:
                raise ConanInvalidConfiguration("Invalid RP2350 variant specified")
        super().validate()

    def package(self):
        if (
            self.options.flash_size
            or self.options.flash_clkdiv
            or self.options.rp_revision
        ):
            copy(
                self,
                f"{self.getboard()}.h",
                dst=Path(self.package_folder).joinpath("include", "picosdk-board-defs"),
                src=self.build_folder,
            )
        super().package()

    def package_info(self):
        self.cpp_info.libs = ["libhal-picosdk"]
        self.cpp_info.set_property("cmake_target_name", "libhal::picosdk")
        self.cpp_info.set_property(
            "cmake_target_aliases",
            ["libhal::rp2350"],
        )

        PLATFORM = str(self.options.platform)
        self.buildenv_info.define("LIBHAL_PLATFORM", PLATFORM)
        self.buildenv_info.define("LIBHAL_PLATFORM_LIBRARY", "picosdk")
        if str(self.options.platform).startswith("rp2"):
            if self.options.flash_size:
                self.buildenv_info.define(
                    "PICO_BOARD_HEADER_DIRS",
                    str(Path(self.package_folder, "include", "picosdk-board-defs")),
                )
            defines = []
            if self.options.variant:
                defines.append(
                    "LIBHAL_VARIANT_" + self._macro(str(self.options.variant)) + "=1"
                )
            defines.append(
                "LIBHAL_PLATFORM_" + self._macro(str(self.options.platform)) + "=1"
            )
            defines.append("PICO_STDIO_SHORT_CIRCUIT_CLIB_FUNCS=0")
            self.cpp_info.defines = defines

        self.cpp_info.exelinkflags = []
        if self.settings.os == "baremetal":
            self.setup_baremetal(PLATFORM)

    def package_id(self):
        self.info.python_requires.major_mode()
        self.info.options.clear()

    def setup_baremetal(self, platform: str):
        if self.options.replace_std_terminate:
            self.cpp_info.exelinkflags.extend(
                [
                    # Override picolibc's default hard fault handler to gracefully
                    # handle semihosting BKPT instructions when no debugger is
                    # attached. Without this, binaries linked with semihosting
                    # libraries will hang in an infinite loop if executed without a
                    # debugger. This wrapper detects BKPT-induced faults, skips the
                    # instruction, and allows execution to continue, enabling test
                    # packages to link successfully while allowing applications to
                    # run standalone.
                    "-Wl,--wrap=arm_hardfault_isr",
                    # Override the default standard set and get terminate functions
                    # to prevent linking in the original default verbose terminate
                    # implementation.
                    "-Wl,--wrap=_ZSt13set_terminatePFvvE",
                    "-Wl,--wrap=_ZSt13get_terminatev",
                ]
            )

        if self.options.replace_std_terminate:
            if self.settings.compiler == "clang":
                self.cpp_info.exelinkflags.extend(
                    [
                        # Overrides the terminate handler from LLVM
                        # This results in a large reduction in binary size since this
                        # terminate handler renders text and that text rendering is
                        # expensive.
                        "-Wl,--wrap=__cxa_terminate_handler",
                    ]
                )
            if self.settings.compiler == "gcc":
                self.cpp_info.exelinkflags.extend(
                    [
                        # Override the terminate handler for GCC.
                        # This results in a large reduction in binary size since this
                        # terminate handler renders text and that text rendering is
                        # expensive.
                        "-Wl,--wrap=_ZN10__cxxabiv119__terminate_handlerE",
                    ]
                )

        package_folder = Path(self.package_folder)
        LIB_PATH = package_folder / "lib" / "liblibhal-picosdk.a"
        self.cpp_info.exelinkflags.extend(
            [
                # Ensure that all symbols are added to the linker's symbol table
                # This is critical in order for the wrapped symbols to make it to
                # the final link binary with --gc-sections enabled.
                # NOTE: gc sections still works as expected, it just doesn't miss
                # any symbols from this archive.
                "-Wl,--whole-archive",
                str(LIB_PATH),
                "-Wl,--no-whole-archive",
            ]
        )

    def generate_rp_header(self):
        platform = str(self.options.platform.value)
        pico_board = self.getboard()
        a2 = "1" if self.options.rp_revision.value == "a2" else "0"
        if platform.startswith("rp235"):
            if self.options.variant == "rp2350a":
                r2350a = "1"
            else:
                r2350a = "0"
        file = f"""#ifndef _{pico_board}_h
                #define _{pico_board}_h
                pico_board_cmake_set(PICO_PLATFORM, {platform})
                #define PICO_RP2350A {r2350a}
                #define PICO_BOOT_STAGE2_CHOOSE_W25Q080 {"1" if self.options.use_w25q_flash else "0"}

                #ifndef PICO_FLASH_SPI_CLKDIV
                #define PICO_FLASH_SPI_CLKDIV {self.options.flash_clkdiv}
                #endif

                pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, {self.options.flash_size})
                #ifndef PICO_FLASH_SIZE_BYTES
                #define PICO_FLASH_SIZE_BYTES {self.options.flash_size}
                #endif

                pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, {a2})
                #ifndef PICO_RP2350_A2_SUPPORTED
                #define PICO_RP2350_A2_SUPPORTED {"1" if a2 else "0"}
                #endif

                #endif"""
        with open(Path(self.build_folder).joinpath(f"{pico_board}.h"), "w") as f:
            f.write(file)
