FROM ghcr.io/wiiu-env/devkitppc:20260225

RUN dkp-pacman -Syyu --noconfirm \
    && dkp-pacman -S --noconfirm devkitARM \
    && apt-get update && apt-get install -y python3 python3-pip \
    && rm -rf /usr/lib/python3.11/EXTERNALLY-MANAGED \
    && pip3 install pycryptodome \
    && dkp-pacman -Scc --noconfirm

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITARM=${DEVKITPRO}/devkitARM
ENV PATH=${DEVKITPRO}/tools/bin:${DEVKITARM}/bin:${PATH}

WORKDIR /project

CMD ["make"]
