#!/usr/bin/python

import os
from multiprocessing import Pool
import datetime
from shutil import copyfile

drives = list("cdfghij")
cams = [str(i) for i in xrange(16)]

output_base_path = "/mnt/c/sensei-data/"

now = datetime.datetime.now().isoformat().replace(':','-')

def make_output_dirs():
    for cam in cams:
        dst_path = os.path.join(output_base_path, now, cam)
        
        if not os.path.exists(dst_path):
            os.makedirs(dst_path)
            
def copy(from_drive):
    for cam in cams:
        src_path = os.path.join("/mnt", from_drive, "test", cam)
        dst_path = os.path.join(output_base_path, now, cam)
        
        print "[", from_drive, cam, "] copy", src_path, "->", dst_path
        
        for entry in os.listdir(src_path):
            src_file = os.path.join(src_path, entry)
            dst_file = os.path.join(dst_path, entry)
            copyfile(src_file, dst_file)

def clean(from_drive):
    for cam in cams:
        src_path = os.path.join("/mnt", from_drive, "test", cam)
        
        for entry in os.listdir(src_path):
            full_path_entry = os.path.join(src_path, entry)
            print "remove:", full_path_entry
            os.remove(full_path_entry)

def drain(from_drive):
    copy(from_drive)
    clean(from_drive)

print "Creating output directories..."
make_output_dirs()

pool = Pool(8)
pool.map(drain, drives)

print "Done"
