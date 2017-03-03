sudo apt-get update
sudo apt-get install -y libssl-dev
sudo apt-get install -y tightvncserver
sudo apt-get install -y xrdp
sudo apt-get install -y samba

cd ~/Downloads
git clone https://github.com/eclipse/paho.mqtt.c.git
cd ~/Downloads/paho.mqtt.c
make && sudo make install && sudo ldconfig

cd ~/Downloads
git clone https://github.com/esnet/iperf.git
cd ~/Downloads/iperf
./configure && make && sudo make install && sudo ldconfig

