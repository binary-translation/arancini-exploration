FROM nixos/nix

RUN mkdir -p ~/.config/nix/
RUN echo "experimental-features = nix-command flakes" > ~/.config/nix/nix.conf

RUN nix upgrade-nix

RUN git clone https://github.com/binary-translation/arancini-exploration.git -v
WORKDIR arancini-exploration
RUN git checkout sr/bench

RUN nix build ./scripts\#phoenix.aarch64-linux --out-link phoenix-aarch64 --accept-flake-config
RUN nix build --accept-flake-config

COPY ./x86_closure.nar ./
RUN nix-store --import < ./x86_closure.nar

# Basic translations
RUN mkdir txlat-aarch64
RUN mkdir txlat-nodeadflags-aarch64
RUN mkdir txlat-nofencemerge-aarch64
# Nlib translations
RUN mkdir txlat-fast-aarch64
RUN mkdir txlat-nodeadflags-fast-aarch64
RUN mkdir txlat-nofencemerge-fast-aarch64

# Don't let them wait until libc is translated
SHELL ["nix", "develop", "--accept-flake-config", "--command", "bash", "-c"]
RUN result/bin/txlat -I test/phoenix/libc.so -O txlat-aarch64/libc.so
RUN result/bin/txlat --disable-flag-opt -I test/phoenix/libc.so -O txlat-nodeadflags-aarch64/libc.so
RUN result/bin/txlat --nlib general.mni.aarch64.long -I test/phoenix/libc.so -O txlat-fast-aarch64/libc.so
RUN result/bin/txlat --nlib general.mni.aarch64.long --disable-flag-opt -I test/phoenix/libc.so -O txlat-nodeadflags-fast-aarch64/libc.so

RUN result/bin/txlat -I test/phoenix/libunwind.so -O txlat-aarch64/libunwind.so
RUN result/bin/txlat --disable-flag-opt -I test/phoenix/libunwind.so -O txlat-nodeadflags-aarch64/libunwind.so
RUN result/bin/txlat --nlib general.mni.aarch64.long -I test/phoenix/libunwind.so -O txlat-fast-aarch64/libunwind.so
RUN result/bin/txlat --nlib general.mni.aarch64.long --disable-flag-opt -I test/phoenix/libunwind.so -O txlat-nodeadflags-fast-aarch64/libunwind.so

SHELL ["/bin/sh", "-c"]
CMD nix develop --accept-flake-config
