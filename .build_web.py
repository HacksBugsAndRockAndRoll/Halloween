from shutil import copyfile
import os
import sys
import fileinput
def build_web():
    # Read in the file
    with open('web/index.html', 'r') as file:
        filedata = file.read()
    # Replace the target string
    filedata = filedata.replace('"', '\\"')
    filedata = filedata.replace('\n', '')
    # Write the file out again
    with open('include/index.h', 'w') as file:
        file.write('const char INDEX_HTML[] = {"' + filedata + '"};')

build_web()