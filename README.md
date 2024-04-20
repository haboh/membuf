# membuf


### DKMS INSTALLATION

Load module sources

1. ``` sudo dkms ldtarball membuf.tar.gz ```

Check that module loaded

2. ``` sudo dkms status ```

Build module

3. ``` sudo dkms build -m membuf -v 0.1 ```

Install module

4. ``` sudo dkms install -m membuf -v 0.1 ```

Load module into kernel

5. ``` sudo modprobe membuf ```

Check that module was successfully loaded 

6. ``` sudo lsmod ```

To unload module

7. ``` sudo modprobe -r membuf ```


#### DIRECT INSTALLATION

1. To build: ```make```

2. To install: ```sudo insmod membuf.ko [buf_size=n]```

3. To uninstall: ```sudo rmmod membuf```


### Usage

`/dev/membuf_device` &mdash; character device for read / writing

`/sys/kernel/kobject_membuf/buf_size` &mdash; sysfs file for changing buffer size
