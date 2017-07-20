KERNELDIR = /lib/modules/`uname -r`/build

obj-m += hwfq.o mmq.o example.o

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
