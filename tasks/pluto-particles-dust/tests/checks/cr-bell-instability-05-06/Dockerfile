# Local-source CPU incumbent recipe for grouped Bell 05+06; no source fetch.
FROM debian:bookworm-slim@sha256:1caf1c703c8f7e15dcf2e7769b35000c764e6f50e4d7401c355fb0248f3ddfdb
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev make python3 python3-numpy \
 && rm -rf /var/lib/apt/lists/*
COPY code/pluto /opt/pluto
COPY tests/checks/cr-bell-instability-05-06/build /app/meta
COPY tests/checks/cr-bell-instability-05-06/run.sh /app/run.sh
WORKDIR /app/build
RUN mkdir -p /app/build && \
  for cfg in 05 06; do \
    cp -a /opt/pluto /tmp/pluto-${cfg} && \
    cp /app/meta/sciaccel.defs /tmp/pluto-${cfg}/Config/sciaccel.defs && \
    mkdir -p /app/build/${cfg} && cd /app/build/${cfg} && \
    cp /tmp/pluto-${cfg}/Test_Problems/Particles/CR/Bell_Instability/init.c . && \
    cp /tmp/pluto-${cfg}/Test_Problems/Particles/CR/Bell_Instability/particles_init.c . && \
    cp /tmp/pluto-${cfg}/Test_Problems/Particles/CR/Bell_Instability/userdef_output.c . && \
    cp /tmp/pluto-${cfg}/Test_Problems/Particles/CR/Bell_Instability/definitions_${cfg}.h definitions.h && \
    cp /tmp/pluto-${cfg}/Test_Problems/Particles/CR/Bell_Instability/pluto_${cfg}.ini pluto.ini && \
    cp /app/meta/deck.py . && \
    if [ "${cfg}" = 05 ]; then python3 deck.py pluto.ini 1.26 0.126 --particles; else python3 deck.py pluto.ini 2.1 0.21 --particles; fi && \
    printf 'ARCH         = sciaccel.defs\\n' > makefile && \
    cp /app/meta/sciaccel.defs . && \
    PLUTO_DIR=/tmp/pluto-${cfg} python3 /tmp/pluto-${cfg}/setup.py --auto-update && make -j"$(nproc)" && \
    cp pluto /app/build/pluto-${cfg} && cp pluto.ini /app/build/pluto_${cfg}.ini; \
  done
ENTRYPOINT ["/app/run.sh"]
