Vagrant.configure(2) do |config|

  config.vm.provider "virtualbox" do |v|
    v.memory = 4096
    v.cpus = 2
  end

  config.vm.box = 'ubuntu/disco64'

  # synced folder
  config.vm.synced_folder '.', '/xv6'

  # disable default synced folder
  config.vm.synced_folder '.', '/vagrant', disabled: true

  # install packages
  config.vm.provision 'shell', inline: <<-EOS
    apt-get -y update
    apt-get install -y \
      python \
      git \
      build-essential \
      gdb-multiarch \
      qemu-system-misc \
      gcc-riscv64-linux-gnu \
      binutils-riscv64-linux-gnu
  EOS

end
