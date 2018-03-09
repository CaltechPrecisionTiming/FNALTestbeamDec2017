#! /usr/bin/env python

import os, sys, commands, time, glob

#look for the current directory
#######################################
pwd = os.environ['PWD']
home = os.environ['HOME']
#######################################
RUN_DIR = pwd
TEMP = pwd
EXE  = "scripts/processDRS_Run.sh"
OUT  = "/store/user/cmstestbeam/ETL/MT6Section1Data/122017/OTSDAQ/"
TARGET = "default"
QUEUE = ""

def write_jdl(jdlfile,run,dfile,rcfile,ofile,lfile,efile):
    fjdl = open(jdlfile,'w')
    fjdl.write('universe = vanilla \n')
    fjdl.write('executable = '+EXE+" \n")
    fjdl.write('getenv = True \n')
    fjdl.write('Arguments = ');
    fjdl.write(run+' ')
    fjdl.write(ofile+" \n")
    fjdl.write('output = '+lfile+" \n")
    fjdl.write('Error = '+efile+" \n")
    fjdl.write('Transfer_Input_Files = '+dfile+", "+rcfile+", config/December2017_LGADOnly.config, dat2rootPixels, calib/calib.tar \n")
    fjdl.write('Transfer_Output_Files = "" \n')
    fjdl.write('queue \n')
    fjdl.close()

if __name__ == "__main__":
    if not len(sys.argv) > 1 or '-h' in sys.argv or '--help' in sys.argv:
        print "Usage: %s [-q queue] [-list lists/list.list]" % sys.argv[0]
        print
        sys.exit(1)

    argv_pos = 1
  
    if '-q' in sys.argv:
        p = sys.argv.index('-q')
        QUEUE = sys.argv[p+1]
        argv_pos += 2
    if '-list' in sys.argv:
        p = sys.argv.index('-list')
        TARGET = sys.argv[p+1]
        argv_pos += 2

    # input sample list
    listfile = "lists/"+TARGET+".list"
        
    # create and organize output folders
    ROOT = OUT+"/"+TARGET+"/"
    TARGET  = RUN_DIR+"/"+TARGET+"/"
    jdldir  = TARGET+"jdl/"
    logdir  = TARGET+"log/"
    errdir  = TARGET+"err/"
    condir  = TARGET+"config/"

    # make output folders
    os.system("rm -rf "+TARGET)
    os.system("mkdir -p "+TARGET)
    os.system("mkdir -p "+logdir)
    os.system("mkdir -p "+jdldir)
    os.system("mkdir -p "+errdir)
    os.system("mkdir -p "+condir)
    os.system("rm -rf /eos/uscms"+ROOT)
    os.system("mkdir -p /eos/uscms"+ROOT)

    with open(listfile,'r') as f:
        inputlist = f.readlines()
        for line in inputlist:
            line = line.split()
            run_number = line[0]
            os.system('eos ls root://cmseos.fnal.gov//store/user/cmstestbeam/ETL/MT6Section1Data/122017/OTSDAQ/CMSTiming/ | grep RawDataSaver0CMSVMETiming_Run'+run_number+" > "+condir+"Run"+run_number+".list")
            rclist = glob.glob('run_config/*'+run_number+"*")
            if len(rclist) > 0:
                os.system('cp '+rclist[0]+" "+condir+"Run"+run_number+".config")
            else :
                os.system('cp run_config/JO_config_empty.txt '+condir+"Run"+run_number+".config")
            write_jdl(jdldir+"Run"+run_number+".jdl", run_number, condir+"Run"+run_number+".list", condir+"Run"+run_number+".config", ROOT+"Run"+run_number+".root", logdir+"Run"+run_number+".log", errdir+"Run"+run_number+".stderr")
            os.system('condor_submit '+jdldir+"Run"+run_number+".jdl")
    
