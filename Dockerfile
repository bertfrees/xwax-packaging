FROM debian:stretch
RUN dpkg --add-architecture armhf
RUN echo 'deb http://ftp.debian.org/debian stretch-backports main' > /etc/apt/sources.list.d/backports.list
RUN apt-get update
RUN apt-get install -y gcc-arm-linux-gnueabihf
RUN apt-get install -y libasound2-dev:armhf \
                       libjack-dev:armhf \
                       libsdl-ttf2.0-dev:armhf
RUN apt-get install -t stretch-backports -y debhelper
RUN apt-get install -y build-essential
