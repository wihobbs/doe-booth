FROM fluxrm/flux-core:fedora40-arm64

COPY Makefile /home/fluxuser/Makefile
COPY build-mpibind.sh /home/fluxuser/build-mpibind.sh
COPY vcpu.c /home/fluxuser/vcpu.c
COPY hello.c /home/fluxuser/hello.c
RUN sudo chown fluxuser /home/fluxuser/*
RUN sudo chmod a+rwx /home/fluxuser/* 

WORKDIR /home/fluxuser
RUN git clone https://github.com/bcumming/affinity.git
RUN CC=gcc CXX=g++ cmake ./affinity/. && make -j 4

ENTRYPOINT ["/usr/bin/flux", "start", "-s4", "/bin/bash"]
