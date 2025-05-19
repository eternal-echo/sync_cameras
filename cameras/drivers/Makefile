KERNEL_SRC ?= /lib/modules/`uname -r`/build

obj-m   +=   virtual_v4l2.o


modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules;

modules-install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install;

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean;
	rm -f *.ko; 