# membuf

#### Build
```
make
```

### Usage

```
sudo insmod membuf.ko [buf_size=n]
```

```
sudo rmmod membuf
```

`/dev/membuf_device` --- character device for read / writing

`/sys/kernel/kobject_membuf/buf_size` --- sysfs file for changing buffer size
