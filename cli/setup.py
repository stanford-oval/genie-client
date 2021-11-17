import setuptools
from subprocess import run

setuptools.setup(
    name="genie-client-cpp-cli",
    author="Stanford OVAL",
    author_email="thingpedia-admins@lists.stanford.edu",
    description=(
        "Command Line Interface (CLI) for development and deployment "
        "of Genie Client CPP"
    ),
    url="https://github.com/stanford-oval/almond-cloud",
    packages=setuptools.find_packages(),
    python_requires=">=3.8,<4",
    install_requires=[
        "splatlog>=0.1.0",
        "clavier>=0.1.2",
        # Fails to build in arm32v7 container :/
        # "paramiko>=2.7.2",
    ],
    scripts=[
        "bin/genie-client-cpp",
    ],
)
