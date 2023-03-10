FROM ubuntu:22.10
COPY . /app
WORKDIR "/app"
RUN apt-get update && apt-get -y install sudo
RUN sudo apt-get -y install gcc
RUN sudo apt-get -y install build-essential
RUN sudo apt-get install -y libgsl-dev
RUN sudo apt-get -y install ffmpeg
RUN make compile