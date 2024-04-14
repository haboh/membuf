CONFIG_MODULE_SIG=n
CONFIG_MODULE_SIG_ALL=n

obj-m += membuf.o

all:
	make -C /usr/src/kernels/6.5.6-300.fc39.x86_64 M=$(PWD) modules

clean:
	make -C /usr/src/kernels/6.5.6-300.fc39.x86_64 M=$(PWD) clean
