$script = <<SCRIPT
sudo apt-get install -y build-essential
sudo apt-get install git-core g++ -y
sudo apt-get install libbluetooth-dev -y

# curl -sL https://deb.nodesource.com/setup_0.12 | sudo -E bash -
# curl -sL https://deb.nodesource.com/setup_0.10 | sudo -E bash -
curl -sL https://deb.nodesource.com/setup_4.x | sudo -E bash -

sudo apt-get install -y nodejs
sudo npm install -g node-gyp
SCRIPT

Vagrant.configure("2") do |config|
	config.vm.box = "ubuntu/trusty64"
	config.vm.box_check_update = false
	config.vm.network "forwarded_port", guest: 8124, host: 8024

	config.vm.hostname = "inotify.local"

	config.vm.provider "virtualbox" do |vb|
		vb.gui = false
		vb.memory = 1024
		vb.cpus = 1
	end

	config.vm.provision "shell", inline: $script
end
