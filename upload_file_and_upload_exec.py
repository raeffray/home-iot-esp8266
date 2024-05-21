import os
from os.path import join
from SCons.Script import DefaultEnvironment, Builder

env = DefaultEnvironment()

# Custom upload actions
def before_upload(source, target, env):
    print("Executing before_upload...")
    env.Execute("pio run --target buildfs -v")
    env.Execute("pio run --target uploadfs -v")

def after_upload(source, target, env):
    print("Executing after_upload...")
    # Run the LittleFS upload after the firmware upload

# Add custom upload targets
env.AddPreAction("upload", before_upload)
env.AddPostAction("upload", after_upload)