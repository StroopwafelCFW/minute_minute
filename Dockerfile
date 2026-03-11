FROM ghcr.io/wiiu-env/devkitppc:20260225

RUN dkp-pacman -Syyu --noconfirm \
    && pacman -S --noconfirm python python-pycryptodome \
    && dkp-pacman -Scc --noconfirm

WORKDIR /project

CMD ["make"]
