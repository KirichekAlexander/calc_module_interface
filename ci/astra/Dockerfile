FROM debian:buster

ENV DEBIAN_FRONTEND=noninteractive

# buster в archive.debian.org
RUN rm -f /etc/apt/sources.list.d/*.list || true \
 && printf "deb [trusted=yes] http://archive.debian.org/debian buster main contrib non-free\n\
deb [trusted=yes] http://archive.debian.org/debian-security buster/updates main contrib non-free\n" > /etc/apt/sources.list \
 && printf 'Acquire::Check-Valid-Until "false";\nAcquire::AllowInsecureRepositories "true";\nAPT::Get::AllowUnauthenticated "true";\n' > /etc/apt/apt.conf.d/99archive \
 && apt-get -o Acquire::Check-Valid-Until=false update

# инструменты + Qt5 + Charts + упаковка .deb
RUN apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config \
    file dpkg-dev \
    qtbase5-dev qttools5-dev qttools5-dev-tools \
    libqt5charts5-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
