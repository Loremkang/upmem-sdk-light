# upmem-sdk-light
This repo is a light version of upmem sdk providing limited functionality with higher speed.

# Prerequisite:
1. Have UPMEM SDK installed first.

# To Compile:

```
mkdir build
cd build
cmake ..
make
```

# To Run:

```
mkdir build
cd build
cmake ..
make
./example
```

# Notice:
1. `third_party/upmem-sdk` is exactly the same as upmem-sdk 2023.2.0 with only one modification:
    1. File `dpu_region_address_translation.h` has its line 108 changed from `void *private;` to `void *privatedata` to pass C++ compilation. It seems that everything is alright.