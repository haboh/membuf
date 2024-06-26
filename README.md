# membuf

### DKMS installation

0. Download latest release

1. Load module sources: ``` sudo dkms ldtarball membuf-0.1.tar.gz ```

2. Check that module loaded: ``` sudo dkms status ```

3. Build module: ``` sudo dkms build -m membuf -v 0.1 ```

4. Install module: ``` sudo dkms install -m membuf -v 0.1 ```

5. Load module into kernel: ``` sudo modprobe membuf ```

6. Check that module was successfully loaded: ``` sudo lsmod ```

7. To unload module: ``` sudo modprobe -r membuf ```


### Direct installation

1. To build: ```make```

2. To install: ```sudo insmod membuf.ko [buf_size=n]```

3. To uninstall: ```sudo rmmod membuf```


### Usage

`/dev/membuf` &mdash; character device for read / writing

`/sys/kernel/membuf/buf_size` &mdash; sysfs file for changing buffer size
