"""
REMOTE CONTROL - MCF
Usage:
    mcf -h
    mcf -c [-a ADDR] [-p PORT]
    mcf -v [-a ADDR] [-p PORT]
    mcf -r [-f FILE]

Flags:
    -h  print help
    -c  print info on components and ports
    -v  start remote viewer
    -r  read and view a recording

Options:
    -a ADDR  target ip-address  [default: 127.0.0.1]
    -p PORT  target port        [default: 6666]
    -f FILE  recording file     [default: record.bin]

Copyright (c) 2024 Accenture
"""

from docopt import docopt
import os.path
from remote_viewer import *
from record_viewer import *

import mcf_python_path.mcf_paths
from mcf import RemoteControl

def check_arg():
    args = docopt(__doc__)
    ip_cli   = args['-a']
    port_cli = args['-p']

    if args['-c']:
        rc = RemoteControl()
        rc.connect(ip_cli, port_cli)
        connection_info = rc.get_info()
        for r_index, r in enumerate(connection_info):
            ports_list = r["ports"]
            print(" -({})- {}:".format(r_index, r["name"]))
            for port in ports_list:
                print("     -- {:<40} ({}, connected = {})".format(
                    port["topic"], port["direction"], port["connected"]
                ))

    if args['-r']:
        in_file = args['-f']
        print("looking for record file = [{}]".format(in_file))
        if os.path.exists(in_file):
            reader_cli = RecordReader()
            reader_cli.open(in_file)
            reader_cli.index()

            window_cli = tk.Tk()
            window_cli.title("MCF Record Viewer")
            window_cli.geometry("800x600")
            build_window(window_cli, reader_cli)
            window_cli.mainloop()

            reader_cli.close()
        else:
            print("ERROR - No such record file: [{}]".format(in_file))

    if args['-v']:
        rc = RemoteControl()
        if not rc.connect(ip_cli, port_cli):
            exit()

        window_cli = tk.Tk()
        window_cli.title("MCF Remote Viewer")
        window_cli.geometry("800x600")

        app_state_cli = AppModel(rc)
        ComponentView(app_state_cli, window_cli)
        DetailsView(app_state_cli, window_cli)

        app_state_cli.reload()
        window_cli.mainloop()

        rc.disconnect()


if __name__ == "__main__":
    check_arg()
