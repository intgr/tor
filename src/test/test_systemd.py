#!/usr/bin/env python
"""Test script to launch tor as if triggered by systemd socket activation,
without actually using systemd. Should work on all reasonable
Unixes.

Also demonstrates how to bind sockets in advance and hand them off to Tor.

Currently doesn't test any failure conditions, nor is it a proper "unit test",
but better than nothing.
"""

import os
import sys
import tempfile
import socket
from socket import SOCK_DGRAM, SOCK_STREAM, AF_INET, AF_INET6
import shutil

#### CONFIG

config = [
    ('SocksPort',   AF_INET,  SOCK_STREAM, '127.0.0.1', 59800),
    ('SocksPort',   AF_INET6, SOCK_STREAM, '[::1]',     59800),
    ('ControlPort', AF_INET,  SOCK_STREAM, '127.0.0.1', 59801),
    ('ControlPort', AF_INET6, SOCK_STREAM, '[::1]',     59801),
    ('DNSPort',     AF_INET,  SOCK_DGRAM,  '127.0.0.1', 59802),
    ('DNSPort',     AF_INET6, SOCK_DGRAM,  '[::1]',     59802),
]
statedir = '/tmp/tor_test_systemd'

#### IMPL

def find_tor():
    # Try to find ../or/tor relative to this script first
    relpath = os.path.join(os.path.dirname(__file__), '..', 'or', 'tor')

    search = [relpath, '/usr/local/bin/tor', '/usr/bin/tor']

    for path in search:
        if os.path.isfile(path):
            print("Found tor at %s" % path)
            return path

    raise Exception("Cannot find tor")

# Globals
tor = find_tor()
is_child = False
torrc = os.path.join(statedir, 'torrc')

def init():
    if not os.path.isdir(statedir):
        os.mkdir(statedir)

    with open(torrc, 'w') as f:
        f.write('DataDirectory %s\n' % statedir)

def exec_tor(config):
    """Executed as forked child"""

    socks = []
    cmdline = ['tor', '-f', torrc]

    for opt, family, socktype, host, port in config:
        sock = socket.socket(family, socktype)
        socks.append(sock)

        if family == AF_INET6:
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, True)

        sock.bind((host.strip('[]'), port))
        if socktype != SOCK_DGRAM:
            sock.listen(128)

        cmdline.extend(['--%s' % opt, '%s:%d' % (host, port)])

    # First file descriptor must be 3
    assert socks[0].fileno() == 3

    os.putenv('LISTEN_PID', str(os.getpid()))
    os.putenv('LISTEN_FDS', str(len(socks)))

    print("Running %s" % (' '.join(cmdline)))
    os.execv(tor, cmdline)

    assert False, "Huh, exec returned?"

def spawn(config):
    global is_child

    pid = os.fork()
    is_child = (pid == 0)

    if is_child:
        try:
            exec_tor(config)
        except:
            import traceback
            traceback.print_exc()

        print("Launching tor failed")
        os._exit(1)

    else:
        print("Child forked as %d, waiting" % pid)
        pid, status = os.waitpid(pid, os.P_WAIT)
        print("Child exited with status %d" % status)

def main():
    init()
    spawn(config)

if __name__=='__main__':
    main()
