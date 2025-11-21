# Post-build script for PlatformIO
# This script runs after the build process

Import("env")

# Example: Print build results
print("Post-build script running...")

# Get the build path
build_dir = env.subst("$BUILD_DIR")
print("Build directory: " + build_dir)

# You can perform post-build tasks here like:
# - Copy firmware to specific location
# - Generate checksums
# - Create version info file

print("Post-build script completed")