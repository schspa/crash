#!/usr/bin/env python

import sys,os
import re

from optparse import OptionParser

parser = OptionParser()
parser.add_option("-f", "--infile",
                  dest="inputfile",
                  help="input log contains regs log")
parser.add_option("-o", "--outfile",
                  dest="outputfile",
                  help="write output to here")


class arm_pt_regs:
    def __init__(self, cpu):
        self.cpu = cpu
        self.regs = {} ##{'PC': 0x1345667, 'SP': 0x123414}

    def set(self, key, value):
        self.regs[key] = value

    def __str__(self):
        ret_str = '## ========= begin arm64 core regs =========\n'
        ret_str += '{}=0x{:d}\n'.format('cpu', self.cpu)
        for item in sorted(self.regs.keys()):
            ret_str += '{}=0x{:016x}\n'.format(item, self.regs[item])
        ret_str += '## ========= end arm64 core regs =========\n'
        return ret_str

    def is_valid_regs(self):
        if self.regs.has_key('pc') and self.regs.has_key('sp'):
            return True
        return False


def parse_pc_lr(msg, regs):
    pc_lr_pattern = re.compile(r"(lr|pc)( :\s*)([\[\<]+)([a-fA-F0-9]+)([\]\>]+)([^$]*)")
    #print('=============== searching for :' + msg + ' ============\n')
    searchObj=re.search(pc_lr_pattern, msg)
    if searchObj != None:
        groups = searchObj.groups()
        regs.set(groups[0], int(groups[3], 16))
        #print(groups[5])
        searchObj=re.search(pc_lr_pattern, groups[5])
        if searchObj != None:
            return parse_pc_lr(groups[5], regs)
        else:
            return

def parse_sp_regs(msg, regs):
    ##pc_lr_pattern = re.compile(r"(\s*)([0-9xsp]+)(\s*:\s*)([a-fA-F0-9]{8,})([^$]*)")
    pc_lr_pattern = re.compile(r"(?:\s*)([xspt]+[0-9xsptae]+)(?:\s*:\s*)([a-fA-F0-9]{8,})([^$]*)")
    #print('=============== searching for :' + msg + ' ============\n')
    searchObj=re.search(pc_lr_pattern, msg)
    if searchObj != None:
        groups = searchObj.groups()
        #print(groups)
        regs.set(groups[0], int(groups[1], 16))
        ##print('get ' + groups[0] + ': '+ groups[1])
        searchObj=re.search(pc_lr_pattern, groups[2])
        if searchObj != None:
            if len(groups) > 2:
                #print('********* searching for left log ************\n')
                return parse_sp_regs(groups[2], regs)
        else:
            return

def get_cpu_number(msg):
    ## "%sCPU: %d PID: %d Comm: %.20s %s %s %.*s\n" at kernel/printk/printk.c
    cpu_pattern = re.compile(r"(CPU:\s)([0-9]+)(\s*)PID:")
    searchObj=re.search(cpu_pattern,msg)
    if searchObj != None:
        groups = searchObj.groups()
        #regs.set('CPU', int(groups[1], 16))
        #print("find CPU:" + groups[1])
        return int(groups[1])
    return None;

def parse_regs_log(logfile, outputfile):
    #print ('input:' + logfile + ' outfile:' + str(outputfile))

    regs = [] ##arm_pt_regs();
    reg = None;
    with open(logfile, 'r') as f:
        for line in f.readlines():
            #outputfile.write(line)
            str_strip = line.strip()
            ret = get_cpu_number(str_strip)
            if ret != None:
                if reg != None and reg.is_valid_regs():
                    regs.append(reg)
                    reg = None
                reg = arm_pt_regs(ret);

            if reg != None:
                parse_pc_lr(line.strip(), reg)
                parse_sp_regs(line.strip(), reg)
    if reg != None and reg.is_valid_regs():
        regs.append(reg)

    for reg in regs:
        outputfile.write(str(reg))

if __name__ == '__main__':
    (options, args) = parser.parse_args()

    if options.inputfile != None:
        outfd = None;
        if options.outputfile != None:
            outfd = open(options.outputfile, 'w')
        if outfd == None:
            outfd = sys.stdout
        parse_regs_log(options.inputfile, outfd)

