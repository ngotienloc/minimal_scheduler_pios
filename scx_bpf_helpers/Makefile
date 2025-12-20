obj-m := scx_bpf_helpers.o
KDIR ?= /lib/modules/6.12.47+rpt-rpi-2712/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
