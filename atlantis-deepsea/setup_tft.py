Import("env")
import os, shutil

def copy_user_setup(*args, **kwargs):
    src = os.path.join(env["PROJECT_DIR"], "include", "User_Setup.h")
    dst_dir = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "TFT_eSPI")
    dst = os.path.join(dst_dir, "User_Setup.h")

    if not os.path.isfile(src):
        print(f">> setup_tft.py: ERROR - source not found: {src}")
        return

    if not os.path.isdir(dst_dir):
        print(f">> setup_tft.py: TFT_eSPI not yet installed at {dst_dir}")
        print(">>   Run 'pio pkg install' then rebuild to apply User_Setup.h")
        return

    shutil.copy2(src, dst)
    print(f">> setup_tft.py: Copied User_Setup.h -> {dst}")

# Try immediately (works if libdeps already installed)
copy_user_setup()

# Also hook into compile phases in case timing differs
env.AddPreAction("compileasm", copy_user_setup)
env.AddPreAction("compile", copy_user_setup)
