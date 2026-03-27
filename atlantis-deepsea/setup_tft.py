Import("env")
import os, shutil

def copy_user_setup(*args, **kwargs):
    src = os.path.join(env["PROJECT_DIR"], "include", "User_Setup.h")
    dst = os.path.join(env["PROJECT_LIBDEPS_DIR"],
                       env["PIOENV"], "TFT_eSPI", "User_Setup.h")
    if os.path.isdir(os.path.dirname(dst)):
        shutil.copy2(src, dst)
        print(">> Copied User_Setup.h into TFT_eSPI library")

env.AddPreAction("compileasm", copy_user_setup)
env.AddPreAction("compile", copy_user_setup)
