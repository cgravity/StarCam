#!python2

import os
from multiprocessing import Pool
import datetime
from shutil import copyfile

drives = list("CDFGHIJ")
cams = [str(i) for i in xrange(16)]

output_base_path = "C:\\sensei-data\\"

now = datetime.datetime.now().isoformat().replace(':','-')

def copy(from_drive):
    for cam in cams:
        src_path = os.path.join(from_drive, "test", cam)
        dst_path = os.path.join(output_base_path, now, cam)
        
        if not os.path.exists(dst_path)
            os.makedirs(dst_path)
        
        for entry in os.listdir(src_path):
            src_file = os.path.join(src_path, entry)
            dst_file = os.path.join(dst_path, entry)
            copyfile(src, dst)

def clean(from_drive):
    for cam in cams:
        src_path = os.path.join(from_drive, "test", cam)
        
        for entry in os.listdir(src_path):
            os.remove(entry)

def drain(from_drive):
    copy(from_drive)
    clean(from_drive)

pool = Pool(8)
p.map(drain, drives)

