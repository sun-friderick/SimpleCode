#! /usr/bin/env python

import subprocess
import os

from oclintscripts import path

def git_hash():
    os.chdir(path.root_dir)
    git_hash = subprocess.check_output(['git', 'log', '-n', '1', '--pretty=%h']).split()[0]
    return git_hash

def oclint_version():
    return "0.8.1"

def oclint_dev_version():
    return oclint_version() + '.dev.' + git_hash()

def llvm_branches():
    return ['trunk', 'branches/release_35', 'tags/RELEASE_350/final', 'branches/release_34', 'tags/RELEASE_34/final', 'branches/release_33', 'tags/RELEASE_33/final', 'branches/release_32', 'tags/RELEASE_32/final']

def llvm_default_branch():
    return 'tags/RELEASE_350/final'
