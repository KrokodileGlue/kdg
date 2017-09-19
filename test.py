#!/usr/bin/env python

from subprocess import call
import sys
import re

failedno = 0

def parse_test(lines, i):
    j = i
    line = lines[j]

    while line != 'END_TEST':
        line = lines[j]
        j += 1

    return j - 1

def parse_case(lines, i):
    i += 1
    j = i
    line = lines[j]

    while line != 'END_CASE':
        line = lines[j]
        j += 1

    return j - 1

def parse_subject(case):
    i = 0

    while case[i] != 'EXPECT_OUTPUT':
        i += 1

    return case[1:i]

def parse_output(case):
    i = 0

    while case[i] != 'EXPECT_OUTPUT':
        i += 1
    i += 1

    return case[i:]

def run_test(test, case, testno, caseno, lineno):
    subject = parse_subject(case)
    output = parse_output(case)
    global failedno

    call(['./test', '\n'.join(subject), '\n'.join(test)])
    poutput = open('output.txt').read().split('\n')
    poutput = [i.rstrip() for i in poutput]
    poutput = '\n'.join(poutput)

    if '\n'.join(output) + '\n' != poutput:
        print 'test #' + str(testno) + ' case #' + str(caseno) + ' failed at line ' + str(lineno) + ':'
        print 'expected: '
        print '\n'.join(output) + '\n'
        print 'recieved: '
        print poutput
        failedno += 1
    else:
        print 'test #' + str(testno) + ' case #' + str(caseno) + ' passed.'

lines = open('test.txt').read().split('\n')
i = 0
testno = 0

while True:
    if i >= len(lines):
        break

    if re.match("#", lines[i]) or lines[i] != 'BEGIN_TEST':
        i += 1
        continue

    testno += 1
    i += 1

    j = parse_test(lines, i)
    test = lines[i:j]
    i = j + 1

    caseno = 0

    while True:
        if i >= len(lines):
            break

        if re.match("#", lines[i]) or lines[i] == '':
            i += 1
            continue

        if lines[i] != 'BEGIN_CASE':
            break

        j = parse_case(lines, i)
        case = lines[i:j]

        caseno += 1

        run_test(test, case, testno, caseno, i + 1)
        i = j + 2

sys.stdout.write('ran ' + str(testno) + ' tests, ' + str(testno - failedno) + ' succeeded')
if failedno != 0:
    sys.stdout.write(' - ' + str(failedno) + ' failed')
sys.stdout.write('.\n')
