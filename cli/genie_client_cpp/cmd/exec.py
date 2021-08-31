import socket
import shlex
from typing import List
from select import select

from clavier import log as logging
from clavier.err import UserError
import paramiko

LOG = logging.getLogger(__name__)

def add_to(subparsers):
    parser = subparsers.add_parser(
        "exec",
        target=run,
        help="Run a command on the remote host",
    )

    parser.add_argument(
        "host",
        help="Domain name or IP address of target",
    )

    parser.add_argument(
        "cmd",
        nargs="...",
        help="command to execute"
    )

def run(
    host: str,
    cmd: List[str],
    user: str ="root",
    password: str = None,
    port: int = 22
):
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        client.connect(
            hostname=host,
            username=user,
            port=port,
            password=password,
            timeout=10,
            allow_agent=False,
            look_for_keys=False
        )
    except paramiko.SSHException as error:
        if password is None:
            client.get_transport().auth_none(user)
        else:
            raise error

    stdin, stdout, stderr = client.exec_command(shlex.join(cmd))

    reading = True
    while reading:
        read, write, execute = select((stdout, stderr), (), ())


    for line in stdout:
        print('> ' + line.strip('\n'))

    for line in stderr:
        print("! " + line.strip('\n'))

    client.close()

