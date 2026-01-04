PROJ := sx1278

obj-m := $(PROJ).o
$(PROJ)-objs := lora.o spi-sx1278.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
        $(MAKE) -C $(KDIR) M=$(PWD) modules

install:
        sudo $(MAKE) -C $(KDIR) M=$(PWD) modules_install
        sudo depmod -a

uninstall:
        sudo modprobe -r $(PROJ)
        sudo rm -f /lib/modules/$(shell uname -r)/extra/$(PROJ).ko*
        sudo depmod -a

test:
        $(MAKE) install
        echo
        ls -l /dev/$(PROJ)*
        $(MAKE) uninstall

clean:
        rm -f *.o *.ko *.mod *.mod.c *.mod.o Module.symvers modules.order