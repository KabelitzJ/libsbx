# Common shader functions for libsbx

## Overview

libsbx is a collection of shader functions designed to be used in various graphics applications.
These functions are written in GLSL and provide a wide range of utilities for graphics programming.

## Usage

To use the functions in this library, include the appropriate header files in your shader code.
For example, to use the `libsbx/common/constants.glsl` file, you would include it like this:

```glsl
#version 460 core

#include <libsbx/common/constants.glsl>

...

main() {
  float result = PI * 2.0;
}
```

## Note

This collection of functions should and will be moved to a more appropriate location in the future.
The current organization is temporary and may change as the library evolves.
