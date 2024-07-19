# syntax=docker/dockerfile:1
FROM ubuntu:20.04

ARG INSTALLDIR=/mcf/deps/install
ARG ROOTWORKDIR=/mcf

WORKDIR $ROOTWORKDIR

# Prevent hanging while installing tzdata
ENV TZ=Europe/Berlin
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y \
    git \
    g++ \
    pkg-config \
    wget \
    make \
    libtool \
    autoconf \
    automake \
    python3.8 \
    python3.8-dev \
    python3.8-distutils \
    python3.8-venv \
    vim \
    libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

# Install pip
RUN wget https://bootstrap.pypa.io/get-pip.py \
    -q -O /tmp/get-pip.py \
    && python3.8 /tmp/get-pip.py \
    && rm /tmp/get-pip.py

RUN ln -fs /usr/bin/python3.8 /usr/bin/python
RUN python -m pip install --upgrade pip

# Setup virtual env and allow users to install packages within the docker image
ENV VIRTUAL_ENV=/home/venv
RUN python3 -m venv $VIRTUAL_ENV
RUN chmod -R a+rw $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

## Create install directory where all manually installed dependencies will be installed
RUN mkdir -p $INSTALLDIR

# Install cmake v3.19
RUN wget https://github.com/Kitware/CMake/releases/download/v3.19.0/cmake-3.19.0-Linux-x86_64.sh \
      -q -O /tmp/cmake-install.sh \
      && chmod u+x /tmp/cmake-install.sh \
      && mkdir -p $ROOTWORKDIR/deps/cmake \
      && /tmp/cmake-install.sh --skip-license --prefix=$ROOTWORKDIR/deps/cmake \
      && rm /tmp/cmake-install.sh
ENV PATH="$ROOTWORKDIR/deps/cmake/bin:${PATH}"

# Install Boost v1.77
RUN cd /home && wget https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.bz2 \
  && tar --bzip2 -xf boost_1_77_0.tar.bz2 \
  && rm boost_1_77_0.tar.bz2 \
  && cd boost_1_77_0 \
  && ./bootstrap.sh --prefix=$ROOTWORKDIR/deps/boost --with-libraries=program_options \
  && ./b2 install \
  && cd /home \
  && rm -rf boost_1_77_0
ENV PATH="$ROOTWORKDIR/deps/boost/bin:${PATH}"

# Install cmake package dependencies
COPY install_deps.sh .
RUN chmod +x "${ROOTWORKDIR}/install_deps.sh"
RUN bash -c "${ROOTWORKDIR}/install_deps.sh deps/install 6"
ENV PATH="$ROOTWORKDIR/deps/install:${PATH}"

# Install mcf python dependencies
COPY mcf_py/requirements.txt /tmp
RUN pip install -r /tmp/requirements.txt

# Install types generator python dependencies
COPY mcf_tools/types_generator/requirements.txt /tmp
RUN pip install -r /tmp/requirements.txt

RUN mkdir -p /home/mcf
WORKDIR /home/mcf
