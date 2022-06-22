FROM node:14
RUN apt-get update
RUN apt-get install -y build-essential libbluetooth-dev
ADD . node-bluetooth-serial-port
WORKDIR node-bluetooth-serial-port
RUN npm install --unsafe-perm
