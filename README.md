# libhal-picosdk

This repo is a libhal wrapper for the Raspberry Pi Pico C/C++ SDK. See <https://libhal.github.io/>
for information about libhal or <https://github.com/raspberrypi/pico-sdk> for information about
the underlying library.

## 📚 Software APIs & Usage

To learn about the available drivers and APIs see the headers
[`include/libhal-picosdk`](https://github.com/libhal/libhal-picosdk/tree/main/include/libhal-picosdk)
directory.

## API Documentation

The generated API documentation is published by
[the API docs workflow](.github/workflows/api.yml) to the central libhal API
site under the `libhal-picosdk` package name:
<https://libhal.github.io/api/libhal-picosdk/main/>.

The workflow delegates to `libhal/ci` and runs the Doxygen + Sphinx build with:

```bash
conan hal docs --doc_version <version> docs
```

For a local documentation check without publishing, install Doxygen and the
Python docs requirements, then run:

```bash
cd docs
export LIBHAL_API_VERSION=local
export LIBHAL_LOCAL_BUILD=1
doxygen doxygen.conf
python -m sphinx -b html . build/html
```

The local HTML output is written to `docs/build/html/index.html`.

## 🧰 Setup

To get started with libhal, follow the
[🚀 Getting Started](https://libhal.github.io/latest/getting_started/) guide.

## 📡 Installing Profiles

Profiles define which platform you mean to build your project against. These
profiles are needed by the conan files to determine which files to build for
demos and for libraries. In order to build binaries using libhal for ARM
microcontrollers, you'll need to install both the ARM GNU toolchain and the ARM
MCU profiles.

```bash
conan config install https://github.com/libhal/conan-config2.git
```

The first command installs the ARM GNU compiler profiles, while the second adds
the ARM MCU platform profiles to your conan `profiles` directory. Note that
running these commands multiple times is safe - they will simply overwrite the
old files with the latest versions.

Now that you have the profiles installed, you can build demos and libraries for
ARM microcontrollers.

## 💾 Flashing/Programming

It is possible to flash RP-series chips via the file manager GUI, or from the
command line using picosdk's tools.

### Programming via Mass Storage Interface

When the Pico microcontrollers enter boot mode, they appear as a mass storage device
on USB. By copying a uf2 firmware file into the directory of the mass storage interface,
it is possible to flash the microcontroller with no external tools.

### Programming using picotool

By adding `picotool` as a dependency, it is possible to flash the RP chips. First, add it
as a `tool_requires` dependency in your project. Then, convert the `.elf` firmware to `.uf2`
via the `pico_add_extra_oututs()` CMake function. 
Then run `source build/rp2350-arm-s/BUILDTYPEHERE/generators/conanbuild.sh`. This will add
picotool to your path. Then run `picotool load FIRMWAREFILE.uf2` to upload.


### Using JTAG/SWD over PyOCD

`PyOCD` is a debugging interface for programming and also debugging ARM Cortex M
processor devices over JTAG and SWD.

This will require a JTAG or SWD debugger. The recommended debugger for the
RP-series chips is the Raspberry Pi Debug Adapter, although any debugger will
suffice.

See [PyOCD Installation Page](https://pyocd.io/docs/installing) for installation
details.

For reference the flashing command is:

```bash
pyocd flash --target rp2350 demos/build/lpc4078/MinSizeRel/blinker.elf.bin
```

Note that the targets for your exact part may not exist in `pyocd`. Because of
this, it means that the bounds of the memory may not fit your device. It is up
to you to make sure you do not flash a binary larger than what can fit on your
device.

## 📦 Adding `libhal-picosdk` to your project

This section assumes you are using the
[`libhal-starter`](https://github.com/libhal/libhal-starter)
project.

Make sure to add the following options and default options to your app's
`ConanFile` class:

```python
    options = {"platform": ["ANY"]}
    default_options = {"platform": "unspecified"}
```

Add the following to your `requirements()` method:

```python
    def requirements(self):
        self.requires("libhal-picosdk/[^1.0.0]")
```

The version number can be changed to whatever is appropriate for your
application. If you don't know, using the latest is usually a good choice.

The CMake from the starter project will already be ready to support the new
platform library. No change needed.

To perform a test build simple run `conan build .` as is done above with the
desired target platform profile.

## ❌ Using `libhal-picosdk` in your library

This library is a platform library and as such should only be depended upon by
applications. Platform libraries do not require ABI stability and thus do not
guarantee it. Depending on a platform library is undefined behavior if an ABI
break occurs.

## 🌟 Package Semantic Versioning Explained

In libhal, different libraries have different requirements and expectations for
how their libraries will be used and how to interpret changes in the semantic
version of a library.

If you are not familiar with [SEMVER](https://semver.org/) you can click the
link to learn more.

### 💥 Major changes

The major number will increment in the event of:

1. An API break
2. A behavior change

We define an API break as an intentional change to the public interface, found
within the `include/` directory, that removes or changes an API in such a way
that code that previously built would no longer be capable of building.

We define a "behavior change" as an intentional change to the documentation of
a public API that would change the API's behavior such that previous and later
versions of the same API would do observably different things.

The usage of the term "intentional" means that the break or behavior change was
expected and accepted for a release. If an API break occurs on accident when it
wasn't previously desired, then such a change should be rolled back and an
alternative non-API breaking solution should be found.

You can depend on the major number to provide API and behavioral
stability for your application. If you upgrade to a new major numbered version
of libhal, your code and applications may or may not continue to work as
expected or compile. Because of this, we try our best to not update the
major number.

### 🚀 Minor changes

The minor number will increment if a new interface, API, or type is introduced
into the public interface OR an ABI break has occurred. ABI breaks with
applications cause no issue and thus are allowed to be minor implementation
breaking changes.

### 🐞 Patch Changes

The patch number will increment if:

1. Bug fixes that align code to the behavior of an API, improves performance
   or improves code size efficiency.
2. Any changes occur within the `/include/libhal-arm-mcu/experimental`
   directory.

For now, you cannot expect ABI or API stability with anything in an `experimental`
namespace.

## 🔎 On Chip Software Debugging

### Using PyOCD (✅ RECOMMENDED)

In one terminal:

```bash
pyocd gdbserver --semihost -Osemihost_console_type=True --persist --target=rp2350
```

In another terminal:

```bash
arm-none-eabi-gdb demos/build/lpc4078/blinker.elf -ex "target remote :3333" -ex "set mem inaccessible-by-default off"
```

Replace `demos/build/lpc4078/blinker.elf` with the path to the elf file you'd
like to use for the debugging session.

> [!tip]
> If you get this error:
>
> ```plaintext
> Program received signal SIGTRAP, Trace/breakpoint trap.
> 0x08002840 in sys_semihost ()
> ```
>
> It means semihosting is not enabled but a semihost function has been
> executed. This usually means that the `--semihost` argument was not added to
> the `pyocd` command. To fix this, you can restart your `pyocd` session with
> the argument OR you can enter `monitor arm semihosting enable` in GDB and
> then continue debugging using the `continue` command.

### Using OpenOCD

Coming soon... (its more complicated)

## 📦 Building & Installing the Library Package

If you'd like to build and install this package into your local conan cache,
execute the following command:

```bash
conan create . -pr stm32f103c8 -pr arm-gcc-12.3 --version=latest
```

- Replace `latest` with the SEMVER version that fits the changes you've made. Or
  just choose a number greater than whats been released.
- Replace `-pr stm32f103c8` with your desired platform.
- Replace `-pr arm-gcc-12.3` with your desired compiler.
- Add `-s build_type=` to specify the build type you want to build for.

If you want to build the package unit tests without creating a package
installed within your cache, you can replace `create` with `build` and remove
the `--version` flag like so:

```bash
conan build . -pr stm32f103c8 -pr arm-gcc-12.3
```

> [!NOTE]
> Currently, we do not support `clang-tidy` checks on cross builds. So if you
> want to check the package against `clang-tidy` you will need to execute the
> command `conan build .` which will build the package for your computer which
> will allow the unit tests to be executable on your machine. The unit test
> will be executed at the end of the build process.

## :busts_in_silhouette: Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

