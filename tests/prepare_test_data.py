#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# (c) Copyright 2018 CERN for the benefit of the LHCb Collaboration           #
#                                                                             #
# This software is distributed under the terms of the Apache version 2        #
# licence, copied verbatim in the file "COPYING".                             #
#                                                                             #
# In applying this licence, CERN does not waive the privileges and immunities #
# granted to it by virtue of its status as an Intergovernmental Organization  #
# or submit itself to any jurisdiction.                                       #
###############################################################################
from __future__ import print_function
'''
Script to prepare test data files and directories.
'''

import sys
import os
import logging
from datetime import datetime
from os.path import join, isdir, dirname, exists
from shutil import copytree, rmtree, copy

EPOCH = datetime(1970, 1, 1)


def call(*args, **kwargs):
    from subprocess import check_output, STDOUT, CalledProcessError
    logging.debug('call(*%r, **%r)', args, kwargs)
    kwargs['stderr'] = STDOUT
    try:
        out = check_output(*args, **kwargs).rstrip()
        if out:
            logging.debug('OUTPUT:\n%s', out)
    except CalledProcessError as err:
        logging.error('%s failed with exit code %s', err.cmd, err.returncode)
        if err.output:
            logging.error('OUTPUT:\n%s', err.output)
        sys.exit(err.returncode)


def makedirs(*args):
    '''
    Create requested directories, if needed.
    '''
    for path in args:
        if not isdir(path):
            os.makedirs(path)


def to_ts(dt):
    return int((dt - EPOCH).total_seconds() * 1e9)


def write_IOVs(iovs, path):
    '''
    write a list of (datetime, key) pairs to the IOVs file in path.
    '''
    with open(join(path, 'IOVs'), 'w') as IOVs:
        IOVs.write(''.join(
            '{0} {1}\n'.format(to_ts(dt), key) for dt, key in iovs))


def lhcb_conddb_case(path):
    # initialize repository from template (tag 'v0')
    src_data = join(dirname(__file__), 'data', 'test_repo')
    logging.debug('copying initial data from %s', src_data)
    copytree(src_data, path)

    call(['git', 'init', path])
    call(['git', 'config', '-f', '.git/config', 'user.name', 'Test User'],
         cwd=path)
    call([
        'git', 'config', '-f', '.git/config', 'user.email',
        'test.user@no.where'
    ],
         cwd=path)
    call(['git', 'add', '.'], cwd=path)
    env = dict(os.environ)
    env['GIT_COMMITTER_DATE'] = env['GIT_AUTHOR_DATE'] = '1483225200'
    call(['git', 'commit', '-m', 'initial version'], cwd=path, env=env)
    call(['git', 'tag', 'v0'], cwd=path)

    # change values for tag 'v1'
    with open(join(src_data, 'values.xml')) as in_file:
        with open(join(path, 'values.xml'), 'w') as out_file:
            out_file.write(in_file.read().replace('42', '2016'))

    # modify changing.xml dir to use partitioning
    dirInit = join(path, 'changing.xml', 'initial')
    dir2016 = join(path, 'changing.xml', '2016')
    dir2017 = join(path, 'changing.xml', '2017')

    makedirs(dirInit, dir2016, dir2017)
    write_IOVs([(EPOCH, 'initial'), (datetime(2016, 1, 1), 2016),
                (datetime(2017, 1, 1), 2017)], join(path, 'changing.xml'))

    copy(join(path, 'changing.xml', 'v0.xml'), join(dirInit, 'v0'))
    write_IOVs([(EPOCH, 'v0')], dirInit)

    copy(join(path, 'changing.xml', 'v1.xml'), join(dir2016, 'v1'))
    write_IOVs([(EPOCH, '../initial/v0'), (datetime(2016, 7, 1), 'v1')],
               dir2016)

    write_IOVs([(EPOCH, '../2016/v1')], dir2017)

    os.remove(join(path, 'changing.xml', 'v0.xml'))
    os.remove(join(path, 'changing.xml', 'v1.xml'))

    call(['git', 'add', '--all', 'changing.xml'], cwd=path)
    call(['git', 'commit', '-am', 'v1 data'], cwd=path)
    call(['git', 'tag', 'v1'], cwd=path)

    # changes for HEAD version (no tag)
    with open(join(src_data, 'values.xml')) as in_file:
        with open(join(path, 'values.xml'), 'w') as out_file:
            out_file.write(in_file.read().replace('42', '0'))
    call(['git', 'commit', '-am', 'new data'], cwd=path)

    # changes for local files
    with open(join(src_data, 'values.xml')) as in_file:
        with open(join(path, 'values.xml'), 'w') as out_file:
            out_file.write(in_file.read().replace('42', '-123'))

    # make a "bare" clone of the repository
    if exists(path + '-bare.git'):
        rmtree(path + '-bare.git')
    call(['git', 'clone', '--mirror', path, path + '-bare.git'])

    # prepare an overlay directory
    if exists(path + '-overlay'):
        rmtree(path + '-overlay')
    makedirs(path + '-overlay')
    with open(join(src_data, 'values.xml')) as in_file:
        with open(join(path + '-overlay', 'values.xml'), 'w') as out_file:
            out_file.write(in_file.read().replace('42', '777'))
    call(['git', 'init'], cwd=path + '-overlay')


def create_mini_repo(path):
    if exists(path):
        rmtree(path)
    env = dict(os.environ)

    call(['git', 'init', path])
    call(['git', 'config', '-f', '.git/config', 'user.name', 'Test User'],
         cwd=path)
    call([
        'git', 'config', '-f', '.git/config', 'user.email',
        'test.user@no.where'
    ],
         cwd=path)

    makedirs(join(path, 'TheDir'))
    with open(join(path, 'TheDir', 'TheFile.txt'), 'w') as f:
        f.write('some data\n')

    makedirs(join(path, 'Cond', 'group'))
    with open(join(path, 'Cond', 'IOVs'), 'w') as f:
        f.write('0 v0\n100 group\n')
    with open(join(path, 'Cond', 'group', 'IOVs'), 'w') as f:
        f.write('50 ../v1\n')
    with open(join(path, 'Cond', 'v0'), 'w') as f:
        f.write('data 0')
    with open(join(path, 'Cond', 'v1'), 'w') as f:
        f.write('data 1')

    call(['git', 'add', '.'], cwd=path)
    env['GIT_COMMITTER_DATE'] = env['GIT_AUTHOR_DATE'] = '1483225100'
    call(['git', 'commit', '-m', 'message 1'], cwd=path, env=env)
    call(['git', 'tag', 'v0'], cwd=path, env=env)

    with open(join(path, 'Cond', 'IOVs'), 'w') as f:
        f.write('0 v0\n100 group\n200 v3\n')
    with open(join(path, 'Cond', 'group', 'IOVs'), 'w') as f:
        f.write('50 ../v1\n150 ../v2\n')
    with open(join(path, 'Cond', 'v2'), 'w') as f:
        f.write('data 2')
    with open(join(path, 'Cond', 'v3'), 'w') as f:
        f.write('data 3')

    call(['git', 'add', '.'], cwd=path)
    env['GIT_COMMITTER_DATE'] = env['GIT_AUTHOR_DATE'] = '1483225200'
    call(['git', 'commit', '-m', 'message 2'], cwd=path, env=env)
    call(['git', 'tag', 'v1'], cwd=path, env=env)

    with open(join(path, 'TheDir', 'TheFile.txt'), 'w') as f:
        f.write('some uncommitted data\n')

    if exists(path + '.git'):
        rmtree(path + '.git')
    call(['git', 'clone', '--mirror', path, path + '.git'])


def write_json_files(path):
    from json import dump
    if not isdir(path):
        os.makedirs(path)
    with open(join(path, 'minimal.json'), 'w') as f:
        dump({}, f)
    with open(join(path, 'basic.json'), 'w') as f:
        dump({"TheDir": {"TheFile.txt": "some JSON (file) data\n"}}, f)


def main():
    level = (logging.DEBUG if
             ('--debug' in sys.argv
              or os.environ.get('VERBOSE')) else logging.WARNING)
    logging.basicConfig(level=level)

    if exists('test_data'):
        logging.debug('removing existing test_data')
        rmtree('test_data')

    lhcb_conddb_case(join('test_data', 'lhcb', 'repo'))

    create_mini_repo(join('test_data', 'repo'))

    write_json_files(join('test_data', 'json'))


if __name__ == '__main__':
    main()
